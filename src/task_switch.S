#include "task.h"

.section .text.task_switch, "ax", @progbits
.align 2
.global task_context_switch
.type task_context_switch, %function

task_context_switch:
    stp x19, x20, [x0, #TASK_CONTEXT_X19_OFFSET]
    stp x21, x22, [x0, #TASK_CONTEXT_X21_OFFSET]
    stp x23, x24, [x0, #TASK_CONTEXT_X23_OFFSET]
    stp x25, x26, [x0, #TASK_CONTEXT_X25_OFFSET]
    stp x27, x28, [x0, #TASK_CONTEXT_X27_OFFSET]
    stp x29, x30, [x0, #TASK_CONTEXT_X29_OFFSET]
    mov x2, sp
    str x2, [x0, #TASK_CONTEXT_SP_OFFSET]

    ldp x19, x20, [x1, #TASK_CONTEXT_X19_OFFSET]
    ldp x21, x22, [x1, #TASK_CONTEXT_X21_OFFSET]
    ldp x23, x24, [x1, #TASK_CONTEXT_X23_OFFSET]
    ldp x25, x26, [x1, #TASK_CONTEXT_X25_OFFSET]
    ldp x27, x28, [x1, #TASK_CONTEXT_X27_OFFSET]
    ldp x29, x30, [x1, #TASK_CONTEXT_X29_OFFSET]
    ldr x2, [x1, #TASK_CONTEXT_SP_OFFSET]
    mov sp, x2
    ret

.size task_context_switch, . - task_context_switch
