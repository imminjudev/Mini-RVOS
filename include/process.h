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

    int syscall_complete;
};

int process_create(
    struct process *process,
    unsigned long pid
);

void process_activate(
    struct process *process
);

void process_start(
    struct process *process
) __attribute__((noreturn));

unsigned long process_current_pid(void);

pagetable_t process_current_pagetable(void);

int process_has_private_user_memory(
    struct process *process
);

int process_address_spaces_distinct(
    struct process *a,
    struct process *b
);

void process_mark_syscall_complete(void);

int process_syscall_complete(
    struct process *process
);

#endif
