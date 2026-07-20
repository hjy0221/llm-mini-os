#include "uart.h"

#include "gic.h"

#include <stdint.h>

// QEMU "virt" 보드의 PL011 UART.
#define UART0_BASE 0x09000000UL
#define UART_DR (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_RSR_ECR (*(volatile uint32_t *)(UART0_BASE + 0x04))
#define UART_FR (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_IBRD (*(volatile uint32_t *)(UART0_BASE + 0x24))
#define UART_FBRD (*(volatile uint32_t *)(UART0_BASE + 0x28))
#define UART_LCR_H (*(volatile uint32_t *)(UART0_BASE + 0x2c))
#define UART_CR (*(volatile uint32_t *)(UART0_BASE + 0x30))
#define UART_IFLS (*(volatile uint32_t *)(UART0_BASE + 0x34))
#define UART_IMSC (*(volatile uint32_t *)(UART0_BASE + 0x38))
#define UART_MIS (*(volatile uint32_t *)(UART0_BASE + 0x40))
#define UART_ICR (*(volatile uint32_t *)(UART0_BASE + 0x44))

#define UART_FR_RXFE (1U << 4)
#define UART_FR_TXFF (1U << 5)
#define UART_FR_BUSY (1U << 3)
#define UART_FR_TXFE (1U << 7)
#define UART_DR_ERROR_MASK (0x0fU << 8)

#define UART_LCR_H_FEN (1U << 4)
#define UART_LCR_H_WLEN_8 (3U << 5)

#define UART_CR_UARTEN (1U << 0)
#define UART_CR_TXE (1U << 8)
#define UART_CR_RXE (1U << 9)

#define UART_INT_RX (1U << 4)
#define UART_INT_RT (1U << 6)
#define UART_INT_FE (1U << 7)
#define UART_INT_PE (1U << 8)
#define UART_INT_BE (1U << 9)
#define UART_INT_OE (1U << 10)
#define UART_INT_ERROR_MASK \
    (UART_INT_FE | UART_INT_PE | UART_INT_BE | UART_INT_OE)
#define UART_RX_INTERRUPT_MASK \
    (UART_INT_RX | UART_INT_RT | UART_INT_ERROR_MASK)

#define UART_IFLS_RX_MASK (7U << 3)

static uint8_t rx_buffer[UART_RX_BUFFER_CAPACITY];
static uint32_t rx_head;
static uint32_t rx_tail;
static uint64_t rx_interrupts;
static uint64_t rx_bytes;
static uint64_t rx_overflows;
static uint64_t rx_errors;
static uint32_t rx_high_watermark;

_Static_assert((UART_RX_BUFFER_CAPACITY & (UART_RX_BUFFER_CAPACITY - 1U)) == 0U,
               "UART RX buffer capacity must be a power of two");

static int rx_buffer_push(uint8_t value) {
    uint32_t head = __atomic_load_n(&rx_head, __ATOMIC_RELAXED);
    uint32_t tail = __atomic_load_n(&rx_tail, __ATOMIC_ACQUIRE);
    uint32_t used = head - tail;

    if (used >= UART_RX_BUFFER_CAPACITY) {
        __atomic_add_fetch(&rx_overflows, 1U, __ATOMIC_RELAXED);
        return 0;
    }

    rx_buffer[head & (UART_RX_BUFFER_CAPACITY - 1U)] = value;
    __atomic_store_n(&rx_head, head + 1U, __ATOMIC_RELEASE);
    if (used + 1U > __atomic_load_n(&rx_high_watermark, __ATOMIC_RELAXED)) {
        __atomic_store_n(&rx_high_watermark, used + 1U, __ATOMIC_RELAXED);
    }
    return 1;
}

static int rx_buffer_pop(uint8_t *value) {
    uint32_t tail = __atomic_load_n(&rx_tail, __ATOMIC_RELAXED);
    uint32_t head = __atomic_load_n(&rx_head, __ATOMIC_ACQUIRE);

    if (tail == head) {
        return 0;
    }

    *value = rx_buffer[tail & (UART_RX_BUFFER_CAPACITY - 1U)];
    __atomic_store_n(&rx_tail, tail + 1U, __ATOMIC_RELEASE);
    return 1;
}

static int drain_rx_fifo(void) {
    int received = 0;

    while ((UART_FR & UART_FR_RXFE) == 0U) {
        uint32_t data = UART_DR;

        __atomic_add_fetch(&rx_bytes, 1U, __ATOMIC_RELAXED);
        if ((data & UART_DR_ERROR_MASK) != 0U) {
            __atomic_add_fetch(&rx_errors, 1U, __ATOMIC_RELAXED);
        }

        if (rx_buffer_push((uint8_t)data)) {
            received = 1;
        }
    }

    return received;
}

void uart_init(void) {
    // 설정 중 UART를 끄고 진행 중인 전송이 끝날 때까지 기다린다.
    UART_IMSC = 0U;
    UART_CR = 0U;
    while ((UART_FR & UART_FR_BUSY) != 0U) {
    }

    // FIFO enable을 먼저 내리면 이전 FIFO 내용과 상태가 초기화된다.
    UART_LCR_H = 0U;
    UART_ICR = 0x7ffU;
    UART_RSR_ECR = 0U;

    // QEMU virt의 24MHz UART clock에서 115200 baud를 사용한다.
    UART_IBRD = 13U;
    UART_FBRD = 1U;
    UART_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;

    // RX FIFO 임계값을 1/8로 낮추고 송수신과 UART를 활성화한다.
    UART_IFLS &= ~UART_IFLS_RX_MASK;
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    (void)drain_rx_fifo();

    gic_enable_interrupt(UART_INTERRUPT_ID, 0x80U);
    UART_ICR = 0x7ffU;
    UART_IMSC = UART_RX_INTERRUPT_MASK;

    __asm__ volatile(
        "dsb sy\n"
        "isb"
        ::: "memory");
}

void uart_handle_interrupt(void) {
    uint32_t pending = UART_MIS;
    int received;

    __atomic_add_fetch(&rx_interrupts, 1U, __ATOMIC_RELAXED);
    // RX/timeout은 FIFO를 비우면 해제된다. W1C는 오류 상태에만 사용한다.
    UART_ICR = pending & UART_INT_ERROR_MASK;
    received = drain_rx_fifo();

    if ((pending & UART_INT_ERROR_MASK) != 0U) {
        UART_RSR_ECR = 0U;
    }

    if (received) {
        // uart_getc()가 WFE에서 잠든 경우 즉시 깨운다.
        __asm__ volatile("sev" ::: "memory");
    }
}

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

void uart_flush(void) {
    while ((UART_FR & UART_FR_TXFE) == 0U ||
           (UART_FR & UART_FR_BUSY) != 0U) {
    }
}

char uart_getc(void) {
    uint8_t value;

    // 첫 WFE가 즉시 반환하게 한 뒤, 버퍼 확인과 수면 사이의 event를 놓치지 않는다.
    __asm__ volatile("sevl" ::: "memory");
    for (;;) {
        if (rx_buffer_pop(&value)) {
            return (char)value;
        }
        __asm__ volatile("wfe" ::: "memory");
    }
}

uint64_t uart_rx_interrupt_count(void) {
    return __atomic_load_n(&rx_interrupts, __ATOMIC_RELAXED);
}

uint64_t uart_rx_byte_count(void) {
    return __atomic_load_n(&rx_bytes, __ATOMIC_RELAXED);
}

uint64_t uart_rx_overflow_count(void) {
    return __atomic_load_n(&rx_overflows, __ATOMIC_RELAXED);
}

uint64_t uart_rx_error_count(void) {
    return __atomic_load_n(&rx_errors, __ATOMIC_RELAXED);
}

uint32_t uart_rx_high_watermark(void) {
    return __atomic_load_n(&rx_high_watermark, __ATOMIC_RELAXED);
}

uint32_t uart_rx_buffered_count(void) {
    uint32_t head = __atomic_load_n(&rx_head, __ATOMIC_ACQUIRE);
    uint32_t tail = __atomic_load_n(&rx_tail, __ATOMIC_ACQUIRE);
    return head - tail;
}
