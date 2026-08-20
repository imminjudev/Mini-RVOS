#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"
#include "../include/scheduler.h"
#include "../include/process.h"

void uart_putc(char c);
void uart_puts(const char *s);

#define SCAUSE_INTERRUPT          (1UL << 63)

#define SCAUSE_USER_ECALL         8UL
#define SCAUSE_SUPERVISOR_TIMER   5UL
#define SCAUSE_STORE_PAGE_FAULT   15UL

#define SYS_WRITE                 1UL
#define SYS_DONE                  2UL
#define SYS_FAIL                  3UL
#define SYS_GETPID                4UL

#define TIMER_INTERVAL            10000000UL
#define MAX_WRITE_LENGTH          256UL

extern void trap_entry(void);

static volatile unsigned long timer_ticks;

void trap_init(void)
{
    riscv_write_stvec((unsigned long)trap_entry);
}

static long sys_write(
    unsigned long fd,
    const char *buffer,
    unsigned long length)
{
    if (fd != 1) {
        return -1;
    }

    if (buffer == 0 ||
        length > MAX_WRITE_LENGTH) {
        return -1;
    }

    riscv_enable_user_memory_access();

    for (unsigned long i = 0;
         i < length;
         i++) {
        uart_putc(buffer[i]);
    }

    riscv_disable_user_memory_access();

    return (long)length;
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
        if (frame->sstatus & SSTATUS_SPP) {
            uart_puts(
                "[FAIL] syscall not from U-mode\n"
            );

            for (;;) {
            }
        }

        frame->sepc += 4;

        if (frame->a7 == SYS_WRITE) {
            long result = sys_write(
                frame->a0,
                (const char *)frame->a1,
                frame->a2
            );

            frame->a0 =
                (unsigned long)result;

            return frame;
        }

        if (frame->a7 == SYS_GETPID) {
            frame->a0 =
                process_current_pid();

            return frame;
        }

        if (frame->a7 == SYS_DONE) {
            process_mark_syscall_complete();

            uart_puts(
                "[OK] process syscall path\n"
            );

            frame->a0 = 0;

            return frame;
        }

        if (frame->a7 == SYS_FAIL) {
            uart_puts(
                "[FAIL] process user test\n"
            );

            for (;;) {
            }
        }

        uart_puts("[FAIL] unknown syscall\n");

        for (;;) {
        }
    }

    if (code == SCAUSE_STORE_PAGE_FAULT) {
        uart_puts(
            "[OK] store page fault caught\n"
        );

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
