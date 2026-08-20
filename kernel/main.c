#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/riscv.h"
#include "../include/trap.h"

void uart_puts(const char *s);

#define RAM_END 0x88000000UL
#define UART0   0x10000000UL

#define SCAUSE_STORE_PAGE_FAULT 15UL

extern char __text_start[];
extern char __text_end[];

extern char __rodata_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __data_end[];

extern char __kernel_end[];

static const unsigned int protected_word
    __attribute__((section(".rodata"))) = 0x12345678U;

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

    trap_init();
    uart_puts("[OK] trap vector ready\n");

    vm_enable(root);

    uart_puts("[OK] paging enabled\n");

    if ((riscv_read_satp() >> 60) == 8) {
        uart_puts("[OK] Sv39 active\n");
    } else {
        uart_puts("[FAIL] Sv39 activation\n");

        for (;;) {
        }
    }

    trigger_store_page_fault(
        (unsigned long)&protected_word
    );

    if (trap_get_last_cause() == SCAUSE_STORE_PAGE_FAULT) {
        uart_puts("[OK] scause captured\n");
    } else {
        uart_puts("[FAIL] scause\n");
    }

    if (trap_get_last_value()
        == (unsigned long)&protected_word) {
        uart_puts("[OK] stval fault address\n");
    } else {
        uart_puts("[FAIL] stval\n");
    }

    if (*(volatile const unsigned int *)&protected_word
        == 0x12345678U) {
        uart_puts("[OK] rodata protection enforced\n");
    } else {
        uart_puts("[FAIL] rodata modified\n");
    }

    uart_puts("[OK] trap return\n");

    for (;;) {
    }
}
