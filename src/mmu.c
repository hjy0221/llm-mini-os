#include "mmu.h"

#include <stddef.h>
#include <stdint.h>

#define TABLE_ENTRY_COUNT 512U
#define L2_BLOCK_SIZE 0x200000UL

#define QEMU_GIC_BASE 0x08000000UL
#define QEMU_UART_BASE 0x09000000UL
#define QEMU_RAM_BASE 0x40000000UL
#define QEMU_RAM_END 0x48000000UL

#define DESC_VALID (1ULL << 0)
#define DESC_TABLE (1ULL << 1)
#define DESC_BLOCK DESC_VALID
#define DESC_AF (1ULL << 10)
#define DESC_SH_OUTER (2ULL << 8)
#define DESC_SH_INNER (3ULL << 8)
#define DESC_ATTR_INDEX(index) ((uint64_t)(index) << 2)
#define DESC_PXN (1ULL << 53)
#define DESC_UXN (1ULL << 54)
#define DESC_TABLE_OUTPUT_MASK 0x0000fffffffff000ULL
#define DESC_OUTPUT_MASK 0x0000ffffffe00000ULL

#define MAIR_DEVICE_NGNRNE 0x00ULL
#define MAIR_NORMAL_WBWA 0xffULL

#define SCTLR_M (1ULL << 0)
#define SCTLR_C (1ULL << 2)
#define SCTLR_I (1ULL << 12)

#define TCR_T0SZ_32 32ULL
#define TCR_IRGN0_WBWA (1ULL << 8)
#define TCR_ORGN0_WBWA (1ULL << 10)
#define TCR_SH0_INNER (3ULL << 12)
#define TCR_T1SZ_48 (16ULL << 16)
#define TCR_EPD1 (1ULL << 23)
#define TCR_TG1_4K (2ULL << 30)
#define TCR_IPS_SHIFT 32U

/*
 * T0SZ=32와 4KiB granule에서는 L1 -> L2 순서로 찾는다.
 * L2 block 하나는 연속된 2MiB를 매핑한다.
 */
static uint64_t level1_table[TABLE_ENTRY_COUNT] __attribute__((aligned(4096)));
static uint64_t device_level2_table[TABLE_ENTRY_COUNT]
    __attribute__((aligned(4096)));
static uint64_t ram_level2_table[TABLE_ENTRY_COUNT]
    __attribute__((aligned(4096)));
static int installed;

_Static_assert((QEMU_GIC_BASE & (L2_BLOCK_SIZE - 1U)) == 0U,
               "GIC mapping must be 2MiB-aligned");
_Static_assert((QEMU_UART_BASE & (L2_BLOCK_SIZE - 1U)) == 0U,
               "UART mapping must be 2MiB-aligned");
_Static_assert((QEMU_RAM_BASE & (L2_BLOCK_SIZE - 1U)) == 0U,
               "RAM mapping must be 2MiB-aligned");
_Static_assert((QEMU_RAM_END & (L2_BLOCK_SIZE - 1U)) == 0U,
               "RAM end must be 2MiB-aligned");
_Static_assert(sizeof(level1_table) == 4096U,
               "translation table must occupy one 4KiB page");

static uint64_t read_sctlr_el1(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));
    return value;
}

static uint64_t read_id_aa64mmfr0_el1(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(value));
    return value;
}

static uint64_t read_mair_el1(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, mair_el1" : "=r"(value));
    return value;
}

static uint64_t read_tcr_el1(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, tcr_el1" : "=r"(value));
    return value;
}

static uint64_t read_ttbr0_el1(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(value));
    return value;
}

static void clear_table(uint64_t table[TABLE_ENTRY_COUNT]) {
    for (size_t index = 0U; index < TABLE_ENTRY_COUNT; ++index) {
        table[index] = 0U;
    }
}

static uint64_t table_descriptor(const uint64_t *table) {
    return ((uint64_t)(uintptr_t)table & DESC_TABLE_OUTPUT_MASK) |
           DESC_VALID | DESC_TABLE;
}

static uint64_t block_descriptor(uintptr_t address, uint64_t attributes) {
    return ((uint64_t)address & DESC_OUTPUT_MASK) | DESC_BLOCK |
           DESC_AF | attributes;
}

static void map_device_block(uintptr_t address) {
    size_t index = (size_t)((address >> 21) & 0x1ffU);

    device_level2_table[index] = block_descriptor(
        address, DESC_ATTR_INDEX(0U) | DESC_SH_OUTER | DESC_PXN | DESC_UXN);
}

static void build_translation_tables(void) {
    clear_table(level1_table);
    clear_table(device_level2_table);
    clear_table(ram_level2_table);

    /* VA 0x00000000..0x3fffffff 안에서 실제로 쓰는 장치 block만 연다. */
    level1_table[0] = table_descriptor(device_level2_table);
    map_device_block(QEMU_GIC_BASE);
    map_device_block(QEMU_UART_BASE);

    /* VA/PA 0x40000000..0x47ffffff: QEMU가 제공한 128MiB RAM. */
    level1_table[1] = table_descriptor(ram_level2_table);
    for (uintptr_t address = QEMU_RAM_BASE; address < QEMU_RAM_END;
         address += L2_BLOCK_SIZE) {
        size_t index = (size_t)((address >> 21) & 0x1ffU);
        ram_level2_table[index] = block_descriptor(
            address, DESC_ATTR_INDEX(1U) | DESC_SH_INNER | DESC_UXN);
    }
}

int mmu_init(void) {
    uint64_t physical_address_range;
    uint64_t mair;
    uint64_t tcr;
    uint64_t sctlr;

    sctlr = read_sctlr_el1();
    if ((sctlr & SCTLR_M) != 0U) {
        /* 이 함수가 설치한 table로 두 번째 호출된 경우만 성공이다. */
        return installed &&
               (read_ttbr0_el1() & ~0xfffULL) ==
                   ((uint64_t)(uintptr_t)level1_table & ~0xfffULL);
    }
    if ((sctlr & (SCTLR_C | SCTLR_I)) != 0U) {
        /* Cache가 이미 켜진 알 수 없는 부팅 상태는 조용히 변경하지 않는다. */
        return 0;
    }

    build_translation_tables();

    /* Attr0=Device-nGnRnE, Attr1=Normal Write-Back Read/Write-Allocate. */
    mair = (MAIR_DEVICE_NGNRNE << 0) | (MAIR_NORMAL_WBWA << 8);

    physical_address_range = read_id_aa64mmfr0_el1() & 0xfU;
    if (physical_address_range == 6U) {
        /* 이 커널의 4KiB/48-bit descriptor 형식에서 안전한 상한. */
        physical_address_range = 5U;
    } else if (physical_address_range > 6U) {
        return 0;
    }

    tcr = TCR_T0SZ_32 | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA |
          TCR_SH0_INNER | TCR_T1SZ_48 | TCR_EPD1 | TCR_TG1_4K |
          (physical_address_range << TCR_IPS_SHIFT);

    __asm__ volatile(
        "dsb sy\n"
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "msr ttbr1_el1, xzr\n"
        "isb\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        :
        : "r"(mair), "r"(tcr), "r"((uint64_t)(uintptr_t)level1_table)
        : "memory");

    /*
     * 첫 단계에서는 주소 변환만 켠다. Cache는 set/way invalidate와
     * 페이지별 실행 권한을 추가한 뒤 별도 단계에서 활성화한다.
     */
    sctlr |= SCTLR_M;
    __asm__ volatile(
        "msr sctlr_el1, %0\n"
        "isb\n"
        :
        : "r"(sctlr)
        : "memory");

    installed = mmu_enabled();
    return installed;
}

int mmu_enabled(void) {
    return (read_sctlr_el1() & SCTLR_M) != 0U;
}

int mmu_data_cache_enabled(void) {
    return (read_sctlr_el1() & SCTLR_C) != 0U;
}

int mmu_instruction_cache_enabled(void) {
    return (read_sctlr_el1() & SCTLR_I) != 0U;
}

uint64_t mmu_translation_base(void) {
    return read_ttbr0_el1();
}

uint64_t mmu_memory_attributes(void) {
    return read_mair_el1();
}

uint64_t mmu_translation_control(void) {
    return read_tcr_el1();
}
