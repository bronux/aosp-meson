/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_X86_64_JNI_FRAME_X86_64_H_
#define ART_RUNTIME_ARCH_X86_64_JNI_FRAME_X86_64_H_

#include <string.h>

#include "arch/instruction_set.h"
#include "base/bit_utils.h"
#include "base/globals.h"
#include "base/logging.h"
#include "base/macros.h"

namespace art HIDDEN {
namespace x86_64 {

constexpr size_t kFramePointerSize = static_cast<size_t>(PointerSize::k64);
static_assert(kX86_64PointerSize == PointerSize::k64, "Unexpected x86_64 pointer size");

static constexpr size_t kNativeStackAlignment = 16;
static_assert(kNativeStackAlignment == kStackAlignment);

// We always have to spill registers xmm12-xmm15 which are callee-save
// in managed ABI but caller-save in native ABI.
constexpr size_t kMmxSpillSize = 8u;
constexpr size_t kAlwaysSpilledMmxRegisters = 4;

// XMM0..XMM7 can be used to pass the first 8 floating args. The rest must go on the stack.
// -- Managed and JNI calling conventions.
constexpr size_t kMaxFloatOrDoubleRegisterArguments = 8u;
// Up to how many integer-like (pointers, objects, longs, int, short, bool, etc) args can be
// enregistered. The rest of the args must go on the stack.
// -- JNI calling convention only (Managed excludes RDI, so it's actually 5).
constexpr size_t kMaxIntLikeRegisterArguments = 6u;

// Get the size of the arguments for a native call.
inline size_t GetNativeOutArgsSize(size_t num_fp_args, size_t num_non_fp_args) {
  // Account for FP arguments passed through Xmm0..Xmm7.
  size_t num_stack_fp_args =
      num_fp_args - std::min(kMaxFloatOrDoubleRegisterArguments, num_fp_args);
  // Account for other (integer) arguments passed through GPR (RDI, RSI, RDX, RCX, R8, R9).
  size_t num_stack_non_fp_args =
      num_non_fp_args - std::min(kMaxIntLikeRegisterArguments, num_non_fp_args);
  static_assert(kFramePointerSize == kMmxSpillSize);
  return (num_stack_fp_args + num_stack_non_fp_args) * kFramePointerSize;
}

// Get stack args size for @CriticalNative method calls.
inline size_t GetCriticalNativeCallArgsSize(std::string_view shorty) {
  size_t num_fp_args =
      std::count_if(shorty.begin() + 1, shorty.end(), [](char c) { return c == 'F' || c == 'D'; });
  size_t num_non_fp_args = shorty.length() - 1u - num_fp_args;

  return GetNativeOutArgsSize(num_fp_args, num_non_fp_args);
}

// Get the frame size for @CriticalNative method stub.
// This must match the size of the frame emitted by the JNI compiler at the native call site.
inline size_t GetCriticalNativeStubFrameSize(std::string_view shorty) {
  // The size of outgoing arguments.
  size_t size = GetCriticalNativeCallArgsSize(shorty);

  // We always need to spill xmm12-xmm15 as they are managed callee-saves
  // but not native callee-saves.
  size += kAlwaysSpilledMmxRegisters * kMmxSpillSize;
  // Add return address size.
  size += kFramePointerSize;

  return RoundUp(size, kNativeStackAlignment);
}

// Get the frame size for direct call to a @CriticalNative method.
// This must match the size of the extra frame emitted by the compiler at the native call site.
inline size_t GetCriticalNativeDirectCallFrameSize(std::string_view shorty) {
  // The size of outgoing arguments.
  size_t size = GetCriticalNativeCallArgsSize(shorty);

  // No return PC to save, zero- and sign-extension are handled by the caller.
  return RoundUp(size, kNativeStackAlignment);
}

}  // namespace x86_64
}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_64_JNI_FRAME_X86_64_H_
