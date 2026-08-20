#ifndef SCHEDULER_H
#define SCHEDULER_H

struct trap_frame;

int scheduler_init(void);

void scheduler_start(void)
    __attribute__((noreturn));

struct trap_frame *scheduler_on_timer(
    struct trap_frame *frame
);

#endif
