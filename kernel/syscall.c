#include "../include/syscall.h"
#include "../include/trap.h"
#include "../include/riscv.h"
#include "../include/process.h"
#include "../include/vm.h"
#include "../include/fs.h"
#include "../include/uart.h"

#define MAX_WRITE_LENGTH 256UL
#define MAX_READ_LENGTH  256UL


static int user_readable(
    const void *pointer,
    unsigned long length)
{
    return vm_user_range_valid(
        process_current_pagetable(),
        (unsigned long)pointer,
        length,
        PTE_R
    );
}


static int user_writable(
    void *pointer,
    unsigned long length)
{
    return vm_user_range_valid(
        process_current_pagetable(),
        (unsigned long)pointer,
        length,
        PTE_W
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

    /*
     * Kernel이 user buffer를 읽으므로
     * user-readable page여야 한다.
     */
    if (!user_readable(
            buffer,
            length)) {

        return -1;
    }

    riscv_enable_user_memory_access();

    long result;

    if (fd == 1) {
        for (unsigned long i = 0;
             i < length;
             i++) {

            uart_putc(
                buffer[i]
            );
        }

        result =
            (long)length;

    } else {
        result =
            fs_write(
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

    /*
     * 문자열은 길이를 미리 모르므로
     * 한 byte씩 mapping을 확인한다.
     */
    riscv_enable_user_memory_access();

    for (unsigned long i = 0;
         i < FS_NAME_MAX;
         i++) {

        if (!user_readable(
                user_path + i,
                1)) {

            riscv_disable_user_memory_access();

            return -1;
        }

        char c =
            user_path[i];

        kernel_path[i] = c;

        if (c == '\0') {
            riscv_disable_user_memory_access();

            return 0;
        }
    }

    riscv_disable_user_memory_access();

    /*
     * FS_NAME_MAX 안에 null terminator가
     * 없으면 잘못된 path로 처리.
     */
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

    /*
     * Kernel이 user buffer에 입력을 쓰므로
     * writable user page여야 한다.
     */
    if (!user_writable(
            buffer,
            length)) {

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
         * Printable ASCII
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

        uart_putc(
            (char)c
        );
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
     * File data를 user buffer에 쓰므로
     * writable page여야 한다.
     */
    if (!user_writable(
            user_buffer,
            length)) {

        return -1;
    }

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


static void sys_exit(
    unsigned long status)
{
    (void)status;

    uart_puts(
        "\nMini-RVOS shell exited.\n"
    );

    riscv_disable_timer_interrupt();

    for (;;) {
        riscv_wfi();
    }
}


struct trap_frame *syscall_handle(
    struct trap_frame *frame)
{
    /*
     * ecall 다음 instruction으로 이동.
     */
    frame->sepc += 4;

    switch (frame->a7) {

    case SYS_WRITE:
        frame->a0 =
            (unsigned long)
            sys_write(
                frame->a0,
                (const char *)
                frame->a1,
                frame->a2
            );

        return frame;


    case SYS_GETPID:
        frame->a0 =
            process_current_pid();

        return frame;


    case SYS_OPEN:
        frame->a0 =
            (unsigned long)
            sys_open(
                (const char *)
                frame->a0
            );

        return frame;


    case SYS_READ:
        frame->a0 =
            (unsigned long)
            sys_read(
                (int)frame->a0,
                (void *)
                frame->a1,
                frame->a2
            );

        return frame;


    case SYS_CREATE:
        frame->a0 =
            (unsigned long)
            sys_create(
                (const char *)
                frame->a0
            );

        return frame;


    case SYS_CLOSE:
        frame->a0 =
            (unsigned long)
            sys_close(
                (int)frame->a0
            );

        return frame;


    case SYS_EXIT:
        sys_exit(
            frame->a0
        );


    default:
        uart_puts(
            "[FAIL] unknown syscall\n"
        );

        for (;;) {
        }
    }
}
