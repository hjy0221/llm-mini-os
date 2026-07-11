#ifndef UART_H
#define UART_H

void uart_putc(char c);
void uart_puts(const char *text);
char uart_getc(void);

#endif
