/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "calling_convention_arm64.h"

#include <android-base/logging.h>

#include "arch/arm64/jni_frame_arm64.h"
#include "arch/instruction_set.h"
#include "utils/arm64/managed_register_arm64.h"

namespace art HIDDEN {
namespace arm64 {

static constexpr ManagedRegister kXArgumentRegisters[] = {
    Arm64ManagedRegister::FromXRegister(X0),
    Arm64ManagedRegister::FromXRegister(X1),
    Arm64ManagedRegister::FromXRegister(X2),
    Arm64ManagedRegister::FromXRegister(X3),
    Arm64ManagedRegister::FromXRegister(X4),
    Arm64ManagedRegister::FromXRegister(X5),
    Arm64ManagedRegister::FromXRegister(X6),
    Arm64ManagedRegister::FromXRegister(X7),
};
static_assert(kMaxIntLikeRegisterArguments == arraysize(kXArgumentRegisters));

static const DRegister kDArgumentRegisters[] = {
  D0, D1, D2, D3, D4, D5, D6, D7
};
static_assert(kMaxFloatOrDoubleRegisterArguments == arraysize(kDArgumentRegisters));

static const SRegister kSArgumentRegisters[] = {
  S0, S1, S2, S3, S4, S5, S6, S7
};
static_assert(kMaxFloatOrDoubleRegisterArguments == arraysize(kSArgumentRegisters));

static constexpr ManagedRegister kCalleeSaveRegisters[] = {
    // Core registers.
    // Note: The native jni function may call to some VM runtime functions which may suspend
    // or trigger GC. And the jni method frame will become top quick frame in those cases.
    // So we need to satisfy GC to save LR and callee-save registers which is similar to
    // CalleeSaveMethod(RefOnly) frame.
    // Jni function is the native function which the java code wants to call.
    // Jni method is the method that is compiled by jni compiler.
    // Call chain: managed code(java) --> jni method --> jni function.
    // This does not apply to the @CriticalNative.

    // Thread register(X19) is saved on stack.
    Arm64ManagedRegister::FromXRegister(X19),
    Arm64ManagedRegister::FromXRegister(X20),  // Note: Marking register.
    Arm64ManagedRegister::FromXRegister(X21),  // Note: Suspend check register.
    Arm64ManagedRegister::FromXRegister(X22),
    Arm64ManagedRegister::FromXRegister(X23),
    Arm64ManagedRegister::FromXRegister(X24),
    Arm64ManagedRegister::FromXRegister(X25),
    Arm64ManagedRegister::FromXRegister(X26),
    Arm64ManagedRegister::FromXRegister(X27),
    Arm64ManagedRegister::FromXRegister(X28),
    Arm64ManagedRegister::FromXRegister(X29),
    Arm64ManagedRegister::FromXRegister(LR),
    // Hard float registers.
    // Considering the case, java_method_1 --> jni method --> jni function --> java_method_2,
    // we may break on java_method_2 and we still need to find out the values of DEX registers
    // in java_method_1. So all callee-saves(in managed code) need to be saved.
    Arm64ManagedRegister::FromDRegister(D8),
    Arm64ManagedRegister::FromDRegister(D9),
    Arm64ManagedRegister::FromDRegister(D10),
    Arm64ManagedRegister::FromDRegister(D11),
    Arm64ManagedRegister::FromDRegister(D12),
    Arm64ManagedRegister::FromDRegister(D13),
    Arm64ManagedRegister::FromDRegister(D14),
    Arm64ManagedRegister::FromDRegister(D15),
};

template <size_t size>
static constexpr uint32_t CalculateCoreCalleeSpillMask(
    const ManagedRegister (&callee_saves)[size]) {
  uint32_t result = 0u;
  for (auto&& r : callee_saves) {
    if (r.AsArm64().IsXRegister()) {
      result |= (1u << r.AsArm64().AsXRegister());
    }
  }
  return result;
}

template <size_t size>
static constexpr uint32_t CalculateFpCalleeSpillMask(const ManagedRegister (&callee_saves)[size]) {
  uint32_t result = 0u;
  for (auto&& r : callee_saves) {
    if (r.AsArm64().IsDRegister()) {
      result |= (1u << r.AsArm64().AsDRegister());
    }
  }
  return result;
}

static constexpr uint32_t kCoreCalleeSpillMask = CalculateCoreCalleeSpillMask(kCalleeSaveRegisters);
static constexpr uint32_t kFpCalleeSpillMask = CalculateFpCalleeSpillMask(kCalleeSaveRegisters);

static constexpr ManagedRegister kAapcs64CalleeSaveRegisters[] = {
    // Core registers.
    Arm64ManagedRegister::FromXRegister(X19),
    Arm64ManagedRegister::FromXRegister(X20),
    Arm64ManagedRegister::FromXRegister(X21),
    Arm64ManagedRegister::FromXRegister(X22),
    Arm64ManagedRegister::FromXRegister(X23),
    Arm64ManagedRegister::FromXRegister(X24),
    Arm64ManagedRegister::FromXRegister(X25),
    Arm64ManagedRegister::FromXRegister(X26),
    Arm64ManagedRegister::FromXRegister(X27),
    Arm64ManagedRegister::FromXRegister(X28),
    Arm64ManagedRegister::FromXRegister(X29),
    Arm64ManagedRegister::FromXRegister(LR),
    // Hard float registers.
    Arm64ManagedRegister::FromDRegister(D8),
    Arm64ManagedRegister::FromDRegister(D9),
    Arm64ManagedRegister::FromDRegister(D10),
    Arm64ManagedRegister::FromDRegister(D11),
    Arm64ManagedRegister::FromDRegister(D12),
    Arm64ManagedRegister::FromDRegister(D13),
    Arm64ManagedRegister::FromDRegister(D14),
    Arm64ManagedRegister::FromDRegister(D15),
};

static constexpr uint32_t kAapcs64CoreCalleeSpillMask =
    CalculateCoreCalleeSpillMask(kAapcs64CalleeSaveRegisters);
static constexpr uint32_t kAapcs64FpCalleeSpillMask =
    CalculateFpCalleeSpillMask(kAapcs64CalleeSaveRegisters);

// Calling convention
static ManagedRegister ReturnRegisterForShorty(std::string_view shorty) {
  if (shorty[0] == 'F') {
    return Arm64ManagedRegister::FromSRegister(S0);
  } else if (shorty[0] == 'D') {
    return Arm64ManagedRegister::FromDRegister(D0);
  } else if (shorty[0] == 'J') {
    return Arm64ManagedRegister::FromXRegister(X0);
  } else if (shorty[0] == 'V') {
    return Arm64ManagedRegister::NoRegister();
  } else {
    return Arm64ManagedRegister::FromWRegister(W0);
  }
}

ManagedRegister Arm64ManagedRuntimeCallingConvention::ReturnRegister() const {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister Arm64JniCallingConvention::ReturnRegister() const {
  return ReturnRegisterForShorty(GetShorty());
}

ManagedRegister Arm64JniCallingConvention::IntReturnRegister() const {
  return Arm64ManagedRegister::FromWRegister(W0);
}

// Managed runtime calling convention

ManagedRegister Arm64ManagedRuntimeCallingConvention::MethodRegister() {
  return Arm64ManagedRegister::FromXRegister(X0);
}

ManagedRegister Arm64ManagedRuntimeCallingConvention::ArgumentRegisterForMethodExitHook() {
  return Arm64ManagedRegister::FromXRegister(X4);
}

bool Arm64ManagedRuntimeCallingConvention::IsCurrentParamInRegister() {
  if (IsCurrentParamAFloatOrDouble()) {
    return itr_float_and_doubles_ < kMaxFloatOrDoubleRegisterArguments;
  } else {
    size_t non_fp_arg_number = itr_args_ - itr_float_and_doubles_;
    return /* method */ 1u + non_fp_arg_number < kMaxIntLikeRegisterArguments;
  }
}

bool Arm64ManagedRuntimeCallingConvention::IsCurrentParamOnStack() {
  return !IsCurrentParamInRegister();
}

ManagedRegister Arm64ManagedRuntimeCallingConvention::CurrentParamRegister() {
  DCHECK(IsCurrentParamInRegister());
  if (IsCurrentParamAFloatOrDouble()) {
    if (IsCurrentParamADouble()) {
      return Arm64ManagedRegister::FromDRegister(kDArgumentRegisters[itr_float_and_doubles_]);
    } else {
      return Arm64ManagedRegister::FromSRegister(kSArgumentRegisters[itr_float_and_doubles_]);
    }
  } else {
    size_t non_fp_arg_number = itr_args_ - itr_float_and_doubles_;
    ManagedRegister x_reg = kXArgumentRegisters[/* method */ 1u + non_fp_arg_number];
    if (IsCurrentParamALong()) {
      return x_reg;
    } else {
      return Arm64ManagedRegister::FromWRegister(x_reg.AsArm64().AsOverlappingWRegister());
    }
  }
}

FrameOffset Arm64ManagedRuntimeCallingConvention::CurrentParamStackOffset() {
  return FrameOffset(displacement_.Int32Value() +  // displacement
                     kFramePointerSize +  // Method ref
                     (itr_slots_ * sizeof(uint32_t)));  // offset into in args
}

// JNI calling convention

Arm64JniCallingConvention::Arm64JniCallingConvention(bool is_static,
                                                     bool is_synchronized,
                                                     bool is_fast_native,
                                                     bool is_critical_native,
                                                     std::string_view shorty)
    : JniCallingConvention(is_static,
                           is_synchronized,
                           is_fast_native,
                           is_critical_native,
                           shorty,
                           kArm64PointerSize) {
}

uint32_t Arm64JniCallingConvention::CoreSpillMask() const {
  return is_critical_native_ ? 0u : kCoreCalleeSpillMask;
}

uint32_t Arm64JniCallingConvention::FpSpillMask() const {
  return is_critical_native_ ? 0u : kFpCalleeSpillMask;
}

ArrayRef<const ManagedRegister> Arm64JniCallingConvention::CalleeSaveScratchRegisters() const {
  DCHECK(!IsCriticalNative());
  // Use X22-X29 from native callee saves.
  constexpr size_t kStart = 3u;
  constexpr size_t kLength = 8u;
  static_assert(kAapcs64CalleeSaveRegisters[kStart].Equals(
                    Arm64ManagedRegister::FromXRegister(X22)));
  static_assert(kAapcs64CalleeSaveRegisters[kStart + kLength - 1u].Equals(
                    Arm64ManagedRegister::FromXRegister(X29)));
  static_assert((kAapcs64CoreCalleeSpillMask & ~kCoreCalleeSpillMask) == 0u);
  return ArrayRef<const ManagedRegister>(kAapcs64CalleeSaveRegisters).SubArray(kStart, kLength);
}

ArrayRef<const ManagedRegister> Arm64JniCallingConvention::ArgumentScratchRegisters() const {
  DCHECK(!IsCriticalNative());
  ArrayRef<const ManagedRegister> scratch_regs(kXArgumentRegisters);
  // Exclude return register (X0) even if unused. Using the same scratch registers helps
  // making more JNI stubs identical for better reuse, such as deduplicating them in oat files.
  static_assert(kXArgumentRegisters[0].Equals(Arm64ManagedRegister::FromXRegister(X0)));
  scratch_regs = scratch_regs.SubArray(/*pos=*/ 1u);
  DCHECK(std::none_of(scratch_regs.begin(),
                      scratch_regs.end(),
                      [return_reg = ReturnRegister().AsArm64()](ManagedRegister reg) {
                        return return_reg.Overlaps(reg.AsArm64());
                      }));
  return scratch_regs;
}

size_t Arm64JniCallingConvention::FrameSize() const {
  if (is_critical_native_) {
    CHECK(!SpillsMethod());
    CHECK(!HasLocalReferenceSegmentState());
    return 0u;  // There is no managed frame for @CriticalNative.
  }

  // Method*, callee save area size, local reference segment state
  DCHECK(SpillsMethod());
  size_t method_ptr_size = static_cast<size_t>(kFramePointerSize);
  size_t callee_save_area_size = CalleeSaveRegisters().size() * kFramePointerSize;
  size_t total_size = method_ptr_size + callee_save_area_size;

  DCHECK(HasLocalReferenceSegmentState());
  // Cookie is saved in one of the spilled registers.

  return RoundUp(total_size, kStackAlignment);
}

size_t Arm64JniCallingConvention::OutFrameSize() const {
  // Count param args, including JNIEnv* and jclass*.
  size_t all_args = NumberOfExtraArgumentsForJni() + NumArgs();
  size_t num_fp_args = NumFloatOrDoubleArgs();
  DCHECK_GE(all_args, num_fp_args);
  size_t num_non_fp_args = all_args - num_fp_args;
  // The size of outgoing arguments.
  size_t size = GetNativeOutArgsSize(num_fp_args, num_non_fp_args);

  // @CriticalNative can use tail call as all managed callee saves are preserved by AAPCS64.
  static_assert((kCoreCalleeSpillMask & ~kAapcs64CoreCalleeSpillMask) == 0u);
  static_assert((kFpCalleeSpillMask & ~kAapcs64FpCalleeSpillMask) == 0u);

  // For @CriticalNative, we can make a tail call if there are no stack args and
  // we do not need to extend the result. Otherwise, add space for return PC.
  if (is_critical_native_ && (size != 0u || RequiresSmallResultTypeExtension())) {
    size += kFramePointerSize;  // We need to spill LR with the args.
  }
  size_t out_args_size = RoundUp(size, kAapcs64StackAlignment);
  if (UNLIKELY(IsCriticalNative())) {
    DCHECK_EQ(out_args_size, GetCriticalNativeStubFrameSize(GetShorty()));
  }
  return out_args_size;
}

ArrayRef<const ManagedRegister> Arm64JniCallingConvention::CalleeSaveRegisters() const {
  if (UNLIKELY(IsCriticalNative())) {
    if (UseTailCall()) {
      return ArrayRef<const ManagedRegister>();  // Do not spill anything.
    } else {
      // Spill LR with out args.
      static_assert((kCoreCalleeSpillMask >> LR) == 1u);  // Contains LR as the highest bit.
      constexpr size_t lr_index = POPCOUNT(kCoreCalleeSpillMask) - 1u;
      static_assert(kCalleeSaveRegisters[lr_index].Equals(
                        Arm64ManagedRegister::FromXRegister(LR)));
      return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters).SubArray(
          /*pos=*/ lr_index, /*length=*/ 1u);
    }
  } else {
    return ArrayRef<const ManagedRegister>(kCalleeSaveRegisters);
  }
}

bool Arm64JniCallingConvention::IsCurrentParamInRegister() {
  if (IsCurrentParamAFloatOrDouble()) {
    return (itr_float_and_doubles_ < kMaxFloatOrDoubleRegisterArguments);
  } else {
    return ((itr_args_ - itr_float_and_doubles_) < kMaxIntLikeRegisterArguments);
  }
  // TODO: Can we just call CurrentParamRegister to figure this out?
}

bool Arm64JniCallingConvention::IsCurrentParamOnStack() {
  // Is this ever not the same for all the architectures?
  return !IsCurrentParamInRegister();
}

ManagedRegister Arm64JniCallingConvention::CurrentParamRegister() {
  CHECK(IsCurrentParamInRegister());
  if (IsCurrentParamAFloatOrDouble()) {
    CHECK_LT(itr_float_and_doubles_, kMaxFloatOrDoubleRegisterArguments);
    if (IsCurrentParamADouble()) {
      return Arm64ManagedRegister::FromDRegister(kDArgumentRegisters[itr_float_and_doubles_]);
    } else {
      return Arm64ManagedRegister::FromSRegister(kSArgumentRegisters[itr_float_and_doubles_]);
    }
  } else {
    int gp_reg = itr_args_ - itr_float_and_doubles_;
    CHECK_LT(static_cast<unsigned int>(gp_reg), kMaxIntLikeRegisterArguments);
    ManagedRegister x_reg = kXArgumentRegisters[gp_reg];
    if (IsCurrentParamALong() || IsCurrentParamAReference() || IsCurrentParamJniEnv())  {
      return x_reg;
    } else {
      return Arm64ManagedRegister::FromWRegister(x_reg.AsArm64().AsOverlappingWRegister());
    }
  }
}

FrameOffset Arm64JniCallingConvention::CurrentParamStackOffset() {
  CHECK(IsCurrentParamOnStack());
  size_t args_on_stack = itr_args_
                  - std::min(kMaxFloatOrDoubleRegisterArguments,
                             static_cast<size_t>(itr_float_and_doubles_))
                  - std::min(kMaxIntLikeRegisterArguments,
                             static_cast<size_t>(itr_args_ - itr_float_and_doubles_));
  size_t offset = displacement_.Int32Value() - OutFrameSize() + (args_on_stack * kFramePointerSize);
  CHECK_LT(offset, OutFrameSize());
  return FrameOffset(offset);
}

// X15 is neither managed callee-save, nor argument register. It is suitable for use as the
// locking argument for synchronized methods and hidden argument for @CriticalNative methods.
static void AssertX15IsNeitherCalleeSaveNorArgumentRegister() {
  // TODO: Change to static_assert; std::none_of should be constexpr since C++20.
  DCHECK(std::none_of(kCalleeSaveRegisters,
                      kCalleeSaveRegisters + std::size(kCalleeSaveRegisters),
                      [](ManagedRegister callee_save) constexpr {
                        return callee_save.Equals(Arm64ManagedRegister::FromXRegister(X15));
                      }));
  DCHECK(std::none_of(kXArgumentRegisters,
                      kXArgumentRegisters + std::size(kXArgumentRegisters),
                      [](ManagedRegister arg) { return arg.AsArm64().AsXRegister() == X15; }));
}

ManagedRegister Arm64JniCallingConvention::LockingArgumentRegister() const {
  DCHECK(!IsFastNative());
  DCHECK(!IsCriticalNative());
  DCHECK(IsSynchronized());
  AssertX15IsNeitherCalleeSaveNorArgumentRegister();
  return Arm64ManagedRegister::FromWRegister(W15);
}

ManagedRegister Arm64JniCallingConvention::HiddenArgumentRegister() const {
  DCHECK(IsCriticalNative());
  AssertX15IsNeitherCalleeSaveNorArgumentRegister();
  return Arm64ManagedRegister::FromXRegister(X15);
}

// Whether to use tail call (used only for @CriticalNative).
bool Arm64JniCallingConvention::UseTailCall() const {
  CHECK(IsCriticalNative());
  return OutFrameSize() == 0u;
}

}  // namespace arm64
}  // namespace art
