#ifndef TASK_H
#define TASK_H

#include "kernel_types.h"

/* Maximum number of simultaneous tasks (including the kernel task at slot 0). */
#define MAX_TASKS       16

/* Per-task kernel stack size in bytes. */
#define TASK_STACK_SIZE 8192u

/* ── Task states ─────────────────────────────────────────────────────── */
typedef enum {
    TASK_DEAD    = 0,   /* slot is free / task has exited              */
    TASK_READY   = 1,   /* runnable, waiting for its turn              */
    TASK_RUNNING = 2,   /* currently executing on the CPU              */
} task_state_t;

/* Function signature for a task entry point. */
typedef void (*task_entry_t)(void);

/* ── Task control block ──────────────────────────────────────────────── */
/*
 * When a task is not running its full CPU state is preserved on its
 * own kernel stack.  kernel_esp is the stack pointer to the bottom
 * of that saved state (a registers_t frame as built by isr_common_stub).
 */
typedef struct {
    uint32_t      kernel_esp;  /* saved ESP when task is not running  */
    task_state_t  state;       /* current lifecycle state             */
    uint32_t      pid;         /* unique process identifier (1-based) */
    uint8_t      *stack;       /* kmalloc'd stack base; NULL = kernel */
    const char   *name;        /* human-readable task name            */
} task_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * task_init()
 *
 * Initialise the task subsystem.  Registers slot 0 as the running
 * "kernel" task (the current boot stack) and hooks task_schedule()
 * into the PIT timer callback.
 * Must be called after heap_init() and timer_init().
 */
void task_init(void);

/*
 * task_create(name, entry)
 *
 * Allocate a new task that will call entry() when scheduled.
 * Returns the new task's PID on success, or 0 on failure.
 * The task is immediately placed in the READY state.
 */
uint32_t task_create(const char *name, task_entry_t entry);

/*
 * task_exit()
 *
 * Mark the calling task as DEAD and spin until the next timer tick
 * switches away from it permanently.  Never returns.
 * Automatically called when a task's entry function returns normally.
 */
void task_exit(void);

/*
 * task_schedule()
 *
 * Round-robin scheduler called from the PIT IRQ (interrupt context).
 * Selects the next READY task and arms the context-switch variables
 * read by isr_common_stub after isr_handler returns.
 */
void task_schedule(void);

/*
 * task_list()
 *
 * Print a table of all non-dead tasks to the terminal.
 */
void task_list(void);

/*
 * task_count()
 *
 * Return the number of tasks currently alive (READY or RUNNING).
 */
uint32_t task_count(void);

#endif /* TASK_H */
