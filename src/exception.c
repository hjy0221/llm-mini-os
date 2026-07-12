#include "exception.h"

#include "uart.h"

extern char exception_vectors[];

static uint64_t read_esr_el1(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
    return value;
}

static uint64_t read_elr_el1(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, elr_el1" : "=r"(value));
    return value;
}

static uint64_t read_far_el1(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, far_el1" : "=r"(value));
    return value;
}

static uint64_t read_spsr_el1(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(value));
    return value;
}

static const char *vector_name(uint64_t vector_index) {
    static const char *const names[] = {
        "Synchronous, current EL with SP0",
        "IRQ, current EL with SP0",
        "FIQ, current EL with SP0",
        "SError, current EL with SP0",
        "Synchronous, current EL with SPx",
        "IRQ, current EL with SPx",
        "FIQ, current EL with SPx",
        "SError, current EL with SPx",
        "Synchronous, lower EL using AArch64",
        "IRQ, lower EL using AArch64",
        "FIQ, lower EL using AArch64",
        "SError, lower EL using AArch64",
        "Synchronous, lower EL using AArch32",
        "IRQ, lower EL using AArch32",
        "FIQ, lower EL using AArch32",
        "SError, lower EL using AArch32",
    };

    if (vector_index < sizeof(names) / sizeof(names[0])) {
        return names[vector_index];
    }
    return "Unknown vector";
}

static const char *exception_class_name(uint64_t exception_class) {
    switch (exception_class) {
    case 0x15:
        return "SVC instruction from AArch64";
    case 0x20:
    case 0x21:
        return "Instruction abort";
    case 0x24:
    case 0x25:
        return "Data abort";
    case 0x3c:
        return "BRK instruction";
    default:
        return "Unclassified or unsupported";
    }
}

void exception_init(void) {
    __asm__ volatile(
        "msr vbar_el1, %0\n"
        "isb"
        :
        : "r"(exception_vectors)
        : "memory");
}

void exception_trigger_test(void) {
    __asm__ volatile("brk #0");
}

void exception_handle(uint64_t vector_index) {
    uint64_t esr = read_esr_el1();
    uint64_t exception_class = (esr >> 26U) & 0x3fU;

    uart_puts("\n*** ARM64 EXCEPTION ***\n");
    uart_puts("Vector:   ");
    uart_puts(vector_name(vector_index));
    uart_puts("\nESR_EL1: ");
    uart_put_hex64(esr);
    uart_puts("\nEC:      ");
    uart_put_hex64(exception_class);
    uart_puts(" (");
    uart_puts(exception_class_name(exception_class));
    uart_puts(")\nELR_EL1: ");
    uart_put_hex64(read_elr_el1());
    uart_puts("\nFAR_EL1: ");
    uart_put_hex64(read_far_el1());
    uart_puts("\nSPSR:    ");
    uart_put_hex64(read_spsr_el1());
    uart_puts("\nSystem halted. Exit QEMU with Control-A, X.\n");

    for (;;) {
        __asm__ volatile("wfe");
    }
}
