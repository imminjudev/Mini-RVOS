#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_WRITE   1UL
#define SYS_GETPID  4UL
#define SYS_OPEN    5UL
#define SYS_READ    6UL
#define SYS_CREATE  7UL
#define SYS_CLOSE   8UL
#define SYS_EXIT    9UL

struct trap_frame;

struct trap_frame *syscall_handle(
    struct trap_frame *frame
);

#endif
