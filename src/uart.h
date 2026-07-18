#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_putc(char c);
void uart_puts(const char *text);
void uart_put_hex64(uint64_t value);
void uart_put_uint64(uint64_t value);
char uart_getc(void);

#endif
