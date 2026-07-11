#include "platform.h"

#include <stdint.h>

#define PSCI_SYSTEM_RESET 0x84000009ULL

void platform_reboot(void) {
    register uint64_t function_id __asm__("x0") = PSCI_SYSTEM_RESET;
    register uint64_t argument_1 __asm__("x1") = 0U;
    register uint64_t argument_2 __asm__("x2") = 0U;
    register uint64_t argument_3 __asm__("x3") = 0U;

    __asm__ volatile(
        "hvc #0"
        : "+r"(function_id)
        : "r"(argument_1), "r"(argument_2), "r"(argument_3)
        : "memory");
}
