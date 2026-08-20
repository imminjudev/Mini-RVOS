#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/riscv.h"

void uart_puts(const char *s);

#define RAM_END 0x88000000UL
#define UART0   0x10000000UL

extern char __text_start[];
extern char __text_end[];

extern char __rodata_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __data_end[];

extern char __kernel_end[];

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

    if (map_region(
            root,
            (unsigned long)__text_start,
            (unsigned long)__text_end,
            PTE_R | PTE_X | PTE_A) != 0) {

        uart_puts("[FAIL] text mapping\n");

        for (;;) {
        }
    }

    if (map_region(
            root,
            (unsigned long)__rodata_start,
            (unsigned long)__rodata_end,
            PTE_R | PTE_A) != 0) {

        uart_puts("[FAIL] rodata mapping\n");

        for (;;) {
        }
    }

    if (map_region(
            root,
            (unsigned long)__data_start,
            (unsigned long)__data_end,
            PTE_R | PTE_W | PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] data mapping\n");

        for (;;) {
        }
    }

    if (map_region(
            root,
            (unsigned long)__kernel_end,
            RAM_END,
            PTE_R | PTE_W | PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] free RAM mapping\n");

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

    uart_puts("[OK] protected mappings ready\n");

    vm_enable(root);

    uart_puts("[OK] paging enabled\n");

    if ((riscv_read_satp() >> 60) == 8) {
        uart_puts("[OK] Sv39 active\n");
    } else {
        uart_puts("[FAIL] Sv39 activation\n");
    }

    for (;;) {
    }
}
