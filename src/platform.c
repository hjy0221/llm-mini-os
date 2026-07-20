#include "platform.h"

#include <stdint.h>

#define PSCI_SYSTEM_OFF 0x84000008ULL
#define PSCI_SYSTEM_RESET 0x84000009ULL

static void psci_system_call(uint64_t function) {
    register uint64_t function_id __asm__("x0") = function;
    register uint64_t argument_1 __asm__("x1") = 0U;
    register uint64_t argument_2 __asm__("x2") = 0U;
    register uint64_t argument_3 __asm__("x3") = 0U;

    __asm__ volatile(
        "hvc #0"
        : "+r"(function_id)
        : "r"(argument_1), "r"(argument_2), "r"(argument_3)
        : "memory");
}

void platform_shutdown(void) {
    psci_system_call(PSCI_SYSTEM_OFF);
}

void platform_reboot(void) {
    psci_system_call(PSCI_SYSTEM_RESET);
}
