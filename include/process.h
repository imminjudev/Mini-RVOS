#ifndef PROCESS_H
#define PROCESS_H

#include "vm.h"
#include "trap.h"

struct process {
    unsigned long pid;

    pagetable_t pagetable;

    void *kernel_stack;
    void *user_stack;

    struct trap_frame *frame;
};

int process_create(
    struct process *process,
    unsigned long pid
);

void process_start(
    struct process *process
) __attribute__((noreturn));

unsigned long process_current_pid(void);

pagetable_t process_current_pagetable(void);

int process_has_private_user_memory(
    struct process *process
);

#endif
