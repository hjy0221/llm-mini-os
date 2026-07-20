ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text : ALIGN(16) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : ALIGN(16) { *(.rodata .rodata.*) }
    .data   : ALIGN(16) { *(.data .data.*) }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . += 0x10000;
    __stack_top = .;

    . = ALIGN(0x1000);
    __kernel_end = .;

    ASSERT(__kernel_end <= 0x48000000,
           "kernel image exceeds QEMU virt RAM")
}
