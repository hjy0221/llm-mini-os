#include "irq.h"

#include "gic.h"
#include "timer.h"

static volatile uint64_t unexpected_count;

void irq_enable(void) {
    // DAIF의 I 비트만 지워 IRQ를 허용한다. FIQ 등은 계속 마스크한다.
    __asm__ volatile(
        "msr daifclr, #2\n"
        "isb"
        ::: "memory");
}

void irq_handle(void) {
    uint32_t acknowledge_value = gic_acknowledge_interrupt();
    uint32_t interrupt_id = gic_interrupt_id(acknowledge_value);

    if (interrupt_id >= GIC_SPURIOUS_INTERRUPT_MIN) {
        return;
    }

    if (interrupt_id == TIMER_INTERRUPT_ID) {
        timer_handle_interrupt();
    } else {
        ++unexpected_count;
    }

    // GIC는 CPU 번호 비트까지 포함된 IAR 원본 값을 돌려받아야 한다.
    gic_end_interrupt(acknowledge_value);
}

uint64_t irq_unexpected_count(void) {
    return unexpected_count;
}
