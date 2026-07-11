#include "shell.h"
#include "uart.h"

void kernel_main(void) {
    uart_puts("\nHello from ARM64 QEMU Mini OS!\n");
    uart_puts("Kernel is running on QEMU virt.\n");
    uart_puts("Type 'help' to see available commands.\n\n");

    shell_run();
}
