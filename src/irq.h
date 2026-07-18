#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

void irq_enable(void);
void irq_handle(void);
uint64_t irq_unexpected_count(void);

#endif
