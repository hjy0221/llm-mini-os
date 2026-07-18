#include "timer.h"

#include "gic.h"

static volatile uint64_t tick_count;
static uint64_t counter_frequency;
static uint64_t tick_interval;
static uint64_t next_deadline;

static uint64_t read_counter_frequency(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

static uint64_t read_counter_value(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

static void set_compare_value(uint64_t compare_value) {
    __asm__ volatile(
        "msr cntp_cval_el0, %0\n"
        "isb"
        :
        : "r"(compare_value)
        : "memory");
}

void timer_init(void) {
    counter_frequency = read_counter_frequency();
    tick_interval = counter_frequency / TIMER_TICKS_PER_SECOND;
    if (tick_interval == 0U) {
        tick_interval = 1U;
    }

    gic_enable_interrupt(TIMER_INTERRUPT_ID, 0x80U);
    next_deadline = read_counter_value() + tick_interval;
    set_compare_value(next_deadline);

    // ENABLE=1, IMASK=0: 물리 타이머를 시작하고 IRQ를 허용한다.
    __asm__ volatile(
        "msr cntp_ctl_el0, %0\n"
        "dsb sy\n"
        "isb"
        :
        : "r"(1UL)
        : "memory");
}

void timer_handle_interrupt(void) {
    uint64_t now = read_counter_value();

    if (now >= next_deadline) {
        // 처리 지연으로 여러 주기를 놓쳐도 시간과 틱 수가 뒤처지지 않게 한다.
        uint64_t elapsed_ticks = (now - next_deadline) / tick_interval + 1U;
        tick_count += elapsed_ticks;
        next_deadline += elapsed_ticks * tick_interval;
    }

    set_compare_value(next_deadline);
}

uint64_t timer_ticks(void) {
    return tick_count;
}

uint64_t timer_uptime_seconds(void) {
    return tick_count / TIMER_TICKS_PER_SECOND;
}

uint64_t timer_frequency(void) {
    return counter_frequency;
}
