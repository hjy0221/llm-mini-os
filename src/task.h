#ifndef TASK_H
#define TASK_H

#define TASK_MAX_COUNT 4

#define TASK_CONTEXT_X19_OFFSET 0
#define TASK_CONTEXT_X20_OFFSET 8
#define TASK_CONTEXT_X21_OFFSET 16
#define TASK_CONTEXT_X22_OFFSET 24
#define TASK_CONTEXT_X23_OFFSET 32
#define TASK_CONTEXT_X24_OFFSET 40
#define TASK_CONTEXT_X25_OFFSET 48
#define TASK_CONTEXT_X26_OFFSET 56
#define TASK_CONTEXT_X27_OFFSET 64
#define TASK_CONTEXT_X28_OFFSET 72
#define TASK_CONTEXT_X29_OFFSET 80
#define TASK_CONTEXT_X30_OFFSET 88
#define TASK_CONTEXT_SP_OFFSET 96
#define TASK_CONTEXT_SIZE 104

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>

typedef uint32_t task_id_t;
typedef void (*task_entry_t)(void *argument);

#define TASK_ID_INVALID UINT32_MAX

enum task_state {
    TASK_STATE_UNUSED,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_FINISHED,
};

struct task_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
    uint64_t sp;
};

struct task_info {
    enum task_state state;
    uint32_t has_allocated_stack;
    uint32_t stack_canary_ok;
    uint64_t schedule_count;
};

struct task_stats {
    task_id_t current_task;
    uint32_t active_tasks;
    uint32_t ready_tasks;
    uint32_t finished_tasks;
    uint64_t created_tasks;
    uint64_t context_switches;
    uint64_t yield_calls;
    uint64_t exited_tasks;
    uint64_t reaped_tasks;
    uint64_t stack_errors;
};

void task_init(void);
task_id_t task_create(task_entry_t entry, void *argument);
/* IRQ/exception handler가 아닌 일반 EL1 함수 흐름에서만 호출한다. */
void task_yield(void);
_Noreturn void task_exit(void);
int task_reap(task_id_t task_id);
size_t task_reap_finished(void);
task_id_t task_current_id(void);
int task_get_info(task_id_t task_id, struct task_info *info);
int task_get_stats(struct task_stats *stats);

_Static_assert(offsetof(struct task_context, x19) ==
                   TASK_CONTEXT_X19_OFFSET,
               "task context x19 offset mismatch");
_Static_assert(offsetof(struct task_context, x20) ==
                   TASK_CONTEXT_X20_OFFSET,
               "task context x20 offset mismatch");
_Static_assert(offsetof(struct task_context, x21) ==
                   TASK_CONTEXT_X21_OFFSET,
               "task context x21 offset mismatch");
_Static_assert(offsetof(struct task_context, x22) ==
                   TASK_CONTEXT_X22_OFFSET,
               "task context x22 offset mismatch");
_Static_assert(offsetof(struct task_context, x23) ==
                   TASK_CONTEXT_X23_OFFSET,
               "task context x23 offset mismatch");
_Static_assert(offsetof(struct task_context, x24) ==
                   TASK_CONTEXT_X24_OFFSET,
               "task context x24 offset mismatch");
_Static_assert(offsetof(struct task_context, x25) ==
                   TASK_CONTEXT_X25_OFFSET,
               "task context x25 offset mismatch");
_Static_assert(offsetof(struct task_context, x26) ==
                   TASK_CONTEXT_X26_OFFSET,
               "task context x26 offset mismatch");
_Static_assert(offsetof(struct task_context, x27) ==
                   TASK_CONTEXT_X27_OFFSET,
               "task context x27 offset mismatch");
_Static_assert(offsetof(struct task_context, x28) ==
                   TASK_CONTEXT_X28_OFFSET,
               "task context x28 offset mismatch");
_Static_assert(offsetof(struct task_context, x29) ==
                   TASK_CONTEXT_X29_OFFSET,
               "task context x29 offset mismatch");
_Static_assert(offsetof(struct task_context, x30) ==
                   TASK_CONTEXT_X30_OFFSET,
               "task context x30 offset mismatch");
_Static_assert(offsetof(struct task_context, sp) == TASK_CONTEXT_SP_OFFSET,
               "task context sp offset mismatch");
_Static_assert(sizeof(struct task_context) == TASK_CONTEXT_SIZE,
               "task context size mismatch");

#endif

#endif
