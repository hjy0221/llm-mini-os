#ifndef MMU_H
#define MMU_H

#include <stdint.h>

/*
 * QEMU virt의 낮은 4GiB에서 사용하는 RAM/MMIO를 같은 주소로 매핑한다.
 * 초기 커널은 EL1에서만 실행되며 TTBR0_EL1 하나만 사용한다.
 */
int mmu_init(void);
int mmu_enabled(void);
int mmu_data_cache_enabled(void);
int mmu_instruction_cache_enabled(void);
uint64_t mmu_translation_base(void);
uint64_t mmu_memory_attributes(void);
uint64_t mmu_translation_control(void);

#endif
