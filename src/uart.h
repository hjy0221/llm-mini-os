#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART_INTERRUPT_ID 33U
#define UART_RX_BUFFER_CAPACITY 256U

void uart_init(void);
void uart_handle_interrupt(void);
void uart_putc(char c);
void uart_puts(const char *text);
void uart_put_hex64(uint64_t value);
void uart_put_uint64(uint64_t value);
void uart_flush(void);
char uart_getc(void);
uint64_t uart_rx_interrupt_count(void);
uint64_t uart_rx_byte_count(void);
uint64_t uart_rx_overflow_count(void);
uint64_t uart_rx_error_count(void);
uint32_t uart_rx_high_watermark(void);
uint32_t uart_rx_buffered_count(void);

#endif
