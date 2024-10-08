/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "runtime_intrinsics.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "dex/invoke_type.h"
#include "intrinsics_enum.h"
#include "intrinsics_list.h"
#include "mirror/class.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art HIDDEN {

namespace {

ArtMethod* FindIntrinsicMethod(Thread* self,
                               const char* class_name,
                               const char* method_name,
                               const char* signature)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  PointerSize image_size = class_linker->GetImagePointerSize();
  ObjPtr<mirror::Class> cls = class_linker->FindSystemClass(self, class_name);
  if (cls == nullptr) {
    LOG(FATAL) << "Could not find class of intrinsic " << class_name;
  }

  ArtMethod* method = cls->FindClassMethod(method_name, signature, image_size);
  if (method == nullptr || method->GetDeclaringClass() != cls) {
    LOG(FATAL) << "Could not find method of intrinsic "
               << class_name << " " << method_name << " " << signature;
  }
  return method;
}

// Initialize an intrinsic. Returns true if the intrinsic is already
// initialized, false otherwise.
bool InitializeIntrinsic(Thread* self,
                         Intrinsics intrinsic,
                         InvokeType invoke_type,
                         const char* class_name,
                         const char* method_name,
                         const char* signature)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* method = FindIntrinsicMethod(self, class_name, method_name, signature);

  CHECK_EQ(method->GetInvokeType(), invoke_type);
  if (method->IsIntrinsic()) {
    CHECK_EQ(method->GetIntrinsic(), intrinsic);
    return true;
  } else {
    method->SetIntrinsic(intrinsic);
    return false;
  }
}

// Returns true if the intrinsic is already initialized, false otherwise.
bool IsIntrinsicInitialized(Thread* self,
                            Intrinsics intrinsic,
                            InvokeType invoke_type,
                            const char* class_name,
                            const char* method_name,
                            const char* signature)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* method = FindIntrinsicMethod(self, class_name, method_name, signature);

  CHECK_EQ(method->GetInvokeType(), invoke_type);
  if (method->IsIntrinsic()) {
    CHECK_EQ(method->GetIntrinsic(), intrinsic);
    return true;
  } else {
    return false;
  }
}

bool AreAllIntrinsicsInitialized() REQUIRES_SHARED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
#define IS_INTRINSIC_INITIALIZED(Name, InvokeType, _, __, ___, ClassName, MethodName, Signature) \
  IsIntrinsicInitialized(self,                                                                   \
                         Intrinsics::k##Name,                                                    \
                         InvokeType,                                                             \
                         ClassName,                                                              \
                         MethodName,                                                             \
                         Signature) &&
  bool result = ART_INTRINSICS_LIST(IS_INTRINSIC_INITIALIZED) true;
#undef IS_INTRINSIC_INITIALIZED
  return result;
}

}  // namespace

void InitializeIntrinsics() {
  Thread* self = Thread::Current();
  // Initialization here uses the short-circuit operator || to stop
  // initializing if there's an already initialized intrinsic.
#define INITIALIZE_INTRINSIC(Name, InvokeType, _, __, ___, ClassName, MethodName, Signature) \
  InitializeIntrinsic(self,                                                                  \
                      Intrinsics::k##Name,                                                   \
                      InvokeType,                                                            \
                      ClassName,                                                             \
                      MethodName,                                                            \
                      Signature) ||
  ART_INTRINSICS_LIST(INITIALIZE_INTRINSIC) true;
#undef INITIALIZE_INTRINSIC
  DCHECK(AreAllIntrinsicsInitialized());
}

}  // namespace art
