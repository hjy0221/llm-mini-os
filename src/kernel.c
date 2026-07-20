#include "exception.h"
#include "gic.h"
#include "irq.h"
#include "mmu.h"
#include "page_alloc.h"
#include "shell.h"
#include "task.h"
#include "timer.h"
#include "uart.h"

void kernel_main(void) {
    exception_init();
    if (!mmu_init()) {
        for (;;) {
            __asm__ volatile("wfe" ::: "memory");
        }
    }
    page_alloc_init();
    task_init();
    gic_init();
    uart_init();
    timer_init();
    irq_enable();

    uart_puts("\nHello from ARM64 QEMU Mini OS!\n");
    uart_puts("Kernel is running on QEMU virt.\n");
    uart_puts("Type 'help' to see available commands.\n\n");

    shell_run();
}
