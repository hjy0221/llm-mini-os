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

void uart_put_uint64(uint64_t value) {
    char digits[20];
    unsigned int length = 0U;

    if (value == 0U) {
        uart_putc('0');
        return;
    }

    while (value != 0U) {
        digits[length++] = (char)('0' + value % 10U);
        value /= 10U;
    }

    while (length != 0U) {
        uart_putc(digits[--length]);
    }
}

char uart_getc(void) {
    while ((UART_FR & UART_FR_RXFE) != 0U) {
        // UART는 아직 polling 방식이지만, 타이머 IRQ가 10ms마다 깨워 준다.
        // 계속 레지스터를 읽는 대신 다음 인터럽트까지 CPU를 쉬게 한다.
        __asm__ volatile("wfi");
    }
    return (char)(UART_DR & 0xffU);
}
