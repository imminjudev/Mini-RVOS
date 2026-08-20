#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/trap.h"
#include "../include/riscv.h"

void uart_puts(const char *s);

#define RAM_END          0x88000000UL
#define UART0            0x10000000UL

#define USER_STACK_BASE  0x40000000UL
#define USER_STACK_TOP   (USER_STACK_BASE + PAGE_SIZE)

extern char __text_start[];
extern char __text_end[];

extern char __user_text_start[];
extern char __user_text_end[];

extern char __rodata_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __data_end[];

extern char __kernel_end[];

extern char __user_rodata_start[];
extern char __user_rodata_end[];

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

static void clear_trap_frame(
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

void kernel_main(unsigned long hart_id, void *dtb)
{
    (void)hart_id;
    (void)dtb;

    uart_puts("Mini-RVOS booting...\n");

    pmm_init(RAM_END);

    pagetable_t root = vm_create();

    if (root == 0) {
        uart_puts("[FAIL] root page table\n");

        for (;;) {
        }
    }

    void *user_stack_page = page_alloc();
    void *kernel_stack_page = page_alloc();

    if (user_stack_page == 0 ||
        kernel_stack_page == 0) {

        uart_puts("[FAIL] user task stacks\n");

        for (;;) {
        }
    }

    if (map_region(
            root,
            (unsigned long)__text_start,
            (unsigned long)__text_end,
            PTE_R | PTE_X | PTE_A) != 0 ||

        map_region(
            root,
            (unsigned long)__user_text_start,
            (unsigned long)__user_text_end,
            PTE_R | PTE_X | PTE_U | PTE_A) != 0 ||

        map_region(
            root,
            (unsigned long)__user_rodata_start,
            (unsigned long)__user_rodata_end,
            PTE_R | PTE_U | PTE_A) != 0 ||

        map_region(
            root,
            (unsigned long)__rodata_start,
            (unsigned long)__rodata_end,
            PTE_R | PTE_A) != 0 ||

        map_region(
            root,
            (unsigned long)__data_start,
            (unsigned long)__data_end,
            PTE_R | PTE_W | PTE_A | PTE_D) != 0 ||

        map_region(
            root,
            (unsigned long)__kernel_end,
            RAM_END,
            PTE_R | PTE_W | PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] kernel/user mappings\n");

        for (;;) {
        }
    }

    if (vm_map_page(
            root,
            UART0,
            UART0,
            PTE_R | PTE_W | PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] UART mapping\n");

        for (;;) {
        }
    }

    if (vm_map_page(
            root,
            USER_STACK_BASE,
            (unsigned long)user_stack_page,
            PTE_R | PTE_W | PTE_U |
            PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] user stack mapping\n");

        for (;;) {
        }
    }

    uart_puts("[OK] user mappings ready\n");

    trap_init();

    /*
     * Disable scheduler timer for this
     * isolated U-mode checkpoint.
     */
    riscv_disable_timer_interrupt();

    vm_enable(root);

    uart_puts("[OK] paging enabled\n");

    unsigned long kernel_stack_top =
        (unsigned long)kernel_stack_page +
        PAGE_SIZE;

    struct trap_frame *user_frame =
        (struct trap_frame *)
        (kernel_stack_top - TRAP_FRAME_SIZE);

    clear_trap_frame(user_frame);

    user_frame->sp =
        USER_STACK_TOP;

    user_frame->sepc =
        (unsigned long)__user_text_start;

    /*
     * SPP = 0 -> sret enters U-mode.
     */
    user_frame->sstatus = 0;

    uart_puts("[OK] entering U-mode\n");

    trap_resume(user_frame);
}
