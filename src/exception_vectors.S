.section .text.exceptions, "ax"
.align 11
.global exception_vectors
.extern exception_handle
.extern irq_handle

.macro FATAL_VECTOR name, index
\name:
    mov x0, #\index
    b exception_fatal_entry
    .balign 0x80
.endm

.macro IRQ_VECTOR name
\name:
    b irq_entry_common
    .balign 0x80
.endm

// Current EL/SP0 예외는 SP_EL1으로 바꾼 뒤 공통 처리기를 사용한다.
// eret은 SPSR_EL1의 원래 SPSel 값을 복원한다.
.macro FATAL_SP0_VECTOR name, index
\name:
    msr spsel, #1
    mov x0, #\index
    b exception_fatal_entry
    .balign 0x80
.endm

.macro IRQ_SP0_VECTOR name
\name:
    msr spsel, #1
    b irq_entry_common
    .balign 0x80
.endm

// VBAR_EL1은 2KB 정렬된 16개 슬롯을 가리킨다.
exception_vectors:
    FATAL_SP0_VECTOR current_sp0_sync,   0
    IRQ_SP0_VECTOR   current_sp0_irq
    FATAL_SP0_VECTOR current_sp0_fiq,    2
    FATAL_SP0_VECTOR current_sp0_serror, 3

    FATAL_VECTOR current_spx_sync,   4
    IRQ_VECTOR   current_spx_irq
    FATAL_VECTOR current_spx_fiq,    6
    FATAL_VECTOR current_spx_serror, 7

    FATAL_VECTOR lower_a64_sync,     8
    IRQ_VECTOR   lower_a64_irq
    FATAL_VECTOR lower_a64_fiq,     10
    FATAL_VECTOR lower_a64_serror,  11

    FATAL_VECTOR lower_a32_sync,    12
    IRQ_VECTOR   lower_a32_irq
    FATAL_VECTOR lower_a32_fiq,     14
    FATAL_VECTOR lower_a32_serror,  15

exception_fatal_entry:
    msr daifset, #0xf
    bl exception_handle

.Lexception_park:
    wfe
    b .Lexception_park

// IRQ 프레임(272바이트): x0-x30, ELR_EL1, SPSR_EL1, 정렬 여유 공간.
irq_entry_common:
    msr daifset, #0x2
    sub sp, sp, #272

    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30,      [sp, #240]

    mrs x0, elr_el1
    str x0, [sp, #248]
    mrs x0, spsr_el1
    str x0, [sp, #256]

    bl irq_handle

    ldr x0, [sp, #248]
    msr elr_el1, x0
    ldr x0, [sp, #256]
    msr spsr_el1, x0

    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30,      [sp, #240]

    add sp, sp, #272
    eret
