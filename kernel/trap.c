#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"
#include "../include/scheduler.h"

void uart_puts(const char *s);

#define SCAUSE_INTERRUPT          (1UL << 63)

#define SCAUSE_USER_ECALL         8UL
#define SCAUSE_SUPERVISOR_TIMER   5UL
#define SCAUSE_STORE_PAGE_FAULT   15UL

#define SYS_TEST                  1UL
#define SYS_TEST_RETURN           2UL

#define TIMER_INTERVAL            10000000UL

extern void trap_entry(void);

static volatile unsigned long timer_ticks;

void trap_init(void)
{
    riscv_write_stvec((unsigned long)trap_entry);
}

struct trap_frame *trap_handler(
    struct trap_frame *frame)
{
    unsigned long cause =
        riscv_read_scause();

    unsigned long code =
        cause & ~SCAUSE_INTERRUPT;

    if (cause & SCAUSE_INTERRUPT) {
        if (code == SCAUSE_SUPERVISOR_TIMER) {
            timer_ticks++;

            sbi_set_timer(
                riscv_read_time() +
                TIMER_INTERVAL
            );

            return scheduler_on_timer(frame);
        }

        uart_puts("[FAIL] unexpected interrupt\n");

        for (;;) {
        }
    }

    if (code == SCAUSE_USER_ECALL) {
        /*
         * SPP == 0 means the trap originated
         * from U-mode.
         */
        if (frame->sstatus & SSTATUS_SPP) {
            uart_puts("[FAIL] ecall not from U-mode\n");

            for (;;) {
            }
        }

        frame->sepc += 4;

        if (frame->a7 == SYS_TEST) {
            uart_puts("[OK] trap from U-mode\n");
            uart_puts("[OK] user ecall received\n");

            frame->a0 = 0;

            return frame;
        }

        if (frame->a7 == SYS_TEST_RETURN) {
            uart_puts("[OK] returned to U-mode after syscall\n");
            uart_puts("[OK] user mode syscall test complete\n");

            frame->a0 = 0;

            return frame;
        }

        uart_puts("[FAIL] unknown syscall\n");

        for (;;) {
        }
    }

    if (code == SCAUSE_STORE_PAGE_FAULT) {
        uart_puts("[OK] store page fault caught\n");

        frame->sepc += 4;

        return frame;
    }

    uart_puts("[FAIL] unexpected exception\n");

    for (;;) {
    }
}

unsigned long trap_get_timer_ticks(void)
{
    return timer_ticks;
}
