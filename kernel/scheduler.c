#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/riscv.h"

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

    process_start(processes[0]);
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

    /*
     * 핵심:
     * 여기서 satp까지 다음 process의
     * page table로 교체된다.
     */
    process_activate(next);

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

    return next->frame;
}
