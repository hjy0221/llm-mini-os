#include "console.h"

#include "uart.h"

static int is_printable(char c) {
    return c >= ' ' && c <= '~';
}

size_t console_readline(char *buffer, size_t capacity) {
    size_t length = 0;

    if (capacity == 0U) {
        return 0U;
    }

    for (;;) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            uart_puts("\n");
            break;
        }

        if (c == '\b' || c == 0x7f) {
            if (length > 0U) {
                --length;
                uart_puts("\b \b");
            }
            continue;
        }

        if (is_printable(c) && length + 1U < capacity) {
            buffer[length++] = c;
            uart_putc(c);
        }
    }

    buffer[length] = '\0';
    return length;
}
