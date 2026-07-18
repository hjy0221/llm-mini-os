#ifndef GIC_H
#define GIC_H

#include <stdint.h>

#define GIC_SPURIOUS_INTERRUPT_MIN 1020U

void gic_init(void);
void gic_enable_interrupt(uint32_t interrupt_id, uint8_t priority);
uint32_t gic_acknowledge_interrupt(void);
void gic_end_interrupt(uint32_t acknowledge_value);
uint32_t gic_interrupt_id(uint32_t acknowledge_value);

#endif
