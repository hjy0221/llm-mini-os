#include "uart.h"

#include <stdint.h>

// QEMU "virt" 보드의 PL011 UART.
#define UART0_BASE 0x09000000UL
#define UART_DR (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR (*(volatile uint32_t *)(UART0_BASE + 0x18))

#define UART_FR_RXFE (1U << 4)
#define UART_FR_TXFF (1U << 5)

void uart_putc(char c) {
    while ((UART_FR & UART_FR_TXFF) != 0U) {
    }
    UART_DR = (uint32_t)c;
}

void uart_puts(const char *text) {
    while (*text != '\0') {
        if (*text == '\n') {
            uart_putc('\r');
        }
        uart_putc(*text++);
    }
}

void uart_put_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> (unsigned int)shift) & 0xfU]);
    }
}

char uart_getc(void) {
    while ((UART_FR & UART_FR_RXFE) != 0U) {
    }
    return (char)(UART_DR & 0xffU);
}
