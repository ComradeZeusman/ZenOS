#include "task.h"
#include "heap.h"
#include "terminal.h"
#include "timer.h"

/* ── Scheduler hook variables (read by arch/isr.asm) ────────────────────
 *
 * isr_common_stub checks sched_next_esp after every isr_handler() call.
 * If non-zero it:
 *   1. Saves current ESP into *sched_save_esp  (current task's kernel_esp)
 *   2. Loads sched_next_esp as the new ESP     (next task's saved frame)
 *   3. Clears sched_next_esp back to 0
 * The restore sequence (pop ds, popa, add esp 8, iret) then runs on the
 * new task's stack, resuming that task exactly where it was preempted.
 */
uint32_t  sched_next_esp = 0;     /* new stack pointer to switch to   */
uint32_t *sched_save_esp = NULL;  /* where to store the current ESP   */

/* ── Internal state ─────────────────────────────────────────────────── */

static task_t    tasks[MAX_TASKS];
static int       current_idx   = 0;
static uint32_t  next_pid      = 1;

/*
 * Stack deferred for kfree.
 *
 * A dying task cannot free its own stack because the interrupt frame
 * is still live on it.  task_exit() marks the slot DEAD; task_schedule()
 * captures the stack pointer into stack_to_free *after* arming the switch
 * to another task.  On the following timer tick (now safely on a different
 * stack) the pointer is freed.
 */
static void *stack_to_free = NULL;

/* ── Forward declaration ─────────────────────────────────────────────── */
void task_exit(void);

/* ── Internal helpers ────────────────────────────────────────────────── */

static void print_uint(uint32_t v) {
    char buf[12];
    int  i = 11;
    buf[i] = '\0';
    if (v == 0) { terminal_putchar('0'); return; }
    while (v) { buf[--i] = (char)('0' + v % 10); v /= 10; }
    terminal_writestring(&buf[i]);
}

/* ── Public API ──────────────────────────────────────────────────────── */

void task_init(void) {
    /*
     * Slot 0 — the "kernel" task already running on the boot stack.
     * We do not allocate a stack for it; kernel_esp will be filled in
     * by isr_common_stub the first time this task is preempted.
     */
    tasks[0].kernel_esp = 0;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].pid        = next_pid++;
    tasks[0].stack      = NULL;     /* boot stack – do not free */
    tasks[0].name       = "kernel";

    current_idx = 0;

    /* Hook the preemptive scheduler into the PIT IRQ */
    timer_register_callback(task_schedule);
}

uint32_t task_create(const char *name, task_entry_t entry) {
    /* Find a free slot (slot 0 is reserved for the kernel task) */
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) { slot = i; break; }
    }
    if (slot == -1) return 0;

    uint8_t *stack = (uint8_t *)kmalloc(TASK_STACK_SIZE);
    if (!stack) return 0;

    /*
     * Build a fake "interrupted" stack frame so that the isr_common_stub
     * restore path can start the task exactly like resuming a preempted one.
     *
     * Stack layout (high → low address; each line is one uint32_t):
     *
     *  [ task_exit ]   ← entry()'s implicit return address
     *  [ eflags    ]   ─┐
     *  [ cs        ]    │ iret frame consumed by the CPU IRET instruction
     *  [ eip=entry ]   ─┘
     *  [ err_code  ]   ─┐ discarded by  add esp, 8
     *  [ int_no    ]   ─┘
     *  [ eax       ]   ─┐
     *  [ ecx       ]    │
     *  [ edx       ]    │ restored by POPA
     *  [ ebx       ]    │
     *  [ orig_esp  ]    │ (POPA skips this field)
     *  [ ebp       ]    │
     *  [ esi       ]    │
     *  [ edi       ]   ─┘
     *  [ ds=0x10   ]   ← kernel_esp points here (bottom of registers_t)
     *
     * After iret the CPU jumps to entry() with:
     *   ESP → [ task_exit ]   so entry()'s RET lands in task_exit()
     *   IF=1 (interrupts re-enabled by EFLAGS restore)
     */
    uint32_t *sp = (uint32_t *)(stack + TASK_STACK_SIZE);

    *--sp = (uint32_t)task_exit;   /* entry()'s return address           */
    *--sp = 0x00000202u;           /* EFLAGS: IF=1, reserved bit 1       */
    *--sp = 0x08u;                 /* CS = kernel code segment           */
    *--sp = (uint32_t)entry;       /* EIP = task entry point             */
    *--sp = 0u;                    /* err_code (discarded)               */
    *--sp = 0u;                    /* int_no   (discarded)               */
    *--sp = 0u;                    /* EAX                                */
    *--sp = 0u;                    /* ECX                                */
    *--sp = 0u;                    /* EDX                                */
    *--sp = 0u;                    /* EBX                                */
    *--sp = 0u;                    /* orig ESP (skipped by POPA)         */
    *--sp = 0u;                    /* EBP                                */
    *--sp = 0u;                    /* ESI                                */
    *--sp = 0u;                    /* EDI                                */
    *--sp = 0x10u;                 /* DS = kernel data segment           */

    tasks[slot].kernel_esp = (uint32_t)sp;
    tasks[slot].state      = TASK_READY;
    tasks[slot].pid        = next_pid++;
    tasks[slot].stack      = stack;
    tasks[slot].name       = name;

    return tasks[slot].pid;
}

void task_exit(void) {
    /*
     * Mark this task dead with interrupts briefly disabled so that
     * task_schedule() cannot observe a half-updated state.
     * We then re-enable interrupts and halt; the next timer tick will
     * preempt us and never return here.
     */
    __asm__ volatile("cli");
    tasks[current_idx].state = TASK_DEAD;
    __asm__ volatile("sti");

    for (;;) __asm__ volatile("hlt");
}

void task_schedule(void) {
    /*
     * Safe point to release the stack of a previously exited task.
     * By the time we reach here we are on a *different* task's stack
     * (the switch already happened on the previous tick), so kfree is safe.
     */
    if (stack_to_free) {
        kfree(stack_to_free);
        stack_to_free = NULL;
    }

    /* Round-robin: scan for the next READY slot */
    int next = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_idx + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) { next = idx; break; }
    }

    if (next == -1) return;  /* no other runnable task – keep current */

    /*
     * If the task we are switching away from has died, defer its stack
     * free to the next tick (see note above).  We cannot free it now
     * because the interrupt frame for this very tick is still live on it.
     */
    if (tasks[current_idx].state == TASK_DEAD && tasks[current_idx].stack) {
        stack_to_free              = tasks[current_idx].stack;
        tasks[current_idx].stack   = NULL;
    }

    /*
     * Arm the context-switch hook in isr_common_stub:
     *   sched_save_esp  ← address of current task's kernel_esp field
     *   sched_next_esp  ← saved ESP of the task we are switching to
     *
     * isr_common_stub reads these after isr_handler() returns and
     * performs the actual ESP swap before the POPA / IRET sequence.
     */
    sched_save_esp = &tasks[current_idx].kernel_esp;
    sched_next_esp = tasks[next].kernel_esp;

    if (tasks[current_idx].state == TASK_RUNNING)
        tasks[current_idx].state = TASK_READY;

    tasks[next].state = TASK_RUNNING;
    current_idx       = next;
}

uint32_t task_count(void) {
    uint32_t n = 0;
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_DEAD) n++;
    return n;
}

void task_list(void) {
    terminal_writestring_colored("PID  STATE    NAME\n",
                                 VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_writestring("---  -------  ----\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) continue;
        print_uint(tasks[i].pid);
        terminal_writestring("    ");
        switch (tasks[i].state) {
            case TASK_RUNNING: terminal_writestring("running  "); break;
            case TASK_READY:   terminal_writestring("ready    "); break;
            default:           terminal_writestring("dead     "); break;
        }
        terminal_writestring(tasks[i].name ? tasks[i].name : "?");
        terminal_putchar('\n');
    }
}
