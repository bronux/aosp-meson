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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_

#include "base/macros.h"
#include "nodes.h"
#include "optimization.h"
#include "optimizing_compiler_stats.h"

namespace art HIDDEN {

class CodeGenerator;

/**
 * Implements optimizations specific to each instruction.
 *
 * Note that graph simplifications producing a constant should be
 * implemented in art::HConstantFolding, while graph simplifications
 * not producing constants should be implemented in
 * art::InstructionSimplifier.  (This convention is a choice that was
 * made during the development of these parts of the compiler and is
 * not bound by any technical requirement.)
 */
class InstructionSimplifier : public HOptimization {
 public:
  InstructionSimplifier(HGraph* graph,
                        CodeGenerator* codegen,
                        OptimizingCompilerStats* stats = nullptr,
                        const char* name = kInstructionSimplifierPassName,
                        bool use_all_optimizations = false)
      : HOptimization(graph, name, stats),
        codegen_(codegen),
        use_all_optimizations_(use_all_optimizations) {}

  static constexpr const char* kInstructionSimplifierPassName = "instruction_simplifier";

  bool Run() override;

 private:
  CodeGenerator* codegen_;

  // Use all optimizations without restrictions.
  bool use_all_optimizations_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSimplifier);
};

// For bitwise operations (And/Or/Xor) with a negated input, try to use
// a negated bitwise instruction.
bool TryMergeNegatedInput(HBinaryOperation* op);

// Convert
// i1: AND a, b
//     SUB a, i1
// into:
//     BIC a, a, b
//
// It also works if `i1` is AND b, a
bool TryMergeWithAnd(HSub* instruction);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_H_
