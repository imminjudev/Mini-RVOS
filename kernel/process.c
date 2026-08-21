#include "../include/process.h"
#include "../include/memory.h"
#include "../include/riscv.h"

#define RAM_END          0x88000000UL
#define UART0            0x10000000UL

#define USER_STACK_BASE  0x40000000UL
#define USER_STACK_TOP   (USER_STACK_BASE + PAGE_SIZE)

extern char __text_start[];
extern char __text_end[];

extern char __user_text_start[];
extern char __user_text_end[];

extern char __user_rodata_start[];
extern char __user_rodata_end[];

extern char __rodata_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __data_end[];

extern char __kernel_end[];

extern void user_entry(void);

static struct process *current_process;

static void page_zero(void *ptr)
{
    unsigned char *p =
        (unsigned char *)ptr;

    for (unsigned long i = 0;
         i < PAGE_SIZE;
         i++) {
        p[i] = 0;
    }
}

static void page_copy(
    void *destination,
    const void *source)
{
    unsigned char *dst =
        (unsigned char *)destination;

    const unsigned char *src =
        (const unsigned char *)source;

    for (unsigned long i = 0;
         i < PAGE_SIZE;
         i++) {
        dst[i] = src[i];
    }
}

static void clear_frame(
    struct trap_frame *frame)
{
    unsigned char *p =
        (unsigned char *)frame;

    for (unsigned long i = 0;
         i < sizeof(*frame);
         i++) {
        p[i] = 0;
    }
}

static int map_region(
    pagetable_t root,
    unsigned long start,
    unsigned long end,
    unsigned long flags)
{
    if (end <= start) {
        return 0;
    }

    return vm_map_range(
        root,
        start,
        start,
        end - start,
        flags
    );
}

static int copy_user_region(
    pagetable_t root,
    unsigned long start,
    unsigned long end,
    unsigned long flags)
{
    for (unsigned long va = start;
         va < end;
         va += PAGE_SIZE) {

        void *page = page_alloc();

        if (page == 0) {
            return -1;
        }

        page_copy(
            page,
            (const void *)va
        );

        if (vm_map_page(
                root,
                va,
                (unsigned long)page,
                flags) != 0) {
            return -1;
        }
    }

    return 0;
}

static int map_kernel(
    pagetable_t root)
{
    if (map_region(
            root,
            (unsigned long)__text_start,
            (unsigned long)__text_end,
            PTE_R | PTE_X | PTE_A) != 0) {
        return -1;
    }

    if (map_region(
            root,
            (unsigned long)__rodata_start,
            (unsigned long)__rodata_end,
            PTE_R | PTE_A) != 0) {
        return -1;
    }

    if (map_region(
            root,
            (unsigned long)__data_start,
            (unsigned long)__data_end,
            PTE_R | PTE_W |
            PTE_A | PTE_D) != 0) {
        return -1;
    }

    if (map_region(
            root,
            (unsigned long)__kernel_end,
            RAM_END,
            PTE_R | PTE_W |
            PTE_A | PTE_D) != 0) {
        return -1;
    }

    if (vm_map_page(
            root,
            UART0,
            UART0,
            PTE_R | PTE_W |
            PTE_A | PTE_D) != 0) {
        return -1;
    }

    return 0;
}

int process_create(
    struct process *process,
    unsigned long pid)
{
    process->pid = pid;
    process->pagetable = 0;
    process->kernel_stack = 0;
    process->user_stack = 0;
    process->frame = 0;
    process->syscall_complete = 0;

    pagetable_t root = vm_create();

    if (root == 0) {
        return -1;
    }

    if (map_kernel(root) != 0) {
        return -1;
    }

    if (copy_user_region(
            root,
            (unsigned long)__user_text_start,
            (unsigned long)__user_text_end,
            PTE_R | PTE_X |
            PTE_U | PTE_A) != 0) {
        return -1;
    }

    if (copy_user_region(
            root,
            (unsigned long)__user_rodata_start,
            (unsigned long)__user_rodata_end,
            PTE_R | PTE_U |
            PTE_A) != 0) {
        return -1;
    }

    void *user_stack = page_alloc();
    void *kernel_stack = page_alloc();

    if (user_stack == 0 ||
        kernel_stack == 0) {
        return -1;
    }

    page_zero(user_stack);
    page_zero(kernel_stack);

    if (vm_map_page(
            root,
            USER_STACK_BASE,
            (unsigned long)user_stack,
            PTE_R | PTE_W |
            PTE_U | PTE_A | PTE_D) != 0) {
        return -1;
    }

    unsigned long kernel_stack_top =
        (unsigned long)kernel_stack +
        PAGE_SIZE;

    struct trap_frame *frame =
        (struct trap_frame *)
        (kernel_stack_top -
         TRAP_FRAME_SIZE);

    clear_frame(frame);

    frame->sp = USER_STACK_TOP;

    frame->sepc =
        (unsigned long)user_entry;

    frame->sstatus = 0;

    process->pagetable = root;
    process->kernel_stack = kernel_stack;
    process->user_stack = user_stack;
    process->frame = frame;

    return 0;
}

void process_activate(
    struct process *process)
{
    current_process = process;

    vm_enable(process->pagetable);
}

void process_start(
    struct process *process)
{
    process_activate(process);

    trap_resume(process->frame);
}

unsigned long process_current_pid(void)
{
    if (current_process == 0) {
        return 0;
    }

    return current_process->pid;
}

pagetable_t process_current_pagetable(void)
{
    if (current_process == 0) {
        return 0;
    }

    return current_process->pagetable;
}

int process_has_private_user_memory(
    struct process *process)
{
    unsigned long pa =
        vm_translate(
            process->pagetable,
            (unsigned long)__user_text_start
        );

    if (pa == 0) {
        return 0;
    }

    return pa !=
        (unsigned long)__user_text_start;
}

int process_address_spaces_distinct(
    struct process *a,
    struct process *b)
{
    if (a->pagetable == b->pagetable) {
        return 0;
    }

    unsigned long a_text =
        vm_translate(
            a->pagetable,
            (unsigned long)__user_text_start
        );

    unsigned long b_text =
        vm_translate(
            b->pagetable,
            (unsigned long)__user_text_start
        );

    unsigned long a_stack =
        vm_translate(
            a->pagetable,
            USER_STACK_BASE
        );

    unsigned long b_stack =
        vm_translate(
            b->pagetable,
            USER_STACK_BASE
        );

    if (a_text == 0 ||
        b_text == 0 ||
        a_stack == 0 ||
        b_stack == 0) {
        return 0;
    }

    return
        a_text != b_text &&
        a_stack != b_stack;
}

void process_mark_syscall_complete(void)
{
    if (current_process != 0) {
        current_process->syscall_complete = 1;
    }
}

int process_syscall_complete(
    struct process *process)
{
    return process->syscall_complete;
}
