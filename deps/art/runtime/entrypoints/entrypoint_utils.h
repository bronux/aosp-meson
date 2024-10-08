/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
#define ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_

#include <jni.h>
#include <stdint.h>

#include "base/callee_save_type.h"
#include "base/locks.h"
#include "base/macros.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction.h"
#include "gc/allocator_type.h"
#include "handle.h"
#include "jvalue.h"

namespace art HIDDEN {

namespace mirror {
class Array;
class Class;
class MethodHandle;
class MethodType;
class Object;
class String;
}  // namespace mirror

class ArtField;
class ArtMethod;
class HandleScope;
enum InvokeType : uint32_t;
class MethodReference;
class OatQuickMethodHeader;
class ScopedObjectAccessAlreadyRunnable;
class Thread;

// Given the context of a calling Method, use its DexCache to resolve a type to a Class. If it
// cannot be resolved, throw an error. If it can, use it to create an instance.
template <bool kInstrumented = true>
ALWAYS_INLINE inline ObjPtr<mirror::Object> AllocObjectFromCode(ObjPtr<mirror::Class> klass,
                                                                Thread* self,
                                                                gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method and a resolved class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE
inline ObjPtr<mirror::Object> AllocObjectFromCodeResolved(ObjPtr<mirror::Class> klass,
                                                          Thread* self,
                                                          gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method and an initialized class, create an instance.
template <bool kInstrumented>
ALWAYS_INLINE
inline ObjPtr<mirror::Object> AllocObjectFromCodeInitialized(ObjPtr<mirror::Class> klass,
                                                             Thread* self,
                                                             gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);


ALWAYS_INLINE inline ObjPtr<mirror::Class> CheckArrayAlloc(dex::TypeIndex type_idx,
                                                           int32_t component_count,
                                                           ArtMethod* method,
                                                           bool* slow_path)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

// Given the context of a calling Method, use its DexCache to resolve a type to an array Class. If
// it cannot be resolved, throw an error. If it can, use it to create an array.
// When verification/compiler hasn't been able to verify access, optionally perform an access
// check.
template <bool kInstrumented = true>
ALWAYS_INLINE inline ObjPtr<mirror::Array> AllocArrayFromCode(dex::TypeIndex type_idx,
                                                              int32_t component_count,
                                                              ArtMethod* method,
                                                              Thread* self,
                                                              gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template <bool kInstrumented>
ALWAYS_INLINE
inline ObjPtr<mirror::Array> AllocArrayFromCodeResolved(ObjPtr<mirror::Class> klass,
                                                        int32_t component_count,
                                                        Thread* self,
                                                        gc::AllocatorType allocator_type)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

enum FindFieldFlags {
  InstanceBit = 1 << 0,
  StaticBit = 1 << 1,
  ObjectBit = 1 << 2,
  PrimitiveBit = 1 << 3,
  ReadBit = 1 << 4,
  WriteBit = 1 << 5,
};

// Type of find field operation for fast and slow case.
enum FindFieldType {
  InstanceObjectRead = InstanceBit | ObjectBit | ReadBit,
  InstanceObjectWrite = InstanceBit | ObjectBit | WriteBit,
  InstancePrimitiveRead = InstanceBit | PrimitiveBit | ReadBit,
  InstancePrimitiveWrite = InstanceBit | PrimitiveBit | WriteBit,
  StaticObjectRead = StaticBit | ObjectBit | ReadBit,
  StaticObjectWrite = StaticBit | ObjectBit | WriteBit,
  StaticPrimitiveRead = StaticBit | PrimitiveBit | ReadBit,
  StaticPrimitiveWrite = StaticBit | PrimitiveBit | WriteBit,
};

template<bool access_check>
inline ArtMethod* FindSuperMethodToCall(uint32_t method_idx,
                                        ArtMethod* resolved_method,
                                        ArtMethod* referrer,
                                        Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_);

template<FindFieldType type, bool access_check>
inline ArtField* FindFieldFromCode(uint32_t field_idx,
                                   ArtMethod* referrer,
                                   Thread* self,
                                   size_t expected_size)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template<InvokeType type>
inline ArtMethod* FindMethodToCall(Thread* self,
                                   ArtMethod* referrer,
                                   ObjPtr<mirror::Object>* this_object,
                                   const Instruction& inst,
                                   bool only_lookup_tls_cache,
                                   /*out*/ bool* string_init)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

inline ObjPtr<mirror::Class> ResolveVerifyAndClinit(dex::TypeIndex type_idx,
                                                    ArtMethod* referrer,
                                                    Thread* self,
                                                    bool can_run_clinit,
                                                    bool verify_access)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

ObjPtr<mirror::MethodHandle> ResolveMethodHandleFromCode(ArtMethod* referrer,
                                                         uint32_t method_handle_idx)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

ObjPtr<mirror::MethodType> ResolveMethodTypeFromCode(ArtMethod* referrer, dex::ProtoIndex proto_idx)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

void CheckReferenceResult(Handle<mirror::Object> o, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

JValue InvokeProxyInvocationHandler(ScopedObjectAccessAlreadyRunnable& soa,
                                    const char* shorty,
                                    jobject rcvr_jobj,
                                    jobject interface_art_method_jobj,
                                    std::vector<jvalue>& args)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

EXPORT bool FillArrayData(ObjPtr<mirror::Object> obj, const Instruction::ArrayDataPayload* payload)
    REQUIRES_SHARED(Locks::mutator_lock_)
    REQUIRES(!Roles::uninterruptible_);

template <typename INT_TYPE, typename FLOAT_TYPE>
inline INT_TYPE art_float_to_integral(FLOAT_TYPE f);

ArtMethod* GetCalleeSaveMethodCallerAndDexPc(ArtMethod** sp,
                                             CalleeSaveType type,
                                             /* out */ uint32_t* dex_pc,
                                             bool do_caller_check = false)
    REQUIRES_SHARED(Locks::mutator_lock_);

struct CallerAndOuterMethod {
  ArtMethod* caller;
  ArtMethod* outer_method;
};

CallerAndOuterMethod GetCalleeSaveMethodCallerAndOuterMethod(Thread* self, CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_);

ArtMethod* GetCalleeSaveOuterMethod(Thread* self, CalleeSaveType type)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Returns the synchronization object for a native method for a GenericJni frame
// we have just created or are about to exit. The synchronization object is
// the class object for static methods and the `this` object otherwise.
ObjPtr<mirror::Object> GetGenericJniSynchronizationObject(Thread* self, ArtMethod* called)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Update .bss method entrypoint if the `outer_method` has a valid OatFile, and either
//   A) the `callee_reference` has the same OatFile as `outer_method`, or
//   B) the `callee_reference` comes from a BCP DexFile that was present during `outer_method`'s
//      OatFile compilation.
// In both cases, we require that the oat file has a .bss entry for the `callee_reference`.
void MaybeUpdateBssMethodEntry(ArtMethod* callee,
                               MethodReference callee_reference,
                               ArtMethod* outer_method) REQUIRES_SHARED(Locks::mutator_lock_);

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_ENTRYPOINT_UTILS_H_
