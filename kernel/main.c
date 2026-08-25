#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"
#include "../include/fs.h"
#include "../include/uart.h"

#ifdef BENCHMARK_MODE
#include "../include/scheduler.h"
#include "../include/benchmark.h"
#endif

#define RAM_END 0x88000000UL

#ifdef BENCHMARK_MODE

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

#if BENCHMARK_USE_ASID

    uart_puts(
        "[BENCH] requested switch mode=ASID\n"
    );

#else

    uart_puts(
        "[BENCH] requested switch mode=FULL_FLUSH\n"
    );

#endif

#if BENCHMARK_WORKLOAD_MEMORY

    uart_puts(
        "[BENCH] requested workload=memory\n"
    );

#else

    uart_puts(
        "[BENCH] requested workload=cpu\n"
    );

#endif

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

#if BENCHMARK_WORKLOAD_MEMORY

    unsigned long process_1_working_set =
        vm_translate(
            benchmark_process_1.pagetable,
            BENCHMARK_WORKING_SET_BASE
        );

    unsigned long process_2_working_set =
        vm_translate(
            benchmark_process_2.pagetable,
            BENCHMARK_WORKING_SET_BASE
        );

    if (process_1_working_set == 0 ||
        process_2_working_set == 0 ||
        process_1_working_set ==
        process_2_working_set) {

        uart_puts(
            "[FAIL] benchmark working-set isolation\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] benchmark working sets private\n"
    );

#endif

#if BENCHMARK_USE_ASID

    unsigned long asid_bits =
        vm_detect_asid_bits(
            benchmark_process_1.pagetable
        );

    /*
     * ASIDs 1 and 2 require at least two
     * writable ASID bits.
     */
    if (asid_bits < 2) {
        uart_puts(
            "[FAIL] insufficient hardware ASID support\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] hardware ASID support detected\n"
    );

#endif

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

    /*
     * Keep supervisor interrupts globally disabled while
     * benchmark kernel code is executing.
     *
     * Supervisor timer interrupts remain deliverable after
     * SRET enters U-mode because SIE does not gate
     * supervisor interrupts at a lower privilege level.
     */
    riscv_disable_interrupts();
    riscv_enable_timer_interrupt();

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
