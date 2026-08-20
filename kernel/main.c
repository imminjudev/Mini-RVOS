#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/riscv.h"

void uart_puts(const char *s);

#define RAM_START 0x80200000UL
#define RAM_END   0x88000000UL

#define UART0     0x10000000UL

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

    if (vm_map_range(
            root,
            RAM_START,
            RAM_START,
            RAM_END - RAM_START,
            PTE_R | PTE_W | PTE_X | PTE_A | PTE_D) != 0) {

        uart_puts("[FAIL] RAM identity map\n");

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

    uart_puts("[OK] identity map ready\n");

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
