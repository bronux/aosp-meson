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

#ifndef ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_
#define ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_

#include <android-base/logging.h>

#include "arch/context.h"
#include "base/macros.h"
#include "registers_x86.h"

namespace art HIDDEN {
namespace x86 {

class X86Context final : public Context {
 public:
  X86Context() {
    Reset();
  }
  virtual ~X86Context() {}

  void Reset() override;

  void FillCalleeSaves(uint8_t* frame, const QuickMethodFrameInfo& fr) override;

  void SetSP(uintptr_t new_sp) override {
    SetGPR(ESP, new_sp);
  }

  void SetPC(uintptr_t new_pc) override {
    eip_ = new_pc;
  }

  void SetNterpDexPC(uintptr_t dex_pc_ptr) override {
    SetGPR(ESI, dex_pc_ptr);
  }

  void SetArg0(uintptr_t new_arg0_value) override {
    SetGPR(EAX, new_arg0_value);
  }

  bool IsAccessibleGPR(uint32_t reg) override {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    return gprs_[reg] != nullptr;
  }

  uintptr_t* GetGPRAddress(uint32_t reg) override {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    return gprs_[reg];
  }

  uintptr_t GetGPR(uint32_t reg) override {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfCpuRegisters));
    DCHECK(IsAccessibleGPR(reg));
    return *gprs_[reg];
  }

  void SetGPR(uint32_t reg, uintptr_t value) override;

  bool IsAccessibleFPR(uint32_t reg) override {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfFloatRegisters));
    return fprs_[reg] != nullptr;
  }

  uintptr_t GetFPR(uint32_t reg) override {
    DCHECK_LT(reg, static_cast<uint32_t>(kNumberOfFloatRegisters));
    DCHECK(IsAccessibleFPR(reg));
    return *fprs_[reg];
  }

  void SetFPR(uint32_t reg, uintptr_t value) override;

  void SmashCallerSaves() override;
  void CopyContextTo(uintptr_t* gprs, uintptr_t* fprs) override;

 private:
  // Pretend XMM registers are made of uin32_t pieces, because they are manipulated
  // in uint32_t chunks.
  enum {
    XMM0_0 = 0, XMM0_1,
    XMM1_0, XMM1_1,
    XMM2_0, XMM2_1,
    XMM3_0, XMM3_1,
    XMM4_0, XMM4_1,
    XMM5_0, XMM5_1,
    XMM6_0, XMM6_1,
    XMM7_0, XMM7_1,
    kNumberOfFloatRegisters};

  // Pointers to register locations. Values are initialized to null or the special registers below.
  uintptr_t* gprs_[kNumberOfCpuRegisters];
  uint32_t* fprs_[kNumberOfFloatRegisters];
  // Hold values for esp, eip and arg0 if they are not located within a stack frame. EIP is somewhat
  // special in that it cannot be encoded normally as a register operand to an instruction (except
  // in 64bit addressing modes).
  uintptr_t esp_, eip_, arg0_;
};
}  // namespace x86
}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_CONTEXT_X86_H_
