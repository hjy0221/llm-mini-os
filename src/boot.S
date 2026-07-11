.section .text.boot
.global _start
.extern kernel_main
.extern __bss_start
.extern __bss_end
.extern __stack_top

_start:
    // QEMU가 여러 CPU를 시작해도 CPU 0만 커널을 실행한다.
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbnz x0, .Lpark

    ldr x0, =__bss_start
    ldr x1, =__bss_end
.Lclear_bss:
    cmp x0, x1
    b.hs .Lbss_done
    str xzr, [x0], #8
    b .Lclear_bss

.Lbss_done:
    ldr x0, =__stack_top
    mov sp, x0
    bl kernel_main

.Lpark:
    wfe
    b .Lpark
