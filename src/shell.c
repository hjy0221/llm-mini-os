#include "shell.h"

#include "console.h"
#include "exception.h"
#include "mmu.h"
#include "page_alloc.h"
#include "platform.h"
#include "task.h"
#include "timer.h"
#include "uart.h"

#include <stddef.h>
#include <stdint.h>

#define SHELL_MAX_ARGUMENTS 8U
#define SLEEP_MAX_SECONDS 86400U
#define TASK_DEMO_DEFAULT_ROUNDS 3U
#define TASK_DEMO_MAX_ROUNDS 20U
#define MEMORY_TEST_PAGE_COUNT 3U

struct task_demo_argument {
    char label;
    uint64_t rounds;
};

enum parse_result {
    PARSE_OK,
    PARSE_TOO_MANY_ARGUMENTS,
    PARSE_UNTERMINATED_SINGLE_QUOTE,
    PARSE_UNTERMINATED_DOUBLE_QUOTE,
    PARSE_TRAILING_BACKSLASH,
};

static int strings_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

static enum parse_result parse_arguments(
    char *line, size_t *argument_count,
    char *arguments[SHELL_MAX_ARGUMENTS + 1U]) {
    char *read_cursor = line;
    char *write_cursor = line;

    *argument_count = 0U;

    for (;;) {
        char quote = '\0';

        while (*read_cursor == ' ') {
            ++read_cursor;
        }

        if (*read_cursor == '\0') {
            break;
        }

        if (*argument_count == SHELL_MAX_ARGUMENTS) {
            arguments[*argument_count] = NULL;
            return PARSE_TOO_MANY_ARGUMENTS;
        }

        arguments[*argument_count] = write_cursor;
        ++*argument_count;

        for (;;) {
            char current = *read_cursor;

            if (current == '\0') {
                *write_cursor = '\0';
                arguments[*argument_count] = NULL;
                if (quote == '\'') {
                    return PARSE_UNTERMINATED_SINGLE_QUOTE;
                }
                if (quote == '"') {
                    return PARSE_UNTERMINATED_DOUBLE_QUOTE;
                }
                return PARSE_OK;
            }

            if (current == '\\') {
                ++read_cursor;
                if (*read_cursor == '\0') {
                    *write_cursor = '\0';
                    arguments[*argument_count] = NULL;
                    return PARSE_TRAILING_BACKSLASH;
                }
                *write_cursor++ = *read_cursor++;
                continue;
            }

            if (quote != '\0') {
                ++read_cursor;
                if (current == quote) {
                    quote = '\0';
                } else {
                    *write_cursor++ = current;
                }
                continue;
            }

            if (current == '\'' || current == '"') {
                quote = current;
                ++read_cursor;
                continue;
            }

            if (current == ' ') {
                ++read_cursor;
                *write_cursor++ = '\0';
                break;
            }

            *write_cursor++ = current;
            ++read_cursor;
        }
    }

    arguments[*argument_count] = NULL;
    return PARSE_OK;
}

static void report_parse_error(enum parse_result result) {
    if (result == PARSE_TOO_MANY_ARGUMENTS) {
        uart_puts("Too many arguments (maximum 8 including command).\n");
    } else if (result == PARSE_UNTERMINATED_SINGLE_QUOTE) {
        uart_puts("Parse error: unterminated single quote.\n");
    } else if (result == PARSE_UNTERMINATED_DOUBLE_QUOTE) {
        uart_puts("Parse error: unterminated double quote.\n");
    } else if (result == PARSE_TRAILING_BACKSLASH) {
        uart_puts("Parse error: trailing backslash.\n");
    }
}

static int require_argument_count(size_t actual, size_t expected,
                                  const char *usage) {
    if (actual == expected) {
        return 1;
    }

    uart_puts("Usage: ");
    uart_puts(usage);
    uart_puts("\n");
    return 0;
}

static int parse_uint64(const char *text, uint64_t *value) {
    uint64_t result = 0U;

    if (*text == '\0') {
        return 0;
    }

    while (*text != '\0') {
        uint64_t digit;

        if (*text < '0' || *text > '9') {
            return 0;
        }

        digit = (uint64_t)(*text - '0');
        if (result > (UINT64_MAX - digit) / 10U) {
            return 0;
        }

        result = result * 10U + digit;
        ++text;
    }

    *value = result;
    return 1;
}

static void command_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help     Show this command list\n");
    uart_puts("  hello    Print a greeting\n");
    uart_puts("  echo ... Print command arguments\n");
    uart_puts("  sleep N  Wait for N seconds\n");
    uart_puts("  mem      Show physical page allocator statistics\n");
    uart_puts("  memtest  Allocate, verify, and free test pages\n");
    uart_puts("  mmu      Show ARM64 address translation state\n");
    uart_puts("  tasks    Show cooperative scheduler statistics\n");
    uart_puts("  taskdemo Run two round-robin kernel tasks\n");
    uart_puts("  clear    Clear the terminal\n");
    uart_puts("  info     Show OS information\n");
    uart_puts("  ticks    Show timer tick count\n");
    uart_puts("  uptime   Show seconds since boot\n");
    uart_puts("  fault    Trigger a test exception and halt\n");
    uart_puts("  shutdown Turn off the virtual machine\n");
    uart_puts("  reboot   Restart the virtual machine\n");
}

static void command_echo(size_t argument_count, char *arguments[]) {
    for (size_t index = 1U; index < argument_count; ++index) {
        if (index != 1U) {
            uart_putc(' ');
        }
        uart_puts(arguments[index]);
    }
    uart_puts("\n");
}

static void command_sleep(size_t argument_count, char *arguments[]) {
    uint64_t seconds;

    if (!require_argument_count(argument_count, 2U, "sleep <seconds>")) {
        return;
    }

    if (!parse_uint64(arguments[1], &seconds) ||
        seconds > SLEEP_MAX_SECONDS) {
        uart_puts("sleep: seconds must be an integer from 0 to ");
        uart_put_uint64(SLEEP_MAX_SECONDS);
        uart_puts("\n");
        return;
    }

    uart_puts("Sleeping for ");
    uart_put_uint64(seconds);
    uart_puts(seconds == 1U ? " second...\n" : " seconds...\n");
    timer_sleep_ticks(seconds * TIMER_TICKS_PER_SECOND);
    uart_puts("Done.\n");
}

static void command_mem(void) {
    uart_puts("Physical page allocator:\n");
    uart_puts("  Page size:   ");
    uart_put_uint64(PAGE_SIZE);
    uart_puts(" bytes\n  Total pages: ");
    uart_put_uint64(page_total_pages());
    uart_puts("\n  Used pages:  ");
    uart_put_uint64(page_used_pages());
    uart_puts("\n  Free pages:  ");
    uart_put_uint64(page_free_pages());
    uart_puts("\n");
}

static int page_is_zero(const void *page) {
    const uint64_t *words = page;

    for (size_t index = 0U; index < PAGE_SIZE / sizeof(*words); ++index) {
        if (words[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

static void command_memtest(void) {
    void *pages[MEMORY_TEST_PAGE_COUNT] = {NULL, NULL, NULL};
    void *original_pages[MEMORY_TEST_PAGE_COUNT] = {NULL, NULL, NULL};
    void *reused_page = NULL;
    uint64_t free_before = page_free_pages();
    int passed = 1;

    for (size_t index = 0U; index < MEMORY_TEST_PAGE_COUNT; ++index) {
        pages[index] = page_alloc();
        original_pages[index] = pages[index];
        if (pages[index] == NULL ||
            ((uintptr_t)pages[index] & (PAGE_SIZE - 1U)) != 0U ||
            !page_is_zero(pages[index])) {
            passed = 0;
            break;
        }
    }

    if (passed) {
        for (size_t index = 0U; index < MEMORY_TEST_PAGE_COUNT; ++index) {
            uint64_t *words = pages[index];

            words[0] = 0xa000U + index;
            words[PAGE_SIZE / sizeof(*words) / 2U] = 0xb000U + index;
            words[PAGE_SIZE / sizeof(*words) - 1U] = 0xc000U + index;
        }
        for (size_t index = 0U; index < MEMORY_TEST_PAGE_COUNT; ++index) {
            const uint64_t *words = pages[index];

            if (words[0] != 0xa000U + index ||
                words[PAGE_SIZE / sizeof(*words) / 2U] != 0xb000U + index ||
                words[PAGE_SIZE / sizeof(*words) - 1U] != 0xc000U + index) {
                passed = 0;
            }
        }
    }

    if (passed) {
        if (!page_free(pages[0])) {
            passed = 0;
        } else if (page_free(pages[0])) {
            /* 다른 페이지가 사용 중일 때 실제 free-list 중복 검사까지 확인한다. */
            uart_puts("memtest: FAIL (double-free was accepted)\n");
            return;
        } else {
            pages[0] = NULL;
        }
    }

    for (size_t index = 0U; index < MEMORY_TEST_PAGE_COUNT; ++index) {
        if (pages[index] != NULL) {
            if (!page_free(pages[index])) {
                passed = 0;
            } else {
                pages[index] = NULL;
            }
        }
    }

    if (passed && page_free_pages() == free_before) {
        reused_page = page_alloc();
        if (reused_page == NULL || !page_is_zero(reused_page) ||
            (reused_page != original_pages[0] &&
             reused_page != original_pages[1] &&
             reused_page != original_pages[2])) {
            passed = 0;
        }
    } else {
        passed = 0;
    }

    if (reused_page != NULL && !page_free(reused_page)) {
        passed = 0;
    }
    if (page_free_pages() != free_before) {
        passed = 0;
    }

    uart_puts(passed ? "memtest: PASS\n" : "memtest: FAIL\n");
}

static void command_mmu(void) {
    uart_puts("MMU: ");
    uart_puts(mmu_enabled() ? "on\n" : "off\n");
    uart_puts("D-cache: ");
    uart_puts(mmu_data_cache_enabled() ? "on\n" : "off\n");
    uart_puts("I-cache: ");
    uart_puts(mmu_instruction_cache_enabled() ? "on\n" : "off\n");
    uart_puts("TTBR0_EL1: ");
    uart_put_hex64(mmu_translation_base());
    uart_puts("\nMAIR_EL1:  ");
    uart_put_hex64(mmu_memory_attributes());
    uart_puts("\nTCR_EL1:   ");
    uart_put_hex64(mmu_translation_control());
    uart_puts("\nMapping:   identity, 4KiB granule, 2MiB blocks\n");
}

static void task_demo_worker(void *argument) {
    const struct task_demo_argument *worker = argument;

    for (uint64_t round = 1U; round <= worker->rounds; ++round) {
        uart_puts("Task ");
        uart_putc(worker->label);
        uart_puts(": ");
        uart_put_uint64(round);
        uart_puts("\n");
        if (round != worker->rounds) {
            task_yield();
        }
    }
}

static int task_finished_state(task_id_t task_id, int *finished) {
    struct task_info info;

    if (!task_get_info(task_id, &info)) {
        return 0;
    }
    *finished = info.state == TASK_STATE_FINISHED;
    return 1;
}

static void command_taskdemo(size_t argument_count, char *arguments[]) {
    struct task_demo_argument workers[2];
    uint64_t rounds = TASK_DEMO_DEFAULT_ROUNDS;
    task_id_t first;
    task_id_t second;

    if (argument_count > 2U) {
        uart_puts("Usage: taskdemo [rounds]\n");
        return;
    }
    if (argument_count == 2U &&
        (!parse_uint64(arguments[1], &rounds) || rounds == 0U ||
         rounds > TASK_DEMO_MAX_ROUNDS)) {
        uart_puts("taskdemo: rounds must be an integer from 1 to ");
        uart_put_uint64(TASK_DEMO_MAX_ROUNDS);
        uart_puts("\n");
        return;
    }

    workers[0].label = 'A';
    workers[0].rounds = rounds;
    workers[1].label = 'B';
    workers[1].rounds = rounds;

    first = task_create(task_demo_worker, &workers[0]);
    if (first == TASK_ID_INVALID) {
        uart_puts("taskdemo: could not create task A\n");
        return;
    }
    second = task_create(task_demo_worker, &workers[1]);
    if (second == TASK_ID_INVALID) {
        int finished = 0;

        uart_puts("taskdemo: could not create task B\n");
        while (task_finished_state(first, &finished) && !finished) {
            task_yield();
        }
        if (!finished || !task_reap(first)) {
            uart_puts("taskdemo: FAIL (task A cleanup error)\n");
        }
        return;
    }

    uart_puts("Cooperative task demo:\n");
    for (;;) {
        int first_finished;
        int second_finished;

        if (!task_finished_state(first, &first_finished) ||
            !task_finished_state(second, &second_finished)) {
            uart_puts("taskdemo: FAIL (task state unavailable)\n");
            return;
        }
        if (first_finished && second_finished) {
            break;
        }
        task_yield();
    }

    {
        int first_reaped = task_reap(first);
        int second_reaped = task_reap(second);

        if (first_reaped && second_reaped) {
            uart_puts("taskdemo: PASS\n");
        } else {
            uart_puts("taskdemo: FAIL (stack or page release error)\n");
        }
    }
}

static void command_tasks(void) {
    struct task_stats stats;

    if (!task_get_stats(&stats)) {
        uart_puts("Scheduler is not initialized.\n");
        return;
    }

    uart_puts("Cooperative scheduler:\n");
    uart_puts("  Current task:    ");
    uart_put_uint64(stats.current_task);
    uart_puts("\n  Active tasks:    ");
    uart_put_uint64(stats.active_tasks);
    uart_puts("\n  Ready tasks:     ");
    uart_put_uint64(stats.ready_tasks);
    uart_puts("\n  Finished tasks:  ");
    uart_put_uint64(stats.finished_tasks);
    uart_puts("\n  Created tasks:   ");
    uart_put_uint64(stats.created_tasks);
    uart_puts("\n  Context switches: ");
    uart_put_uint64(stats.context_switches);
    uart_puts("\n  Yield calls:     ");
    uart_put_uint64(stats.yield_calls);
    uart_puts("\n  Exited tasks:    ");
    uart_put_uint64(stats.exited_tasks);
    uart_puts("\n  Reaped tasks:    ");
    uart_put_uint64(stats.reaped_tasks);
    uart_puts("\n  Stack errors:    ");
    uart_put_uint64(stats.stack_errors);
    uart_puts("\n");
}

static void command_info(void) {
    uart_puts("llm-mini-os\n");
    uart_puts("  Architecture: AArch64\n");
    uart_puts("  Machine:      QEMU virt\n");
    uart_puts("  Console:      PL011 UART RX IRQ, 256-byte ring\n");
    uart_puts("  Shell:        built-in\n");
    uart_puts("  Exceptions:   VBAR_EL1 installed\n");
    uart_puts("  Interrupts:   GICv2\n");
    uart_puts("  MMU:          ");
    uart_puts(mmu_enabled() ? "identity map enabled\n" : "disabled\n");
    uart_puts("  Memory:       4KiB page allocator, ");
    uart_put_uint64(page_free_pages());
    uart_puts("/");
    uart_put_uint64(page_total_pages());
    uart_puts(" pages free\n");
    uart_puts("  Scheduler:    cooperative round-robin\n");
    uart_puts("  Timer:        ARM Generic Timer, ");
    uart_put_uint64(timer_frequency());
    uart_puts(" Hz, ");
    uart_put_uint64(TIMER_TICKS_PER_SECOND);
    uart_puts(" ticks/s\n");
    uart_puts("  UART RX IRQs: ");
    uart_put_uint64(uart_rx_interrupt_count());
    uart_puts("\n  UART RX bytes: ");
    uart_put_uint64(uart_rx_byte_count());
    uart_puts("\n  UART buffered: ");
    uart_put_uint64(uart_rx_buffered_count());
    uart_puts("\n  UART highwater: ");
    uart_put_uint64(uart_rx_high_watermark());
    uart_puts("\n  UART drops:   ");
    uart_put_uint64(uart_rx_overflow_count());
    uart_puts("\n  UART errors:  ");
    uart_put_uint64(uart_rx_error_count());
    uart_puts("\n");
}

static void command_ticks(void) {
    uart_puts("Timer ticks: ");
    uart_put_uint64(timer_ticks());
    uart_puts("\n");
}

static void command_uptime(void) {
    uart_puts("Uptime: ");
    uart_put_uint64(timer_uptime_seconds());
    uart_puts(" seconds\n");
}

static void execute_command(size_t argument_count, char *arguments[]) {
    const char *command = arguments[0];

    if (strings_equal(command, "help")) {
        if (require_argument_count(argument_count, 1U, "help")) {
            command_help();
        }
    } else if (strings_equal(command, "hello")) {
        if (require_argument_count(argument_count, 1U, "hello")) {
            uart_puts("Hello!\n");
        }
    } else if (strings_equal(command, "echo")) {
        command_echo(argument_count, arguments);
    } else if (strings_equal(command, "sleep")) {
        command_sleep(argument_count, arguments);
    } else if (strings_equal(command, "mem")) {
        if (require_argument_count(argument_count, 1U, "mem")) {
            command_mem();
        }
    } else if (strings_equal(command, "memtest")) {
        if (require_argument_count(argument_count, 1U, "memtest")) {
            command_memtest();
        }
    } else if (strings_equal(command, "mmu")) {
        if (require_argument_count(argument_count, 1U, "mmu")) {
            command_mmu();
        }
    } else if (strings_equal(command, "tasks")) {
        if (require_argument_count(argument_count, 1U, "tasks")) {
            command_tasks();
        }
    } else if (strings_equal(command, "taskdemo")) {
        command_taskdemo(argument_count, arguments);
    } else if (strings_equal(command, "clear")) {
        if (require_argument_count(argument_count, 1U, "clear")) {
            uart_puts("\x1b[2J\x1b[H");
        }
    } else if (strings_equal(command, "info")) {
        if (require_argument_count(argument_count, 1U, "info")) {
            command_info();
        }
    } else if (strings_equal(command, "ticks")) {
        if (require_argument_count(argument_count, 1U, "ticks")) {
            command_ticks();
        }
    } else if (strings_equal(command, "uptime")) {
        if (require_argument_count(argument_count, 1U, "uptime")) {
            command_uptime();
        }
    } else if (strings_equal(command, "fault")) {
        if (require_argument_count(argument_count, 1U, "fault")) {
            uart_puts("Triggering a BRK exception...\n");
            exception_trigger_test();
        }
    } else if (strings_equal(command, "shutdown")) {
        if (require_argument_count(argument_count, 1U, "shutdown")) {
            uart_puts("Shutting down...\n");
            uart_flush();
            platform_shutdown();
            uart_puts("Shutdown failed.\n");
        }
    } else if (strings_equal(command, "reboot")) {
        if (require_argument_count(argument_count, 1U, "reboot")) {
            uart_puts("Rebooting...\n");
            uart_flush();
            platform_reboot();
            uart_puts("Reboot failed.\n");
        }
    } else {
        uart_puts("Unknown command: ");
        uart_puts(command);
        uart_puts("\nType 'help' to see available commands.\n");
    }
}

void shell_run(void) {
    char command[CONSOLE_LINE_CAPACITY];
    char *arguments[SHELL_MAX_ARGUMENTS + 1U];

    for (;;) {
        size_t argument_count;
        enum parse_result parse_result;

        uart_puts("mini-os> ");
        (void)console_readline(command, sizeof(command));

        parse_result = parse_arguments(command, &argument_count, arguments);
        if (parse_result != PARSE_OK) {
            report_parse_error(parse_result);
            continue;
        }

        if (argument_count != 0U) {
            execute_command(argument_count, arguments);
        }
    }
}
