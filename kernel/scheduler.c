#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/riscv.h"

#ifdef BENCHMARK_MODE
#include "../include/research.h"
#include "../include/benchmark.h"
#endif

void uart_puts(const char *s);

#define PROCESS_COUNT 2
#define TEST_SWITCHES 6

static struct process *processes[PROCESS_COUNT];

static long current_index = -1;
static unsigned long switch_count;

int scheduler_init(
    struct process *first,
    struct process *second)
{
    if (first == 0 ||
        second == 0) {
        return -1;
    }

    processes[0] = first;
    processes[1] = second;

    current_index = -1;
    switch_count = 0;

    return 0;
}

void scheduler_start(void)
{
    current_index = 0;

#ifdef BENCHMARK_MODE

    /*
     * Initial address-space activation is not part of the
     * measured context-switch sequence.
     */
    process_activate(processes[0]);

    research_reset();

    trap_resume(
        processes[0]->frame
    );

#else

    process_start(processes[0]);

#endif
}

struct trap_frame *scheduler_on_timer(
    struct trap_frame *frame)
{
    if (current_index < 0 ||
        current_index >= PROCESS_COUNT) {
        return frame;
    }

    struct process *previous =
        processes[current_index];

    previous->frame = frame;

    long next_index =
        (current_index + 1) %
        PROCESS_COUNT;

    struct process *next =
        processes[next_index];

    current_index = next_index;
    switch_count++;

#ifdef BENCHMARK_MODE

    research_record_context_switch();

#endif

    process_activate(next);

#ifdef BENCHMARK_MODE

    if (switch_count == 1) {
        uart_puts(
            "[OK] benchmark preemption active\n"
        );
    }

    if (switch_count ==
        BENCHMARK_SWITCHES) {

        uart_puts(
            "[OK] benchmark complete\n"
        );

        research_print_summary();

        riscv_disable_timer_interrupt();
    }

#else

    if (switch_count == 1) {
        uart_puts(
            "[OK] process preemption active\n"
        );
    }

    if (next->pid == 1) {
        uart_puts(
            "[switch -> process 1]\n"
        );
    } else {
        uart_puts(
            "[switch -> process 2]\n"
        );
    }

    if (switch_count == TEST_SWITCHES) {
        if (process_syscall_complete(
                processes[0]) &&
            process_syscall_complete(
                processes[1])) {

            uart_puts(
                "[OK] both processes executed\n"
            );
            uart_puts(
                "[OK] address space switching\n"
            );
            uart_puts(
                "[OK] process round robin\n"
            );
        } else {
            uart_puts(
                "[FAIL] process execution\n"
            );
        }

        riscv_disable_timer_interrupt();
    }

#endif

    return next->frame;
}
