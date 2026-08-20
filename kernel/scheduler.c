#include "../include/scheduler.h"
#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"

void uart_puts(const char *s);

#define TASK_COUNT 2
#define TEST_SWITCHES 6

struct task {
    struct trap_frame *frame;
    void *stack;

    volatile unsigned long work;
    unsigned long slices;
};

static struct task tasks[TASK_COUNT];

static long current_task = -1;
static unsigned long switch_count;

static void clear_frame(struct trap_frame *frame)
{
    unsigned long *p = (unsigned long *)frame;

    for (unsigned long i = 0;
         i < sizeof(*frame) / sizeof(unsigned long);
         i++) {
        p[i] = 0;
    }
}

static void task_a(void)
{
    uart_puts("[task A started]\n");

    for (;;) {
        tasks[0].work++;
    }
}

static void task_b(void)
{
    uart_puts("[task B started]\n");

    for (;;) {
        tasks[1].work++;
    }
}

static int task_create(
    unsigned long id,
    void (*entry)(void))
{
    void *stack = page_alloc();

    if (stack == 0) {
        return -1;
    }

    unsigned long stack_top =
        (unsigned long)stack + PAGE_SIZE;

    struct trap_frame *frame =
        (struct trap_frame *)
        (stack_top - sizeof(struct trap_frame));

    clear_frame(frame);

    frame->sp = stack_top;
    frame->sepc = (unsigned long)entry;

    /*
     * Return into Supervisor mode.
     * SPIE makes interrupts enabled after sret.
     */
    frame->sstatus =
        SSTATUS_SPP | SSTATUS_SPIE;

    tasks[id].frame = frame;
    tasks[id].stack = stack;
    tasks[id].work = 0;
    tasks[id].slices = 0;

    return 0;
}

int scheduler_init(void)
{
    current_task = -1;
    switch_count = 0;

    if (task_create(0, task_a) != 0) {
        return -1;
    }

    if (task_create(1, task_b) != 0) {
        return -1;
    }

    return 0;
}

void scheduler_start(void)
{
    current_task = 0;

    trap_resume(tasks[0].frame);

    for (;;) {
    }
}

struct trap_frame *scheduler_on_timer(
    struct trap_frame *frame)
{
    if (current_task < 0 ||
        current_task >= TASK_COUNT) {
        return frame;
    }

    long previous = current_task;

    tasks[previous].frame = frame;
    tasks[previous].slices++;

    long next =
        (previous + 1) % TASK_COUNT;

    current_task = next;

    switch_count++;

    if (switch_count == 1) {
        uart_puts("[OK] timer preemption active\n");
    }

    if (next == 0) {
        uart_puts("[switch -> task A]\n");
    } else {
        uart_puts("[switch -> task B]\n");
    }

    if (switch_count == TEST_SWITCHES) {
        if (tasks[0].work > 0 &&
            tasks[1].work > 0) {

            uart_puts("[OK] both tasks executed\n");
            uart_puts("[OK] preemptive round robin\n");
        } else {
            uart_puts("[FAIL] task execution\n");
        }

        riscv_disable_timer_interrupt();
    }

    return tasks[next].frame;
}
