#include "../include/memory.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"

void uart_puts(const char *s);

#define RAM_END 0x88000000UL

static struct process init_process;

void kernel_main(
    unsigned long hart_id,
    void *dtb)
{
    (void)hart_id;
    (void)dtb;

    uart_puts("Mini-RVOS booting...\n");

    pmm_init(RAM_END);

    /*
     * process_create()는 user program template을
     * 복사하므로 paging을 켜기 전에 실행.
     */
    if (process_create(
            &init_process,
            1) != 0) {

        uart_puts(
            "[FAIL] process creation\n"
        );

        for (;;) {
        }
    }

    uart_puts("[OK] process created\n");

    if (init_process.pid == 1) {
        uart_puts("[OK] PID assigned\n");
    } else {
        uart_puts("[FAIL] PID\n");

        for (;;) {
        }
    }

    if (process_has_private_user_memory(
            &init_process)) {

        uart_puts(
            "[OK] private user address space\n"
        );
    } else {
        uart_puts(
            "[FAIL] private address space\n"
        );

        for (;;) {
        }
    }

    trap_init();

    /*
     * 이번 checkpoint에서는 scheduler를
     * 아직 process와 연결하지 않는다.
     */
    riscv_disable_timer_interrupt();

    uart_puts("[OK] entering process 1\n");

    process_start(&init_process);
}
