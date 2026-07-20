#include "gic.h"

#include <stddef.h>

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

#define GICD_CTLR 0x000U
#define GICD_ISENABLER 0x100U
#define GICD_ICENABLER 0x180U
#define GICD_ICPENDR 0x280U
#define GICD_IPRIORITYR 0x400U
#define GICD_ITARGETSR 0x800U
#define GICD_ICFGR 0xc00U

#define GICC_CTLR 0x000U
#define GICC_PMR 0x004U
#define GICC_BPR 0x008U
#define GICC_IAR 0x00cU
#define GICC_EOIR 0x010U

static volatile uint32_t *gicd_register(size_t offset) {
    return (volatile uint32_t *)(GICD_BASE + offset);
}

static volatile uint32_t *gicc_register(size_t offset) {
    return (volatile uint32_t *)(GICC_BASE + offset);
}

static void data_sync_barrier(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

void gic_init(void) {
    // 설정 중에는 Distributor와 CPU interface를 모두 닫는다.
    *gicd_register(GICD_CTLR) = 0U;
    *gicc_register(GICC_CTLR) = 0U;

    // 모든 우선순위를 CPU가 받을 수 있게 하고 이진 우선순위 분할을 끈다.
    *gicc_register(GICC_PMR) = 0xffU;
    *gicc_register(GICC_BPR) = 0U;

    data_sync_barrier();
    *gicc_register(GICC_CTLR) = 1U;
    *gicd_register(GICD_CTLR) = 1U;
    data_sync_barrier();
}

void gic_enable_interrupt(uint32_t interrupt_id, uint8_t priority) {
    volatile uint8_t *priority_register =
        (volatile uint8_t *)(GICD_BASE + GICD_IPRIORITYR + interrupt_id);
    uint32_t register_index = interrupt_id / 32U;
    uint32_t bit = interrupt_id % 32U;

    // 먼저 해당 IRQ를 끄고 남아 있을 수 있는 pending 상태를 제거한다.
    *gicd_register(GICD_ICENABLER + register_index * sizeof(uint32_t)) =
        1U << bit;
    *gicd_register(GICD_ICPENDR + register_index * sizeof(uint32_t)) =
        1U << bit;

    // PPI/SPI를 level-triggered로 설정한다. 타이머 PPI 30번에 필요하다.
    if (interrupt_id >= 16U) {
        uint32_t config_index = interrupt_id / 16U;
        uint32_t config_shift = (interrupt_id % 16U) * 2U;
        volatile uint32_t *config_register =
            gicd_register(GICD_ICFGR + config_index * sizeof(uint32_t));
        uint32_t config = *config_register;

        config &= ~(3U << config_shift);
        *config_register = config;
    }

    // 다중 CPU GIC의 SPI는 target이 필요하므로 CPU 0을 지정한다.
    // 단일 CPU GIC에서는 이 필드가 RAZ/WI일 수 있다.
    if (interrupt_id >= 32U) {
        volatile uint8_t *target_register =
            (volatile uint8_t *)(GICD_BASE + GICD_ITARGETSR + interrupt_id);
        *target_register = 1U;
    }

    *priority_register = priority;
    *gicd_register(GICD_ISENABLER + register_index * sizeof(uint32_t)) =
        1U << bit;
    data_sync_barrier();
}

uint32_t gic_acknowledge_interrupt(void) {
    return *gicc_register(GICC_IAR);
}

void gic_end_interrupt(uint32_t acknowledge_value) {
    // 장치의 interrupt clear가 GIC EOI보다 먼저 관측되어야 한다.
    data_sync_barrier();
    *gicc_register(GICC_EOIR) = acknowledge_value;
    data_sync_barrier();
}

uint32_t gic_interrupt_id(uint32_t acknowledge_value) {
    return acknowledge_value & 0x3ffU;
}
