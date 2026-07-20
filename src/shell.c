#include "shell.h"

#include "console.h"
#include "exception.h"
#include "platform.h"
#include "timer.h"
#include "uart.h"

#include <stddef.h>

static int strings_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

static void command_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help    Show this command list\n");
    uart_puts("  hello   Print a greeting\n");
    uart_puts("  clear   Clear the terminal\n");
    uart_puts("  info    Show OS information\n");
    uart_puts("  ticks   Show timer tick count\n");
    uart_puts("  uptime  Show seconds since boot\n");
    uart_puts("  fault   Trigger a test exception and halt\n");
    uart_puts("  shutdown Turn off the virtual machine\n");
    uart_puts("  reboot  Restart the virtual machine\n");
}

static void command_info(void) {
    uart_puts("llm-mini-os\n");
    uart_puts("  Architecture: AArch64\n");
    uart_puts("  Machine:      QEMU virt\n");
    uart_puts("  Console:      PL011 UART RX IRQ, 256-byte ring\n");
    uart_puts("  Shell:        built-in\n");
    uart_puts("  Exceptions:   VBAR_EL1 installed\n");
    uart_puts("  Interrupts:   GICv2\n");
    uart_puts("  Timer:        ARM Generic Timer, ");
    uart_put_uint64(timer_frequency());
    uart_puts(" Hz, ");
    uart_put_uint64(TIMER_TICKS_PER_SECOND);
    uart_puts(" ticks/s\n");
    uart_puts("  UART RX IRQs: ");
    uart_put_uint64(uart_rx_interrupt_count());
    uart_puts("\n  UART RX bytes: ");
    uart_put_uint64(uart_rx_byte_count());
    uart_puts("\n  UART buffered: ");
    uart_put_uint64(uart_rx_buffered_count());
    uart_puts("\n  UART highwater: ");
    uart_put_uint64(uart_rx_high_watermark());
    uart_puts("\n  UART drops:   ");
    uart_put_uint64(uart_rx_overflow_count());
    uart_puts("\n  UART errors:  ");
    uart_put_uint64(uart_rx_error_count());
    uart_puts("\n");
}

static void command_ticks(void) {
    uart_puts("Timer ticks: ");
    uart_put_uint64(timer_ticks());
    uart_puts("\n");
}

static void command_uptime(void) {
    uart_puts("Uptime: ");
    uart_put_uint64(timer_uptime_seconds());
    uart_puts(" seconds\n");
}

static void execute_command(const char *command) {
    if (strings_equal(command, "help")) {
        command_help();
    } else if (strings_equal(command, "hello")) {
        uart_puts("Hello!\n");
    } else if (strings_equal(command, "clear")) {
        uart_puts("\x1b[2J\x1b[H");
    } else if (strings_equal(command, "info")) {
        command_info();
    } else if (strings_equal(command, "ticks")) {
        command_ticks();
    } else if (strings_equal(command, "uptime")) {
        command_uptime();
    } else if (strings_equal(command, "fault")) {
        uart_puts("Triggering a BRK exception...\n");
        exception_trigger_test();
    } else if (strings_equal(command, "shutdown")) {
        uart_puts("Shutting down...\n");
        uart_flush();
        platform_shutdown();
        uart_puts("Shutdown failed.\n");
    } else if (strings_equal(command, "reboot")) {
        uart_puts("Rebooting...\n");
        uart_flush();
        platform_reboot();
        uart_puts("Reboot failed.\n");
    } else {
        uart_puts("Unknown command: ");
        uart_puts(command);
        uart_puts("\nType 'help' to see available commands.\n");
    }
}

void shell_run(void) {
    char command[CONSOLE_LINE_CAPACITY];

    for (;;) {
        uart_puts("mini-os> ");
        size_t length = console_readline(command, sizeof(command));

        if (length != 0U) {
            execute_command(command);
        }
    }
}
