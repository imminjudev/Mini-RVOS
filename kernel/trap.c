#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"

void uart_puts(const char *s);

#define SCAUSE_INTERRUPT              (1UL << 63)
#define SCAUSE_SUPERVISOR_TIMER       5UL
#define SCAUSE_STORE_PAGE_FAULT       15UL

#define TIMER_INTERVAL 10000000UL

extern void trap_entry(void);

static volatile unsigned long last_cause;
static volatile unsigned long last_value;
static volatile unsigned long timer_ticks;

void trap_init(void)
{
    riscv_write_stvec((unsigned long)trap_entry);
}

void trap_handler(void)
{
    unsigned long cause = riscv_read_scause();
    unsigned long code = cause & ~(1UL << 63);

    last_cause = cause;
    last_value = riscv_read_stval();

    if (cause & SCAUSE_INTERRUPT) {
        if (code == SCAUSE_SUPERVISOR_TIMER) {
            timer_ticks++;

            sbi_set_timer(
                riscv_read_time() + TIMER_INTERVAL
            );

            if (timer_ticks == 1) {
                uart_puts("[OK] timer interrupt caught\n");
            }

            if (timer_ticks == 3) {
                uart_puts("[OK] repeated timer interrupts\n");
                riscv_disable_timer_interrupt();
            }

            return;
        }

        uart_puts("[FAIL] unexpected interrupt\n");

        for (;;) {
        }
    }

    if (code == SCAUSE_STORE_PAGE_FAULT) {
        uart_puts("[OK] store page fault caught\n");

        riscv_write_sepc(riscv_read_sepc() + 4);

        return;
    }

    uart_puts("[FAIL] unexpected exception\n");

    for (;;) {
    }
}

unsigned long trap_get_last_cause(void)
{
    return last_cause;
}

unsigned long trap_get_last_value(void)
{
    return last_value;
}

unsigned long trap_get_timer_ticks(void)
{
    return timer_ticks;
}
