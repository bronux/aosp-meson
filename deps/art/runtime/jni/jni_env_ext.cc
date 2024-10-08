/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jni_env_ext.h"

#include <algorithm>
#include <vector>

#include "android-base/stringprintf.h"

#include "base/mutex.h"
#include "base/to_str.h"
#include "check_jni.h"
#include "hidden_api.h"
#include "indirect_reference_table.h"
#include "java_vm_ext.h"
#include "jni_internal.h"
#include "lock_word.h"
#include "mirror/object-inl.h"
#include "nth_caller_visitor.h"
#include "scoped_thread_state_change.h"
#include "thread-current-inl.h"
#include "thread-inl.h"
#include "thread_list.h"

namespace art HIDDEN {

using android::base::StringPrintf;

static constexpr size_t kMonitorsInitial = 32;  // Arbitrary.
static constexpr size_t kMonitorsMax = 4096;  // Maximum number of monitors held by JNI code.

const JNINativeInterface* JNIEnvExt::table_override_ = nullptr;

jint JNIEnvExt::GetEnvHandler(JavaVMExt* vm, /*out*/void** env, jint version) {
  UNUSED(vm);
  // GetEnv always returns a JNIEnv* for the most current supported JNI version,
  // and unlike other calls that take a JNI version doesn't care if you supply
  // JNI_VERSION_1_1, which we don't otherwise support.
  if (JavaVMExt::IsBadJniVersion(version) && version != JNI_VERSION_1_1) {
    return JNI_EVERSION;
  }
  Thread* thread = Thread::Current();
  CHECK(thread != nullptr);
  *env = thread->GetJniEnv();
  return JNI_OK;
}

JNIEnvExt* JNIEnvExt::Create(Thread* self_in, JavaVMExt* vm_in, std::string* error_msg) {
  std::unique_ptr<JNIEnvExt> ret(new JNIEnvExt(self_in, vm_in));
  if (!ret->Initialize(error_msg)) {
    return nullptr;
  }
  return ret.release();
}

JNIEnvExt::JNIEnvExt(Thread* self_in, JavaVMExt* vm_in)
    : self_(self_in),
      vm_(vm_in),
      locals_(vm_in->IsCheckJniEnabled()),
      monitors_("monitors", kMonitorsInitial, kMonitorsMax),
      critical_(0),
      check_jni_(false),
      runtime_deleted_(false) {
  MutexLock mu(Thread::Current(), *Locks::jni_function_table_lock_);
  check_jni_ = vm_in->IsCheckJniEnabled();
  functions = GetFunctionTable(check_jni_);
  unchecked_functions_ = GetJniNativeInterface();
}

bool JNIEnvExt::Initialize(std::string* error_msg) {
  return locals_.Initialize(/*max_count=*/ 1u, error_msg);
}

void JNIEnvExt::SetFunctionsToRuntimeShutdownFunctions() {
  functions = GetRuntimeShutdownNativeInterface();
}

JNIEnvExt::~JNIEnvExt() {
}

jobject JNIEnvExt::NewLocalRef(mirror::Object* obj) {
  if (obj == nullptr) {
    return nullptr;
  }
  std::string error_msg;
  jobject ref = reinterpret_cast<jobject>(locals_.Add(obj, &error_msg));
  if (UNLIKELY(ref == nullptr)) {
    // This is really unexpected if we allow resizing LRTs...
    LOG(FATAL) << error_msg;
    UNREACHABLE();
  }
  return ref;
}

void JNIEnvExt::DeleteLocalRef(jobject obj) {
  if (obj != nullptr) {
    locals_.Remove(reinterpret_cast<IndirectRef>(obj));
  }
}

void JNIEnvExt::SetCheckJniEnabled(bool enabled) {
  check_jni_ = enabled;
  locals_.SetCheckJniEnabled(enabled);
  MutexLock mu(Thread::Current(), *Locks::jni_function_table_lock_);
  functions = GetFunctionTable(enabled);
  // Check whether this is a no-op because of override.
  if (enabled && JNIEnvExt::table_override_ != nullptr) {
    LOG(WARNING) << "Enabling CheckJNI after a JNIEnv function table override is not functional.";
  }
}

void JNIEnvExt::DumpReferenceTables(std::ostream& os) {
  locals_.Dump(os);
  monitors_.Dump(os);
}

void JNIEnvExt::PushFrame(int capacity) {
  DCHECK_GE(locals_.FreeCapacity(), static_cast<size_t>(capacity));
  stacked_local_ref_cookies_.push_back(PushLocalReferenceFrame());
}

void JNIEnvExt::PopFrame() {
  PopLocalReferenceFrame(stacked_local_ref_cookies_.back());
  stacked_local_ref_cookies_.pop_back();
}

// Note: the offset code is brittle, as we can't use OFFSETOF_MEMBER or offsetof easily. Thus, there
//       are tests in jni_internal_test to match the results against the actual values.

// This is encoding the knowledge of the structure and layout of JNIEnv fields.
static size_t JNIEnvSize(PointerSize pointer_size) {
  // A single pointer.
  return static_cast<size_t>(pointer_size);
}

inline MemberOffset JNIEnvExt::LocalReferenceTableOffset(PointerSize pointer_size) {
  return MemberOffset(JNIEnvSize(pointer_size) +
                      2 * static_cast<size_t>(pointer_size));  // Thread* self + JavaVMExt* vm
}

MemberOffset JNIEnvExt::LrtSegmentStateOffset(PointerSize pointer_size) {
  return MemberOffset(LocalReferenceTableOffset(pointer_size).SizeValue() +
                      jni::LocalReferenceTable::SegmentStateOffset().SizeValue());
}

MemberOffset JNIEnvExt::LrtPreviousStateOffset(PointerSize pointer_size) {
  return MemberOffset(LocalReferenceTableOffset(pointer_size).SizeValue() +
                      jni::LocalReferenceTable::PreviousStateOffset().SizeValue());
}

MemberOffset JNIEnvExt::SelfOffset(PointerSize pointer_size) {
  return MemberOffset(JNIEnvSize(pointer_size));
}

// Use some defining part of the caller's frame as the identifying mark for the JNI segment.
static uintptr_t GetJavaCallFrame(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) {
  NthCallerVisitor zeroth_caller(self, 0, false);
  zeroth_caller.WalkStack();
  if (zeroth_caller.caller == nullptr) {
    // No Java code, must be from pure native code.
    return 0;
  } else if (zeroth_caller.GetCurrentQuickFrame() == nullptr) {
    // Shadow frame = interpreter. Use the actual shadow frame's address.
    DCHECK(zeroth_caller.GetCurrentShadowFrame() != nullptr);
    return reinterpret_cast<uintptr_t>(zeroth_caller.GetCurrentShadowFrame());
  } else {
    // Quick frame = compiled code. Use the bottom of the frame.
    return reinterpret_cast<uintptr_t>(zeroth_caller.GetCurrentQuickFrame());
  }
}

void JNIEnvExt::RecordMonitorEnter(jobject obj) {
  locked_objects_.push_back(std::make_pair(GetJavaCallFrame(self_), obj));
}

static std::string ComputeMonitorDescription(Thread* self,
                                             jobject obj) REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Object> o = self->DecodeJObject(obj);
  if ((o->GetLockWord(false).GetState() == LockWord::kThinLocked) &&
      Locks::mutator_lock_->IsExclusiveHeld(self)) {
    // Getting the identity hashcode here would result in lock inflation and suspension of the
    // current thread, which isn't safe if this is the only runnable thread.
    return StringPrintf("<@addr=0x%" PRIxPTR "> (a %s)",
                        reinterpret_cast<intptr_t>(o.Ptr()),
                        o->PrettyTypeOf().c_str());
  } else {
    // IdentityHashCode can cause thread suspension, which would invalidate o if it moved. So
    // we get the pretty type before we call IdentityHashCode.
    const std::string pretty_type(o->PrettyTypeOf());
    return StringPrintf("<0x%08x> (a %s)", o->IdentityHashCode(), pretty_type.c_str());
  }
}

static void RemoveMonitors(Thread* self,
                           uintptr_t frame,
                           ReferenceTable* monitors,
                           std::vector<std::pair<uintptr_t, jobject>>* locked_objects)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  auto kept_end = std::remove_if(
      locked_objects->begin(),
      locked_objects->end(),
      [self, frame, monitors](const std::pair<uintptr_t, jobject>& pair)
          REQUIRES_SHARED(Locks::mutator_lock_) {
        if (frame == pair.first) {
          ObjPtr<mirror::Object> o = self->DecodeJObject(pair.second);
          monitors->Remove(o);
          return true;
        }
        return false;
      });
  locked_objects->erase(kept_end, locked_objects->end());
}

void JNIEnvExt::CheckMonitorRelease(jobject obj) {
  uintptr_t current_frame = GetJavaCallFrame(self_);
  std::pair<uintptr_t, jobject> exact_pair = std::make_pair(current_frame, obj);
  auto it = std::find(locked_objects_.begin(), locked_objects_.end(), exact_pair);
  bool will_abort = false;
  if (it != locked_objects_.end()) {
    locked_objects_.erase(it);
  } else {
    // Check whether this monitor was locked in another JNI "session."
    ObjPtr<mirror::Object> mirror_obj = self_->DecodeJObject(obj);
    for (std::pair<uintptr_t, jobject>& pair : locked_objects_) {
      if (self_->DecodeJObject(pair.second) == mirror_obj) {
        std::string monitor_descr = ComputeMonitorDescription(self_, pair.second);
        vm_->JniAbortF("<JNI MonitorExit>",
                      "Unlocking monitor that wasn't locked here: %s",
                      monitor_descr.c_str());
        will_abort = true;
        break;
      }
    }
  }

  // When we abort, also make sure that any locks from the current "session" are removed from
  // the monitors table, otherwise we may visit local objects in GC during abort (which won't be
  // valid anymore).
  if (will_abort) {
    RemoveMonitors(self_, current_frame, &monitors_, &locked_objects_);
  }
}

void JNIEnvExt::CheckNoHeldMonitors() {
  // The locked_objects_ are grouped by their stack frame component, as this enforces structured
  // locking, and the groups form a stack. So the current frame entries are at the end. Check
  // whether the vector is empty, and when there are elements, whether the last element belongs
  // to this call - this signals that there are unlocked monitors.
  if (!locked_objects_.empty()) {
    uintptr_t current_frame = GetJavaCallFrame(self_);
    std::pair<uintptr_t, jobject>& pair = locked_objects_[locked_objects_.size() - 1];
    if (pair.first == current_frame) {
      std::string monitor_descr = ComputeMonitorDescription(self_, pair.second);
      vm_->JniAbortF("<JNI End>",
                    "Still holding a locked object on JNI end: %s",
                    monitor_descr.c_str());
      // When we abort, also make sure that any locks from the current "session" are removed from
      // the monitors table, otherwise we may visit local objects in GC during abort.
      RemoveMonitors(self_, current_frame, &monitors_, &locked_objects_);
    } else if (kIsDebugBuild) {
      // Make sure there are really no other entries and our checking worked as expected.
      for (std::pair<uintptr_t, jobject>& check_pair : locked_objects_) {
        CHECK_NE(check_pair.first, current_frame);
      }
    }
  }
  // Ensure critical locks aren't held when returning to Java.
  if (critical_ > 0) {
    vm_->JniAbortF("<JNI End>",
                  "Critical lock held when returning to Java on thread %s",
                  ToStr<Thread>(*self_).c_str());
  }
}

void ThreadResetFunctionTable(Thread* thread, [[maybe_unused]] void* arg)
    REQUIRES(Locks::jni_function_table_lock_) {
  JNIEnvExt* env = thread->GetJniEnv();
  bool check_jni = env->IsCheckJniEnabled();
  env->functions = JNIEnvExt::GetFunctionTable(check_jni);
  env->unchecked_functions_ = GetJniNativeInterface();
}

void JNIEnvExt::SetTableOverride(const JNINativeInterface* table_override) {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  MutexLock mu2(Thread::Current(), *Locks::jni_function_table_lock_);

  JNIEnvExt::table_override_ = table_override;

  // See if we have a runtime. Note: we cannot run other code (like JavaVMExt's CheckJNI install
  // code), as we'd have to recursively lock the mutex.
  Runtime* runtime = Runtime::Current();
  if (runtime != nullptr) {
    runtime->GetThreadList()->ForEach(ThreadResetFunctionTable, nullptr);
    // Core Platform API checks rely on stack walking and classifying the caller. If a table
    // override is installed do not try to guess what semantics should be.
    runtime->SetCorePlatformApiEnforcementPolicy(hiddenapi::EnforcementPolicy::kDisabled);
  }
}

const JNINativeInterface* JNIEnvExt::GetFunctionTable(bool check_jni) {
  const JNINativeInterface* override = JNIEnvExt::table_override_;
  if (override != nullptr) {
    return override;
  }
  return check_jni ? GetCheckJniNativeInterface() : GetJniNativeInterface();
}

void JNIEnvExt::ResetFunctionTable() {
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  MutexLock mu2(Thread::Current(), *Locks::jni_function_table_lock_);
  Runtime* runtime = Runtime::Current();
  CHECK(runtime != nullptr);
  runtime->GetThreadList()->ForEach(ThreadResetFunctionTable, nullptr);
}

}  // namespace art
