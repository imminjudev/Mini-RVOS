#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"
#include "../include/fs.h"
#include "../include/uart.h"

#define RAM_END 0x88000000UL

static struct process shell_process;

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

    /*
     * Interactive shell checkpoint:
     * scheduler timer는 잠시 사용하지 않는다.
     */
    riscv_disable_timer_interrupt();

    uart_puts(
        "[OK] entering user shell\n"
    );

    process_start(
        &shell_process
    );
}
