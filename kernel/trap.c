#include "../include/trap.h"
#include "../include/riscv.h"

void uart_puts(const char *s);

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_STORE_PAGE_FAULT 15UL

extern void trap_entry(void);

static volatile unsigned long last_cause;
static volatile unsigned long last_value;

void trap_init(void)
{
    riscv_write_stvec((unsigned long)trap_entry);
}

void trap_handler(void)
{
    unsigned long cause = riscv_read_scause();
    unsigned long value = riscv_read_stval();

    last_cause = cause;
    last_value = value;

    if (cause & SCAUSE_INTERRUPT) {
        uart_puts("[FAIL] unexpected interrupt\n");

        for (;;) {
        }
    }

    if (cause == SCAUSE_STORE_PAGE_FAULT) {
        uart_puts("[OK] store page fault caught\n");

        /*
         * trigger_store_page_fault()의 faulting sw는
         * .option norvc로 4-byte instruction임.
         */
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
