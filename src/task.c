#include "task.h"

#include "page_alloc.h"

#include <stddef.h>
#include <stdint.h>

#define TASK_STACK_CANARY 0x7461736b7374616bULL

struct task_slot {
    struct task_context context;
    task_entry_t entry;
    void *argument;
    void *stack_page;
    enum task_state state;
    int stack_corrupted;
    uint64_t schedule_count;
};

extern void task_context_switch(struct task_context *previous,
                                const struct task_context *next);

static struct task_slot tasks[TASK_MAX_COUNT];
static task_id_t current_task;
static uint64_t created_tasks;
static uint64_t context_switches;
static uint64_t yield_calls;
static uint64_t exited_tasks;
static uint64_t reaped_tasks;
static uint64_t stack_errors;
static int initialized;

static uint64_t stack_canary(task_id_t task_id) {
    return TASK_STACK_CANARY ^ (uint64_t)task_id;
}

static int stack_canary_ok(task_id_t task_id) {
    const struct task_slot *task = &tasks[task_id];

    return task->stack_page == NULL ||
           *(const uint64_t *)task->stack_page == stack_canary(task_id);
}

static void record_stack_error(task_id_t task_id) {
    if (!tasks[task_id].stack_corrupted && !stack_canary_ok(task_id)) {
        tasks[task_id].stack_corrupted = 1;
        ++stack_errors;
    }
}

static void clear_context(struct task_context *context) {
    context->x19 = 0U;
    context->x20 = 0U;
    context->x21 = 0U;
    context->x22 = 0U;
    context->x23 = 0U;
    context->x24 = 0U;
    context->x25 = 0U;
    context->x26 = 0U;
    context->x27 = 0U;
    context->x28 = 0U;
    context->x29 = 0U;
    context->x30 = 0U;
    context->sp = 0U;
}

static void clear_slot(struct task_slot *task) {
    clear_context(&task->context);
    task->entry = NULL;
    task->argument = NULL;
    task->stack_page = NULL;
    task->state = TASK_STATE_UNUSED;
    task->stack_corrupted = 0;
    task->schedule_count = 0U;
}

static task_id_t find_next_ready(void) {
    for (task_id_t distance = 1U; distance < TASK_MAX_COUNT; ++distance) {
        task_id_t candidate = (current_task + distance) % TASK_MAX_COUNT;

        if (tasks[candidate].state == TASK_STATE_READY) {
            return candidate;
        }
    }

    return TASK_ID_INVALID;
}

static _Noreturn void task_bootstrap(void) {
    task_entry_t entry = tasks[current_task].entry;
    void *argument = tasks[current_task].argument;

    entry(argument);
    task_exit();
}

void task_init(void) {
    if (initialized) {
        return;
    }

    for (task_id_t task_id = 0U; task_id < TASK_MAX_COUNT; ++task_id) {
        clear_slot(&tasks[task_id]);
    }

    current_task = 0U;
    tasks[0].state = TASK_STATE_RUNNING;
    tasks[0].schedule_count = 1U;
    created_tasks = 0U;
    context_switches = 0U;
    yield_calls = 0U;
    exited_tasks = 0U;
    reaped_tasks = 0U;
    stack_errors = 0U;
    initialized = 1;
}

task_id_t task_create(task_entry_t entry, void *argument) {
    struct task_slot *task;
    void *stack_page;
    task_id_t task_id;

    if (!initialized || entry == NULL) {
        return TASK_ID_INVALID;
    }

    for (task_id = 1U; task_id < TASK_MAX_COUNT; ++task_id) {
        if (tasks[task_id].state == TASK_STATE_UNUSED) {
            break;
        }
    }

    if (task_id == TASK_MAX_COUNT) {
        return TASK_ID_INVALID;
    }

    stack_page = page_alloc();
    if (stack_page == NULL) {
        return TASK_ID_INVALID;
    }

    task = &tasks[task_id];
    clear_slot(task);
    task->entry = entry;
    task->argument = argument;
    task->stack_page = stack_page;
    task->state = TASK_STATE_READY;
    *(uint64_t *)stack_page = stack_canary(task_id);
    task->context.sp = (uint64_t)(uintptr_t)stack_page + PAGE_SIZE;
    task->context.x30 = (uint64_t)(uintptr_t)task_bootstrap;
    ++created_tasks;
    return task_id;
}

void task_yield(void) {
    task_id_t previous;
    task_id_t next;

    if (!initialized) {
        return;
    }

    record_stack_error(current_task);
    if (tasks[current_task].stack_corrupted && current_task != 0U) {
        task_exit();
    }

    ++yield_calls;
    next = find_next_ready();
    if (next == TASK_ID_INVALID) {
        return;
    }

    previous = current_task;
    tasks[previous].state = TASK_STATE_READY;
    tasks[next].state = TASK_STATE_RUNNING;
    ++tasks[next].schedule_count;
    current_task = next;
    ++context_switches;
    task_context_switch(&tasks[previous].context, &tasks[next].context);
}

_Noreturn void task_exit(void) {
    task_id_t previous;
    task_id_t next;

    if (!initialized || current_task == 0U) {
        for (;;) {
            task_yield();
            __asm__ volatile("wfe" ::: "memory");
        }
    }

    previous = current_task;
    record_stack_error(previous);
    tasks[previous].state = TASK_STATE_FINISHED;
    ++exited_tasks;
    next = find_next_ready();

    if (next != TASK_ID_INVALID) {
        tasks[next].state = TASK_STATE_RUNNING;
        ++tasks[next].schedule_count;
        current_task = next;
        ++context_switches;
        task_context_switch(&tasks[previous].context, &tasks[next].context);
    }

    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

int task_reap(task_id_t task_id) {
    struct task_slot *task;

    if (!initialized || current_task != 0U || task_id >= TASK_MAX_COUNT ||
        task_id == current_task) {
        return 0;
    }

    task = &tasks[task_id];
    if (task->state != TASK_STATE_FINISHED || task->stack_page == NULL) {
        return 0;
    }

    record_stack_error(task_id);
    if (task->stack_corrupted) {
        /* 아래로 자라는 stack이 page 바닥을 넘은 흔적이면 해제하지 않는다. */
        return 0;
    }

    if (!page_free(task->stack_page)) {
        return 0;
    }

    clear_slot(task);
    ++reaped_tasks;
    return 1;
}

size_t task_reap_finished(void) {
    size_t count = 0U;

    if (!initialized) {
        return 0U;
    }

    for (task_id_t task_id = 1U; task_id < TASK_MAX_COUNT; ++task_id) {
        if (task_reap(task_id)) {
            ++count;
        }
    }

    return count;
}

task_id_t task_current_id(void) {
    if (!initialized) {
        return TASK_ID_INVALID;
    }
    return current_task;
}

int task_get_info(task_id_t task_id, struct task_info *info) {
    if (!initialized || task_id >= TASK_MAX_COUNT || info == NULL) {
        return 0;
    }

    info->state = tasks[task_id].state;
    info->has_allocated_stack = tasks[task_id].stack_page != NULL;
    info->stack_canary_ok = !tasks[task_id].stack_corrupted &&
                            stack_canary_ok(task_id);
    info->schedule_count = tasks[task_id].schedule_count;
    return 1;
}

int task_get_stats(struct task_stats *stats) {
    if (!initialized || stats == NULL) {
        return 0;
    }

    stats->current_task = current_task;
    stats->active_tasks = 0U;
    stats->ready_tasks = 0U;
    stats->finished_tasks = 0U;

    for (task_id_t task_id = 0U; task_id < TASK_MAX_COUNT; ++task_id) {
        enum task_state state = tasks[task_id].state;

        if (state == TASK_STATE_RUNNING || state == TASK_STATE_READY) {
            ++stats->active_tasks;
        }
        if (state == TASK_STATE_READY) {
            ++stats->ready_tasks;
        }
        if (state == TASK_STATE_FINISHED) {
            ++stats->finished_tasks;
        }
    }

    stats->created_tasks = created_tasks;
    stats->context_switches = context_switches;
    stats->yield_calls = yield_calls;
    stats->exited_tasks = exited_tasks;
    stats->reaped_tasks = reaped_tasks;
    stats->stack_errors = stack_errors;
    return 1;
}
