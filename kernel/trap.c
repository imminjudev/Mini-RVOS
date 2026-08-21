#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"
#include "../include/scheduler.h"
#include "../include/syscall.h"
#include "../include/uart.h"

#define SCAUSE_INTERRUPT          (1UL << 63)

#define SCAUSE_SUPERVISOR_TIMER   5UL
#define SCAUSE_USER_ECALL         8UL
#define SCAUSE_STORE_PAGE_FAULT   15UL

#define TIMER_INTERVAL            10000000UL

extern void trap_entry(void);

static volatile unsigned long timer_ticks;

static void print_hex(
    unsigned long value)
{
    for (int shift = 60;
         shift >= 0;
         shift -= 4) {

        unsigned long digit =
            (value >> shift) & 0xf;

        uart_putc(
            digit < 10
            ? (char)('0' + digit)
            : (char)('a' + digit - 10)
        );
    }
}

void trap_init(void)
{
    riscv_write_stvec(
        (unsigned long)trap_entry
    );
}

struct trap_frame *trap_handler(
    struct trap_frame *frame)
{
    unsigned long cause =
        riscv_read_scause();

    unsigned long code =
        cause & ~SCAUSE_INTERRUPT;

    if (cause & SCAUSE_INTERRUPT) {
        if (code ==
            SCAUSE_SUPERVISOR_TIMER) {

            timer_ticks++;

            sbi_set_timer(
                riscv_read_time() +
                TIMER_INTERVAL
            );

            return scheduler_on_timer(
                frame
            );
        }

        uart_puts(
            "[FAIL] unexpected interrupt\n"
        );

        uart_puts("scause = ");
        print_hex(cause);
        uart_putc('\n');

        for (;;) {
        }
    }

    if (code == SCAUSE_USER_ECALL) {
        if (frame->sstatus &
            SSTATUS_SPP) {

            uart_puts(
                "[FAIL] ecall not from U-mode\n"
            );

            for (;;) {
            }
        }

        return syscall_handle(frame);
    }

    if (code ==
        SCAUSE_STORE_PAGE_FAULT) {

        uart_puts(
            "[OK] store page fault caught\n"
        );

        frame->sepc += 4;

        return frame;
    }

    uart_puts(
        "[FAIL] unexpected exception\n"
    );

    uart_puts("scause = ");
    print_hex(cause);

    uart_puts("\nsepc   = ");
    print_hex(riscv_read_sepc());

    uart_puts("\nstval  = ");
    print_hex(riscv_read_stval());

    uart_putc('\n');

    for (;;) {
    }
}

unsigned long trap_get_timer_ticks(void)
{
    return timer_ticks;
}
