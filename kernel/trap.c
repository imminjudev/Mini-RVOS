#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/sbi.h"
#include "../include/scheduler.h"
#include "../include/process.h"
#include "../include/fs.h"
#include "../include/uart.h"

#define SCAUSE_INTERRUPT          (1UL << 63)

#define SCAUSE_USER_ECALL         8UL
#define SCAUSE_SUPERVISOR_TIMER   5UL
#define SCAUSE_STORE_PAGE_FAULT   15UL

#define SYS_WRITE                 1UL
#define SYS_DONE                  2UL
#define SYS_FAIL                  3UL
#define SYS_GETPID                4UL
#define SYS_OPEN                  5UL
#define SYS_READ                  6UL
#define SYS_CREATE                7UL
#define SYS_CLOSE                 8UL

#define TIMER_INTERVAL            10000000UL
#define MAX_WRITE_LENGTH          256UL
#define MAX_READ_LENGTH           256UL

extern void trap_entry(void);

static volatile unsigned long timer_ticks;


/*
 * 디버깅용 64-bit hex 출력.
 */
static void print_hex(
    unsigned long value)
{
    for (int shift = 60;
         shift >= 0;
         shift -= 4) {

        unsigned long digit =
            (value >> shift) & 0xf;

        if (digit < 10) {
            uart_putc(
                (char)('0' + digit)
            );
        } else {
            uart_putc(
                (char)(
                    'a' +
                    digit - 10
                )
            );
        }
    }
}


void trap_init(void)
{
    riscv_write_stvec(
        (unsigned long)trap_entry
    );
}


static long sys_write(
    unsigned long fd,
    const char *buffer,
    unsigned long length)
{
    if (buffer == 0 ||
        length > MAX_WRITE_LENGTH) {

        return -1;
    }

    riscv_enable_user_memory_access();

    long result;

    if (fd == 1) {
        for (unsigned long i = 0;
             i < length;
             i++) {

            uart_putc(buffer[i]);
        }

        result =
            (long)length;
    } else {
        result = fs_write(
            process_current_pid(),
            (int)fd,
            buffer,
            length
        );
    }

    riscv_disable_user_memory_access();

    return result;
}


static int copy_user_path(
    const char *user_path,
    char *kernel_path)
{
    if (user_path == 0) {
        return -1;
    }

    riscv_enable_user_memory_access();

    for (unsigned long i = 0;
         i < FS_NAME_MAX;
         i++) {

        char c =
            user_path[i];

        kernel_path[i] = c;

        if (c == '\0') {
            riscv_disable_user_memory_access();

            return 0;
        }
    }

    riscv_disable_user_memory_access();

    return -1;
}


static long sys_open(
    const char *user_path)
{
    char path[FS_NAME_MAX];

    if (copy_user_path(
            user_path,
            path) != 0) {

        return -1;
    }

    return fs_open(
        process_current_pid(),
        path
    );
}


static long sys_create(
    const char *user_path)
{
    char path[FS_NAME_MAX];

    if (copy_user_path(
            user_path,
            path) != 0) {

        return -1;
    }

    if (fs_create(
            path,
            0,
            0) < 0) {

        return -1;
    }

    return fs_open(
        process_current_pid(),
        path
    );
}


static long sys_read_stdin(
    char *buffer,
    unsigned long length)
{
    if (buffer == 0 ||
        length == 0) {

        return -1;
    }

    riscv_enable_user_memory_access();

    unsigned long count = 0;

    for (;;) {
        int c =
            uart_getc();

        /*
         * Enter
         */
        if (c == '\r' ||
            c == '\n') {

            uart_putc('\n');
            break;
        }

        /*
         * Backspace / Delete
         */
        if (c == 8 ||
            c == 127) {

            if (count > 0) {
                count--;

                uart_putc('\b');
                uart_putc(' ');
                uart_putc('\b');
            }

            continue;
        }

        /*
         * Ctrl-D
         */
        if (c == 4) {
            break;
        }

        /*
         * Printable ASCII only.
         */
        if (c < 32 ||
            c > 126) {

            continue;
        }

        if (count >= length) {
            uart_putc('\n');
            break;
        }

        buffer[count++] =
            (char)c;

        /*
         * 입력 echo.
         */
        uart_putc((char)c);
    }

    riscv_disable_user_memory_access();

    return (long)count;
}


static long sys_read(
    int fd,
    void *user_buffer,
    unsigned long length)
{
    if (user_buffer == 0 ||
        length > MAX_READ_LENGTH) {

        return -1;
    }

    /*
     * stdin
     */
    if (fd == 0) {
        return sys_read_stdin(
            (char *)user_buffer,
            length
        );
    }

    /*
     * file read
     */
    riscv_enable_user_memory_access();

    long result =
        fs_read(
            process_current_pid(),
            fd,
            user_buffer,
            length
        );

    riscv_disable_user_memory_access();

    return result;
}


static long sys_close(
    int fd)
{
    return fs_close(
        process_current_pid(),
        fd
    );
}


struct trap_frame *trap_handler(
    struct trap_frame *frame)
{
    unsigned long cause =
        riscv_read_scause();

    unsigned long code =
        cause &
        ~SCAUSE_INTERRUPT;


    /*
     * Interrupt
     */
    if (cause &
        SCAUSE_INTERRUPT) {

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

        uart_puts(
            "scause = "
        );

        print_hex(cause);

        uart_putc('\n');

        for (;;) {
        }
    }


    /*
     * U-mode syscall
     */
    if (code ==
        SCAUSE_USER_ECALL) {

        if (frame->sstatus &
            SSTATUS_SPP) {

            uart_puts(
                "[FAIL] syscall not from U-mode\n"
            );

            for (;;) {
            }
        }

        /*
         * ecall 다음 instruction으로 복귀.
         */
        frame->sepc += 4;


        /*
         * write(fd, buf, len)
         */
        if (frame->a7 ==
            SYS_WRITE) {

            frame->a0 =
                (unsigned long)
                sys_write(
                    frame->a0,
                    (const char *)
                    frame->a1,
                    frame->a2
                );

            return frame;
        }


        /*
         * getpid()
         */
        if (frame->a7 ==
            SYS_GETPID) {

            frame->a0 =
                process_current_pid();

            return frame;
        }


        /*
         * open(path)
         */
        if (frame->a7 ==
            SYS_OPEN) {

            frame->a0 =
                (unsigned long)
                sys_open(
                    (const char *)
                    frame->a0
                );

            return frame;
        }


        /*
         * read(fd, buf, len)
         */
        if (frame->a7 ==
            SYS_READ) {

            frame->a0 =
                (unsigned long)
                sys_read(
                    (int)frame->a0,
                    (void *)
                    frame->a1,
                    frame->a2
                );

            return frame;
        }


        /*
         * create(path)
         */
        if (frame->a7 ==
            SYS_CREATE) {

            frame->a0 =
                (unsigned long)
                sys_create(
                    (const char *)
                    frame->a0
                );

            return frame;
        }


        /*
         * close(fd)
         */
        if (frame->a7 ==
            SYS_CLOSE) {

            frame->a0 =
                (unsigned long)
                sys_close(
                    (int)frame->a0
                );

            return frame;
        }


        /*
         * 테스트 완료용 syscall.
         */
        if (frame->a7 ==
            SYS_DONE) {

            process_mark_syscall_complete();

            frame->a0 = 0;

            return frame;
        }


        if (frame->a7 ==
            SYS_FAIL) {

            uart_puts(
                "[FAIL] user process\n"
            );

            for (;;) {
            }
        }


        uart_puts(
            "[FAIL] unknown syscall\n"
        );

        uart_puts(
            "syscall = "
        );

        print_hex(
            frame->a7
        );

        uart_putc('\n');

        for (;;) {
        }
    }


    /*
     * 기존 store page fault 테스트.
     */
    if (code ==
        SCAUSE_STORE_PAGE_FAULT) {

        uart_puts(
            "[OK] store page fault caught\n"
        );

        frame->sepc += 4;

        return frame;
    }


    /*
     * 예상하지 못한 exception.
     *
     * 원인 분석용 CSR 출력.
     */
    uart_puts(
        "[FAIL] unexpected exception\n"
    );

    uart_puts(
        "scause = "
    );

    print_hex(cause);

    uart_puts(
        "\nsepc   = "
    );

    print_hex(
        riscv_read_sepc()
    );

    uart_puts(
        "\nstval  = "
    );

    print_hex(
        riscv_read_stval()
    );

    uart_putc('\n');

    for (;;) {
    }
}


unsigned long trap_get_timer_ticks(void)
{
    return timer_ticks;
}
