#include "../include/memory.h"
#include "../include/vm.h"

void uart_puts(const char *s);

#define PHYS_MEM_END 0x88000000UL
#define TEST_VA      0x40000000UL

void kernel_main(unsigned long hart_id, void *dtb)
{
    (void)hart_id;
    (void)dtb;

    uart_puts("Mini-RVOS booting...\n");

    pmm_init(PHYS_MEM_END);

    pagetable_t root = vm_create();
    void *page = page_alloc();

    if (root == 0 || page == 0) {
        uart_puts("[FAIL] vm allocation\n");

        for (;;) {
        }
    }

    if (vm_map_page(
            root,
            TEST_VA,
            (unsigned long)page,
            PTE_R | PTE_W | PTE_A | PTE_D) == 0) {
        uart_puts("[OK] page mapping\n");
    } else {
        uart_puts("[FAIL] page mapping\n");
    }

    unsigned long translated =
        vm_translate(root, TEST_VA);

    if (translated == (unsigned long)page) {
        uart_puts("[OK] address translation\n");
    } else {
        uart_puts("[FAIL] address translation\n");
    }

    if (vm_translate(root, TEST_VA + 123)
        == (unsigned long)page + 123) {
        uart_puts("[OK] page offset preserved\n");
    } else {
        uart_puts("[FAIL] page offset\n");
    }

    for (;;) {
    }
}
