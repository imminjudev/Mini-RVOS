#include "../include/scheduler.h"
#include "../include/memory.h"

void uart_puts(const char *s);

#define TASK_COUNT 2
#define TARGET_RUNS 3

struct context {
    unsigned long ra;
    unsigned long sp;

    unsigned long s0;
    unsigned long s1;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
};

struct task {
    struct context context;
    void *stack;
    void (*entry)(void);
    unsigned long runs;
};

static struct task tasks[TASK_COUNT];
static struct context scheduler_context;

static long current_task = -1;

extern void context_switch(
    struct context *old_context,
    struct context *new_context
);

static void task_yield(void);

static void task_a(void)
{
    for (;;) {
        uart_puts("[task A]\n");

        tasks[current_task].runs++;

        task_yield();
    }
}

static void task_b(void)
{
    for (;;) {
        uart_puts("[task B]\n");

        tasks[current_task].runs++;

        task_yield();
    }
}

static void task_bootstrap(void)
{
    if (current_task < 0 ||
        current_task >= TASK_COUNT) {

        for (;;) {
        }
    }

    tasks[current_task].entry();

    for (;;) {
    }
}

static void clear_context(struct context *context)
{
    unsigned long *p = (unsigned long *)context;

    for (unsigned long i = 0;
         i < sizeof(*context) / sizeof(unsigned long);
         i++) {
        p[i] = 0;
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

    clear_context(&tasks[id].context);

    tasks[id].stack = stack;
    tasks[id].entry = entry;
    tasks[id].runs = 0;

    tasks[id].context.ra =
        (unsigned long)task_bootstrap;

    tasks[id].context.sp =
        (unsigned long)stack + PAGE_SIZE;

    return 0;
}

int scheduler_init(void)
{
    clear_context(&scheduler_context);

    if (task_create(0, task_a) != 0) {
        return -1;
    }

    if (task_create(1, task_b) != 0) {
        return -1;
    }

    return 0;
}

static void task_yield(void)
{
    long previous = current_task;

    if (tasks[0].runs >= TARGET_RUNS &&
        tasks[1].runs >= TARGET_RUNS) {

        current_task = -1;

        context_switch(
            &tasks[previous].context,
            &scheduler_context
        );

        return;
    }

    long next =
        (previous + 1) % TASK_COUNT;

    current_task = next;

    context_switch(
        &tasks[previous].context,
        &tasks[next].context
    );
}

void scheduler_start(void)
{
    current_task = 0;

    context_switch(
        &scheduler_context,
        &tasks[0].context
    );
}

unsigned long scheduler_task_runs(unsigned long id)
{
    if (id >= TASK_COUNT) {
        return 0;
    }

    return tasks[id].runs;
}
