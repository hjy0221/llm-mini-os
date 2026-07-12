#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdint.h>

void exception_init(void);
void exception_trigger_test(void);
void exception_handle(uint64_t vector_index) __attribute__((noreturn));

#endif
