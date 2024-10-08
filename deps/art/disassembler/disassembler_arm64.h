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

#ifndef ART_DISASSEMBLER_DISASSEMBLER_ARM64_H_
#define ART_DISASSEMBLER_DISASSEMBLER_ARM64_H_

#include "disassembler.h"

// TODO(VIXL): Make VIXL compile cleanly with -Wshadow, -Wdeprecated-declarations.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "aarch64/decoder-aarch64.h"
#include "aarch64/disasm-aarch64.h"
#pragma GCC diagnostic pop

namespace art {
namespace arm64 {

class CustomDisassembler final : public vixl::aarch64::Disassembler {
 public:
  explicit CustomDisassembler(DisassemblerOptions* options)
      : vixl::aarch64::Disassembler(),
        read_literals_(options->can_read_literals_),
        base_address_(options->base_address_),
        end_address_(options->end_address_),
        options_(options) {
    if (!options->absolute_addresses_) {
      MapCodeAddress(0,
                     reinterpret_cast<const vixl::aarch64::Instruction*>(options->base_address_));
    }
  }

  // Use register aliases in the disassembly.
  void AppendRegisterNameToOutput(const vixl::aarch64::Instruction* instr,
                                  const vixl::aarch64::CPURegister& reg) override;

  // Overriding to print the address with trailing zeroes e.g. 0x00004074 instead of 0x4074.
  void AppendCodeRelativeAddressToOutput(const vixl::aarch64::Instruction* instr,
                                         const void* addr) override;

  // Intercepts the instruction flow captured by the parent method,
  // to specially instrument for particular instruction types.
  void Visit(vixl::aarch64::Metadata* metadata, const vixl::aarch64::Instruction* instr) override;

 private:
  // Improve the disassembly of literal load instructions.
  void VisitLoadLiteralInstr(const vixl::aarch64::Instruction* instr);

  // Improve the disassembly of thread offset.
  void VisitLoadStoreUnsignedOffsetInstr(const vixl::aarch64::Instruction* instr);

  // Improve the disassembly of branch to thunk jumping to pointer from thread entrypoint.
  void VisitUnconditionalBranchInstr(const vixl::aarch64::Instruction* instr);

  void AppendThreadOfsetName(const vixl::aarch64::Instruction* instr);

  // Indicate if the disassembler should read data loaded from literal pools.
  // This should only be enabled if reading the target of literal loads is safe.
  // Here are possible outputs when the option is on or off:
  // read_literals_ | disassembly
  //           true | 0x72681558: 1c000acb  ldr s11, pc+344 (addr 0x726816b0)
  //          false | 0x72681558: 1c000acb  ldr s11, pc+344 (addr 0x726816b0) (3.40282e+38)
  const bool read_literals_;

  // Valid address range: [base_address_, end_address_)
  const void* const base_address_;
  const void* const end_address_;

  DisassemblerOptions* options_;
};

class DisassemblerArm64 final : public Disassembler {
 public:
  explicit DisassemblerArm64(DisassemblerOptions* options) :
      Disassembler(options), disasm(options) {
    decoder.AppendVisitor(&disasm);
  }

  size_t Dump(std::ostream& os, const uint8_t* begin) override;
  void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) override;

 private:
  vixl::aarch64::Decoder decoder;
  CustomDisassembler disasm;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerArm64);
};

}  // namespace arm64
}  // namespace art

#endif  // ART_DISASSEMBLER_DISASSEMBLER_ARM64_H_
