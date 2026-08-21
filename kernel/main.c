#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/sbi.h"
#include "../include/fs.h"

void uart_puts(const char *s);

#define RAM_END        0x88000000UL
#define TIMER_INTERVAL 10000000UL

static struct process process_one;
static struct process process_two;

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

    if (fs_init() != 0 ||
        fs_inode_count() != 1) {

        uart_puts(
            "[FAIL] filesystem initialization\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] in-memory filesystem initialized\n"
    );

    if (process_create(
            &process_one,
            1) != 0) {

        uart_puts(
            "[FAIL] process 1 creation\n"
        );

        for (;;) {
        }
    }

    if (process_create(
            &process_two,
            2) != 0) {

        uart_puts(
            "[FAIL] process 2 creation\n"
        );

        for (;;) {
        }
    }

    uart_puts(
        "[OK] two processes created\n"
    );

    if (!process_address_spaces_distinct(
            &process_one,
            &process_two)) {

        uart_puts(
            "[FAIL] process isolation\n"
        );

        for (;;) {
        }
    }

    if (scheduler_init(
            &process_one,
            &process_two) != 0) {

        uart_puts(
            "[FAIL] process scheduler\n"
        );

        for (;;) {
        }
    }

    trap_init();

    riscv_enable_timer_interrupt();

    sbi_set_timer(
        riscv_read_time() +
        TIMER_INTERVAL
    );

    uart_puts(
        "[OK] filesystem test ready\n"
    );

    scheduler_start();
}
