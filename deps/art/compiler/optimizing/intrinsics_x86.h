/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_X86_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_X86_H_

#include "base/macros.h"
#include "intrinsics.h"
#include "intrinsics_list.h"

namespace art HIDDEN {

class ArenaAllocator;
class HInvokeStaticOrDirect;
class HInvokeVirtual;

namespace x86 {

class CodeGeneratorX86;
class X86Assembler;

class IntrinsicLocationsBuilderX86 final : public IntrinsicVisitor {
 public:
  explicit IntrinsicLocationsBuilderX86(CodeGeneratorX86* codegen);

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  void Visit ## Name(HInvoke* invoke) override;
  ART_INTRINSICS_WITH_HINVOKE_LIST(OPTIMIZING_INTRINSICS)
#undef OPTIMIZING_INTRINSICS

  // Check whether an invoke is an intrinsic, and if so, create a location summary. Returns whether
  // a corresponding LocationSummary with the intrinsified_ flag set was generated and attached to
  // the invoke.
  bool TryDispatch(HInvoke* invoke);

 private:
  ArenaAllocator* const allocator_;
  CodeGeneratorX86* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicLocationsBuilderX86);
};

class IntrinsicCodeGeneratorX86 final : public IntrinsicVisitor {
 public:
  explicit IntrinsicCodeGeneratorX86(CodeGeneratorX86* codegen) : codegen_(codegen) {}

  // Define visitor methods.

#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache, SideEffects, Exceptions, ...) \
  void Visit ## Name(HInvoke* invoke) override;
  ART_INTRINSICS_WITH_HINVOKE_LIST(OPTIMIZING_INTRINSICS)
#undef OPTIMIZING_INTRINSICS

 private:
  X86Assembler* GetAssembler();

  ArenaAllocator* GetAllocator();

  void HandleValueOf(HInvoke* invoke,
                     const IntrinsicVisitor::ValueOfInfo& info,
                     DataType::Type type);

  CodeGeneratorX86* const codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicCodeGeneratorX86);
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_X86_H_
