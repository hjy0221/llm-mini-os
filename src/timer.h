#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_INTERRUPT_ID 30U
#define TIMER_TICKS_PER_SECOND 100U

void timer_init(void);
void timer_handle_interrupt(void);
uint64_t timer_ticks(void);
uint64_t timer_uptime_seconds(void);
uint64_t timer_frequency(void);

#endif
