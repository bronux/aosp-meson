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

#ifndef ART_RUNTIME_ARCH_X86_ASM_SUPPORT_X86_S_
#define ART_RUNTIME_ARCH_X86_ASM_SUPPORT_X86_S_

#include "asm_support_x86.h"
#include "interpreter/cfi_asm_support.h"

// Regular gas(1) & current clang/llvm assembler support named macro parameters.
#define MACRO0(macro_name) .macro macro_name
#define MACRO1(macro_name, macro_arg1) .macro macro_name macro_arg1
#define MACRO2(macro_name, macro_arg1, macro_arg2) .macro macro_name macro_arg1, macro_arg2
#define MACRO3(macro_name, macro_arg1, macro_arg2, macro_arg3) .macro macro_name macro_arg1, macro_arg2, macro_arg3
#define MACRO4(macro_name, macro_arg1, macro_arg2, macro_arg3, macro_arg4) .macro macro_name macro_arg1, macro_arg2, macro_arg3, macro_arg4
#define MACRO5(macro_name, macro_arg1, macro_arg2, macro_arg3, macro_arg4, macro_arg5) .macro macro_name macro_arg1, macro_arg2, macro_arg3, macro_arg4, macro_arg5
#define END_MACRO .endm

#if defined(__clang__)
    // Clang/llvm does not support .altmacro. However, the clang/llvm preprocessor doesn't
    // separate the backslash and parameter by a space. Everything just works.
    #define RAW_VAR(name) \name
    #define VAR(name) \name
    #define CALLVAR(name) SYMBOL(\name)
    #define PLT_VAR(name) \name@PLT
    #define REG_VAR(name) %\name
    #define CALL_MACRO(name) \name
#else
    // Regular gas(1) uses \argument_name for macro arguments.
    // We need to turn on alternate macro syntax so we can use & instead or the preprocessor
    // will screw us by inserting a space between the \ and the name. Even in this mode there's
    // no special meaning to $, so literals are still just $x. The use of altmacro means % is a
    // special character meaning care needs to be taken when passing registers as macro
    // arguments.
    .altmacro
    #define RAW_VAR(name) name&
    #define VAR(name) name&
    #define CALLVAR(name) SYMBOL(name&)
    #define PLT_VAR(name) name&@PLT
    #define REG_VAR(name) %name
    #define CALL_MACRO(name) name&
#endif

#define LITERAL(value) $value
#if defined(__APPLE__)
    #define MACRO_LITERAL(value) $(value)
#else
    #define MACRO_LITERAL(value) $value
#endif

#if defined(__APPLE__)
    #define FUNCTION_TYPE(name)
    #define SIZE(name)
#else
    #define FUNCTION_TYPE(name) .type name, @function
    #define SIZE(name) .size name, .-name
#endif

    // CFI support.
#if !defined(__APPLE__)
    #define CFI_STARTPROC .cfi_startproc
    #define CFI_ENDPROC .cfi_endproc
    #define CFI_ADJUST_CFA_OFFSET(size) .cfi_adjust_cfa_offset size
    #define CFI_DEF_CFA(reg,size) .cfi_def_cfa reg,size
    #define CFI_DEF_CFA_REGISTER(reg) .cfi_def_cfa_register reg
    #define CFI_RESTORE(reg) .cfi_restore reg
    #define CFI_REL_OFFSET(reg,size) .cfi_rel_offset reg,size
    #define CFI_REGISTER(orig_reg, current_reg) .cfi_register orig_reg, current_reg
    #define CFI_REMEMBER_STATE .cfi_remember_state
    // The spec is not clear whether the CFA is part of the saved state and tools
    // differ in the behaviour, so explicitly set the CFA to avoid any ambiguity.
    // The restored CFA state should match the CFA state during CFI_REMEMBER_STATE.
    // `objdump -Wf libart.so | egrep "_cfa|_state"` is useful to audit the opcodes.
    MACRO2(CFI_RESTORE_STATE_AND_DEF_CFA, reg, off)
        .cfi_restore_state
        .cfi_def_cfa \reg,\off
    END_MACRO
    #define CFI_ESCAPE(...) .cfi_escape __VA_ARGS__
    #define CFI_RESTORE_STATE .cfi_restore_state
#else
    // Mac OS' doesn't like cfi_* directives.
    #define CFI_STARTPROC
    #define CFI_ENDPROC
    #define CFI_ADJUST_CFA_OFFSET(size)
    #define CFI_DEF_CFA(reg,size)
    #define CFI_DEF_CFA_REGISTER(reg)
    #define CFI_RESTORE(reg)
    #define CFI_REL_OFFSET(reg,size)
    #define CFI_REGISTER(orig_reg, current_reg)
    #define CFI_REMEMBER_STATE
    MACRO2(CFI_RESTORE_STATE_AND_DEF_CFA, reg, off)
    END_MACRO
    #define CFI_ESCAPE(...)
    #define CFI_RESTORE_STATE
#endif

#define CFI_REG_eax 0
#define CFI_REG_ecx 1
#define CFI_REG_edx 2
#define CFI_REG_ebx 3
#define CFI_REG_esp 4
#define CFI_REG_ebp 5
#define CFI_REG_esi 6
#define CFI_REG_edi 7
#define CFI_REG_eip 8

#define CFI_REG(reg) CFI_REG_##reg

MACRO3(CFI_EXPRESSION_BREG, n, b, offset)
    .if (-0x40 <= (\offset)) && ((\offset) < 0x40)
        CFI_EXPRESSION_BREG_1(\n, \b, \offset)
    .elseif (-0x2000 <= (\offset)) && ((\offset) < 0x2000)
        CFI_EXPRESSION_BREG_2(\n, \b, \offset)
    .else
        .error "Unsupported offset"
    .endif
END_MACRO

MACRO3(CFI_DEF_CFA_BREG_PLUS_UCONST, reg, offset, size)
    .if ((\size) < 0)
        .error "Size should be positive"
    .endif
    .if (((\offset) < -0x40) || ((\offset) >= 0x40))
        .error "Unsupported offset"
    .endif
    .if ((\size) < 0x80)
        CFI_DEF_CFA_BREG_PLUS_UCONST_1_1(\reg, \offset, \size)
    .elseif ((\size) < 0x4000)
        CFI_DEF_CFA_BREG_PLUS_UCONST_1_2(\reg, \offset, \size)
    .else
        .error "Unsupported size"
    .endif
END_MACRO

    // Symbols. On a Mac, we need a leading underscore.
#if !defined(__APPLE__)
    #define SYMBOL(name) name
    #define PLT_SYMBOL(name) name ## @PLT
#else
    // Mac OS' symbols have an _ prefix.
    #define SYMBOL(name) _ ## name
    #define PLT_SYMBOL(name) _ ## name
#endif

// Directive to hide a function symbol.
#if defined(__APPLE__)
    #define ASM_HIDDEN .private_extern
#else
    #define ASM_HIDDEN .hidden
#endif

    /* Cache alignment for function entry */
MACRO0(ALIGN_FUNCTION_ENTRY)
    .balign 16
END_MACRO

MACRO2(DEFINE_FUNCTION_CUSTOM_CFA, c_name, cfa_offset)
    FUNCTION_TYPE(SYMBOL(\c_name))
    ASM_HIDDEN CALLVAR(c_name)
    .globl CALLVAR(c_name)
    ALIGN_FUNCTION_ENTRY
CALLVAR(c_name):
    CFI_STARTPROC
    // Ensure we get an appropriate starting CFA.
    CFI_DEF_CFA(esp, RAW_VAR(cfa_offset))
END_MACRO

MACRO1(DEFINE_FUNCTION, c_name)
    DEFINE_FUNCTION_CUSTOM_CFA RAW_VAR(c_name), __SIZEOF_POINTER__
END_MACRO

MACRO1(END_FUNCTION, c_name)
    CFI_ENDPROC
    SIZE(SYMBOL(\c_name))
END_MACRO

MACRO1(PUSH, reg)
    pushl REG_VAR(reg)
    CFI_ADJUST_CFA_OFFSET(4)
    CFI_REL_OFFSET(REG_VAR(reg), 0)
END_MACRO

MACRO1(POP, reg)
    popl REG_VAR(reg)
    CFI_ADJUST_CFA_OFFSET(-4)
    CFI_RESTORE(REG_VAR(reg))
END_MACRO

// Arguments do not need .cfi_rel_offset as they are caller-saved and
// therefore cannot hold caller's variables or unwinding data.
MACRO1(PUSH_ARG, reg)
    pushl REG_VAR(reg)
    CFI_ADJUST_CFA_OFFSET(4)
END_MACRO

MACRO1(POP_ARG, reg)
    popl REG_VAR(reg)
    CFI_ADJUST_CFA_OFFSET(-4)
END_MACRO

MACRO1(CFI_RESTORE_REG, reg)
    CFI_RESTORE(REG_VAR(reg))
END_MACRO

MACRO1(INCREASE_FRAME, frame_adjustment)
    subl MACRO_LITERAL(RAW_VAR(frame_adjustment)), %esp
    CFI_ADJUST_CFA_OFFSET((RAW_VAR(frame_adjustment)))
END_MACRO

MACRO1(DECREASE_FRAME, frame_adjustment)
    addl MACRO_LITERAL(RAW_VAR(frame_adjustment)), %esp
    CFI_ADJUST_CFA_OFFSET(-(RAW_VAR(frame_adjustment)))
END_MACRO

#define UNREACHABLE int3

MACRO1(UNIMPLEMENTED,name)
    FUNCTION_TYPE(\name)
    .globl VAR(name)
    ALIGN_FUNCTION_ENTRY
VAR(name):
    CFI_STARTPROC
    UNREACHABLE
    UNREACHABLE
    CFI_ENDPROC
    SIZE(\name)
END_MACRO

MACRO3(SETUP_PC_REL_BASE_IMPL, reg, label, call_label)
    call RAW_VAR(call_label)
    CFI_ADJUST_CFA_OFFSET(4)
RAW_VAR(label):
    popl REG_VAR(reg)
    CFI_ADJUST_CFA_OFFSET(-4)
END_MACRO

MACRO1(SETUP_PC_REL_BASE_0, reg)
    SETUP_PC_REL_BASE_IMPL \reg, 0, 0f
END_MACRO

MACRO2(SETUP_PC_REL_BASE, reg, label)
    SETUP_PC_REL_BASE_IMPL \reg, \label, \label
END_MACRO

MACRO1(LOAD_RUNTIME_INSTANCE, reg)
    SETUP_PC_REL_BASE_0 \reg
    // Load Runtime::instance_.
    movl SYMBOL(_ZN3art7Runtime9instance_E) - 0b(REG_VAR(reg)), REG_VAR(reg)
END_MACRO

// Macros to poison (negate) the reference for heap poisoning.
MACRO1(POISON_HEAP_REF, rRef)
#ifdef USE_HEAP_POISONING
    neg REG_VAR(rRef)
#endif  // USE_HEAP_POISONING
END_MACRO

// Macros to unpoison (negate) the reference for heap poisoning.
MACRO1(UNPOISON_HEAP_REF, rRef)
#ifdef USE_HEAP_POISONING
    neg REG_VAR(rRef)
#endif  // USE_HEAP_POISONING
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly)
     */
MACRO1(SETUP_SAVE_REFS_ONLY_FRAME, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    LOAD_RUNTIME_INSTANCE \temp_reg
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 3*4 + 16 + 4)
#error "FRAME_SIZE_SAVE_REFS_ONLY(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_SAVE_REFS_ONLY_FRAME)
    addl MACRO_LITERAL(16), %esp  // Unwind stack up to saved values
    CFI_ADJUST_CFA_OFFSET(-16)
    POP ebp  // Restore callee saves (ebx is saved/restored by the upcall)
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     */
MACRO1(SETUP_SAVE_ALL_CALLEE_SAVES_FRAME, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    LOAD_RUNTIME_INSTANCE \temp_reg
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 3*4 + 16 + 4)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs), except for pushing the method
     */
MACRO0(SETUP_SAVE_REFS_AND_ARGS_FRAME_REGISTERS_ONLY)
    PUSH edi      // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH_ARG ebx  // Save args.
    PUSH_ARG edx
    PUSH_ARG ecx
    // Create space for FPR args.
    INCREASE_FRAME 4 * 8
    // Save FPRs.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    // Ugly compile-time check, but we only have the preprocessor.
    // First +4: implicit return address pushed on stack when caller made call.
    // Last +4: we're not pushing the method on the stack here.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 4 + 6*4 + 4*8 + 4)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_SAVE_REFS_AND_ARGS_FRAME)
    // Restore FPRs. The method is still on the stack.
    movsd 4(%esp), %xmm0
    movsd 12(%esp), %xmm1
    movsd 20(%esp), %xmm2
    movsd 28(%esp), %xmm3

    DECREASE_FRAME 36             // Remove FPRs and method pointer.

    POP_ARG ecx                   // Restore args
    POP_ARG edx
    POP_ARG ebx
    POP ebp                       // Restore callee saves
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     */
MACRO0(DELIVER_PENDING_EXCEPTION_FRAME_READY)
    // Outgoing argument set up
    INCREASE_FRAME 12                          // alignment padding
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*)
    call SYMBOL(art_quick_do_long_jump)
    UNREACHABLE
    CFI_ADJUST_CFA_OFFSET(-16)                 // Reset CFA in case there is more code afterwards.
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx      // save callee saves for throw
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END_MACRO

MACRO0(RETURN_OR_DELIVER_PENDING_EXCEPTION)
    cmpl MACRO_LITERAL(0),%fs:THREAD_EXCEPTION_OFFSET // exception field == 0 ?
    jne 1f                                            // if exception field != 0 goto 1
    ret                                               // return
1:                                                    // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

// Locking is needed for both managed code and JNI stubs.
MACRO4(LOCK_OBJECT_FAST_PATH, obj, tmp, saved_eax, slow_lock)
1:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj)), %eax  // EAX := lock word
    movl %fs:THREAD_ID_OFFSET, REG_VAR(tmp)  // tmp: thread id.
    xorl %eax, REG_VAR(tmp)               // tmp: thread id with count 0 + read barrier bits.
    testl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %eax  // Test the non-gc bits.
    jnz  2f                               // Check if unlocked.
    // Unlocked case - store tmp: original lock word plus thread id, preserved read barrier bits.
                                          // EAX: old val, tmp: new val.
    lock cmpxchg REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
    jnz  1b                               // cmpxchg failed retry
    .ifnc \saved_eax, none
        movl REG_VAR(saved_eax), %eax     // Restore EAX.
    .endif
    ret
2:  // EAX: original lock word, tmp: thread id ^ EAX
                                          // Check lock word state and thread id together,
    testl LITERAL(LOCK_WORD_STATE_MASK_SHIFTED | LOCK_WORD_THIN_LOCK_OWNER_MASK_SHIFTED), \
          REG_VAR(tmp)
    jne  \slow_lock                       // Slow path if either of the two high bits are set.
                                          // Increment the recursive lock count.
    leal LOCK_WORD_THIN_LOCK_COUNT_ONE(%eax), REG_VAR(tmp)
    testl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_MASK_SHIFTED), REG_VAR(tmp)
    jz   \slow_lock                       // If count overflowed, go to slow lock.
    // Update lockword for recursive lock, cmpxchg necessary for read barrier bits.
                                          // EAX: old val, tmp: new val.
    lock cmpxchg REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
    jnz  1b                               // cmpxchg failed retry
    .ifnc \saved_eax, none
        movl REG_VAR(saved_eax), %eax     // Restore EAX.
    .endif
    ret
END_MACRO

// Unlocking is needed for both managed code and JNI stubs.
MACRO4(UNLOCK_OBJECT_FAST_PATH, obj, tmp, saved_eax, slow_unlock)
1:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj)), %eax  // EAX := lock word
    movl %fs:THREAD_ID_OFFSET, REG_VAR(tmp)  // tmp := thread id
    xorl %eax, REG_VAR(tmp)               // tmp := thread id ^ lock word
    test LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), REG_VAR(tmp)
    jnz  2f                               // Check if simply locked.
    // Transition to unlocked.
#ifndef USE_READ_BARRIER
    movl REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
#else
    lock cmpxchg REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
    jnz  1b                               // cmpxchg failed retry
#endif
    .ifnc \saved_eax, none
        movl REG_VAR(saved_eax), %eax     // Restore EAX.
    .endif
    ret
2:  // EAX: original lock word, tmp: lock_word ^ thread id
                                          // Check lock word state and thread id together.
    testl LITERAL(LOCK_WORD_STATE_MASK_SHIFTED | LOCK_WORD_THIN_LOCK_OWNER_MASK_SHIFTED), \
          REG_VAR(tmp)
    jnz  \slow_unlock
    // Update lockword for recursive unlock, cmpxchg necessary for read barrier bits.
                                          // tmp: new lock word with decremented count.
    leal -LOCK_WORD_THIN_LOCK_COUNT_ONE(%eax), REG_VAR(tmp)
#ifndef USE_READ_BARRIER
    movl REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
#else
    lock cmpxchg REG_VAR(tmp), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(obj))
    jnz  1b                               // cmpxchg failed retry
#endif
    .ifnc \saved_eax, none
        movl REG_VAR(saved_eax), %eax     // Restore EAX.
    .endif
    ret
END_MACRO

#endif  // ART_RUNTIME_ARCH_X86_ASM_SUPPORT_X86_S_
