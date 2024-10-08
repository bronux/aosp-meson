/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_X86_64_ASM_SUPPORT_X86_64_H_
#define ART_RUNTIME_ARCH_X86_64_ASM_SUPPORT_X86_64_H_

#include "asm_support.h"
#include "entrypoints/entrypoint_asm_constants.h"

#define FRAME_SIZE_SAVE_ALL_CALLEE_SAVES (64 + 4*8)
#define FRAME_SIZE_SAVE_REFS_ONLY (64 + 4*8)
#define FRAME_SIZE_SAVE_REFS_AND_ARGS (112 + 12*8)
#define FRAME_SIZE_SAVE_EVERYTHING (144 + 16*8)
#define FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT FRAME_SIZE_SAVE_EVERYTHING
#define FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK FRAME_SIZE_SAVE_EVERYTHING
#define SAVE_EVERYTHING_FRAME_RAX_OFFSET \
    (FRAME_SIZE_SAVE_EVERYTHING - CALLEE_SAVE_EVERYTHING_NUM_CORE_SPILLS * POINTER_SIZE)

// Size of Context::gprs_.
#define X86_64_LONG_JUMP_GPRS_SIZE 136
// Size of Context::fprs_.
#define X86_64_LONG_JUMP_FPRS_SIZE 128
// Size of Context::gprs_ + Context::fprs_.
#define X86_64_LONG_JUMP_CONTEXT_SIZE (X86_64_LONG_JUMP_GPRS_SIZE + X86_64_LONG_JUMP_FPRS_SIZE)

#endif  // ART_RUNTIME_ARCH_X86_64_ASM_SUPPORT_X86_64_H_
