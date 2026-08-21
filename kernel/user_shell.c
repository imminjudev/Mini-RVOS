#include "../include/syscall.h"

#define USER_TEXT \
    __attribute__((section(".user.text")))

#define USER_RODATA \
    __attribute__((section(".user.rodata")))


/*
 * Shell strings
 */

static const char banner[] USER_RODATA =
    "\nMini-RVOS shell\n"
    "Type 'help' for commands.\n\n";

static const char prompt[] USER_RODATA =
    "$ ";

static const char help_command[] USER_RODATA =
    "help";

static const char pid_command[] USER_RODATA =
    "pid";

static const char echo_command[] USER_RODATA =
    "echo";

static const char echo_prefix[] USER_RODATA =
    "echo ";

static const char cat_command[] USER_RODATA =
    "cat";

static const char cat_prefix[] USER_RODATA =
    "cat ";

static const char exit_command[] USER_RODATA =
    "exit";

static const char help_text[] USER_RODATA =
    "commands: help pid echo cat exit\n";

static const char unknown_text[] USER_RODATA =
    "unknown command\n";

static const char cat_usage[] USER_RODATA =
    "usage: cat <path>\n";

static const char cat_error[] USER_RODATA =
    "cat: file not found\n";

static const char newline[] USER_RODATA =
    "\n";


/*
 * Assembly syscall entry.
 *
 * C:
 *
 * user_syscall3(
 *     syscall_number,
 *     arg0,
 *     arg1,
 *     arg2
 * )
 *
 * user_syscall.S가 실제 ecall ABI로 변환한다.
 */
extern long user_syscall3(
    unsigned long number,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2
);


/*
 * Minimal user-space string functions.
 */

static USER_TEXT unsigned long string_length(
    const char *s)
{
    unsigned long length = 0;

    while (s[length] != '\0') {
        length++;
    }

    return length;
}


static USER_TEXT int string_equal(
    const char *a,
    const char *b)
{
    while (*a != '\0' &&
           *b != '\0') {

        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}


static USER_TEXT int string_starts_with(
    const char *text,
    const char *prefix)
{
    while (*prefix != '\0') {
        if (*text != *prefix) {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}


/*
 * User-space syscall wrappers.
 */

static USER_TEXT long user_write(
    int fd,
    const char *buffer,
    unsigned long length)
{
    return user_syscall3(
        SYS_WRITE,
        (unsigned long)fd,
        (unsigned long)buffer,
        length
    );
}


static USER_TEXT long user_read(
    int fd,
    char *buffer,
    unsigned long length)
{
    return user_syscall3(
        SYS_READ,
        (unsigned long)fd,
        (unsigned long)buffer,
        length
    );
}


static USER_TEXT long user_open(
    const char *path)
{
    return user_syscall3(
        SYS_OPEN,
        (unsigned long)path,
        0,
        0
    );
}


static USER_TEXT long user_close(
    int fd)
{
    return user_syscall3(
        SYS_CLOSE,
        (unsigned long)fd,
        0,
        0
    );
}


static USER_TEXT unsigned long user_getpid(void)
{
    return (unsigned long)
        user_syscall3(
            SYS_GETPID,
            0,
            0,
            0
        );
}


static USER_TEXT void user_exit(
    unsigned long status)
{
    user_syscall3(
        SYS_EXIT,
        status,
        0,
        0
    );

    /*
     * SYS_EXIT은 kernel에서 돌아오지 않아야 한다.
     * 혹시 돌아오더라도 실행을 계속하지 않는다.
     */
    for (;;) {
    }
}


/*
 * Simple stdout helper.
 */

static USER_TEXT void write_text(
    const char *text)
{
    user_write(
        1,
        text,
        string_length(text)
    );
}


/*
 * pid command
 */

static USER_TEXT void command_pid(void)
{
    unsigned long value =
        user_getpid();

    char buffer[32];

    unsigned long position = 0;

    if (value == 0) {
        buffer[position++] = '0';
    } else {
        char reverse[20];

        unsigned long count = 0;

        /*
         * Decimal digits in reverse order.
         */
        while (value > 0) {
            unsigned long digit =
                value % 10;

            reverse[count++] =
                (char)('0' + digit);

            value /= 10;
        }

        /*
         * Reverse them back.
         */
        while (count > 0) {
            buffer[position++] =
                reverse[--count];
        }
    }

    buffer[position++] = '\n';

    user_write(
        1,
        buffer,
        position
    );
}


/*
 * cat <path>
 */

static USER_TEXT void command_cat(
    const char *path)
{
    if (*path == '\0') {
        write_text(cat_usage);
        return;
    }

    long fd =
        user_open(path);

    if (fd < 0) {
        write_text(cat_error);
        return;
    }

    char buffer[128];

    for (;;) {
        long count =
            user_read(
                (int)fd,
                buffer,
                sizeof(buffer)
            );

        if (count < 0) {
            write_text(cat_error);
            break;
        }

        if (count == 0) {
            break;
        }

        user_write(
            1,
            buffer,
            (unsigned long)count
        );
    }

    user_close((int)fd);
}


/*
 * User-space shell entry.
 */

USER_TEXT void user_shell_main(void)
{
    char command[128];

    write_text(banner);

    for (;;) {
        write_text(prompt);

        /*
         * stdin:
         *
         * read(
         *     0,
         *     command,
         *     127
         * )
         */
        long length =
            user_read(
                0,
                command,
                sizeof(command) - 1
            );

        if (length < 0) {
            continue;
        }

        /*
         * Make input a C string.
         */
        command[length] = '\0';

        if (length == 0) {
            continue;
        }


        /*
         * help
         */
        if (string_equal(
                command,
                help_command)) {

            write_text(help_text);

            continue;
        }


        /*
         * pid
         */
        if (string_equal(
                command,
                pid_command)) {

            command_pid();

            continue;
        }


        /*
         * exit
         */
        if (string_equal(
                command,
                exit_command)) {

            user_exit(0);
        }


        /*
         * echo
         */
        if (string_equal(
                command,
                echo_command)) {

            write_text(newline);

            continue;
        }


        /*
         * echo <text>
         */
        if (string_starts_with(
                command,
                echo_prefix)) {

            write_text(
                command +
                sizeof("echo ") - 1
            );

            write_text(newline);

            continue;
        }


        /*
         * cat without path.
         */
        if (string_equal(
                command,
                cat_command)) {

            write_text(cat_usage);

            continue;
        }


        /*
         * cat <path>
         */
        if (string_starts_with(
                command,
                cat_prefix)) {

            command_cat(
                command +
                sizeof("cat ") - 1
            );

            continue;
        }


        /*
         * Unknown command.
         */
        write_text(
            unknown_text
        );
    }
}
