#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"
#include "../include/fs.h"
#include "../include/uart.h"

#ifdef BENCHMARK_MODE
#include "../include/scheduler.h"
#include "../include/sbi.h"
#endif

#define RAM_END 0x88000000UL

#ifdef BENCHMARK_MODE

#define BENCHMARK_TIMER_INTERVAL 10000000UL

static struct process benchmark_process_1;
static struct process benchmark_process_2;

#else

static struct process shell_process;

#endif

void kernel_main(
    unsigned long hart_id,
    void *dtb)
{
    (void)hart_id;
    (void)dtb;

    uart_puts(
        "Mini-RVOS booting...\n"
    );

    pmm_init(RAM_END);

#ifdef BENCHMARK_MODE

    uart_puts(
        "[BENCH] benchmark mode\n"
    );

    if (process_create(
            &benchmark_process_1,
            1) != 0) {

        uart_puts(
            "[FAIL] benchmark process 1 creation\n"
        );

        for (;;) {
        }
    }

    if (process_create(
            &benchmark_process_2,
            2) != 0) {

        uart_puts(
            "[FAIL] benchmark process 2 creation\n"
        );

        for (;;) {
        }
    }

    if (scheduler_init(
            &benchmark_process_1,
            &benchmark_process_2) != 0) {

        uart_puts(
            "[FAIL] benchmark scheduler initialization\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] benchmark processes created\n"
    );

    trap_init();

    sbi_set_timer(
        riscv_read_time() +
        BENCHMARK_TIMER_INTERVAL
    );

    riscv_enable_timer_interrupt();
    riscv_enable_interrupts();

    uart_puts(
        "[OK] entering benchmark scheduler\n"
    );

    scheduler_start();

#else

    if (fs_init() != 0) {
        uart_puts(
            "[FAIL] filesystem initialization\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] filesystem initialized\n"
    );

    if (process_create(
            &shell_process,
            1) != 0) {

        uart_puts(
            "[FAIL] shell process creation\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] shell process created\n"
    );

    trap_init();

    riscv_disable_timer_interrupt();

    uart_puts(
        "[OK] entering user shell\n"
    );

    process_start(
        &shell_process
    );

#endif
}
