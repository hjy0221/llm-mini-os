.section .text.exceptions, "ax"
.align 11
.global exception_vectors
.extern exception_handle

.macro VECTOR name, index
\name:
    mov x0, #\index
    b exception_entry_common
    .balign 0x80
.endm

// VBAR_EL1은 2KB 정렬된 16개 슬롯을 가리킨다.
exception_vectors:
    VECTOR current_sp0_sync,   0
    VECTOR current_sp0_irq,    1
    VECTOR current_sp0_fiq,    2
    VECTOR current_sp0_serror, 3

    VECTOR current_spx_sync,   4
    VECTOR current_spx_irq,    5
    VECTOR current_spx_fiq,    6
    VECTOR current_spx_serror, 7

    VECTOR lower_a64_sync,     8
    VECTOR lower_a64_irq,      9
    VECTOR lower_a64_fiq,     10
    VECTOR lower_a64_serror,  11

    VECTOR lower_a32_sync,    12
    VECTOR lower_a32_irq,     13
    VECTOR lower_a32_fiq,     14
    VECTOR lower_a32_serror,  15

exception_entry_common:
    msr daifset, #0xf
    bl exception_handle

.Lexception_park:
    wfe
    b .Lexception_park
