#include "../include/memory.h"

void uart_puts(const char *s);

#define PHYS_MEM_END 0x88000000UL

volatile unsigned long bss_probe;

void kernel_main(unsigned long hart_id, void *dtb)
{
    (void)hart_id;
    (void)dtb;

    uart_puts("Mini-RVOS booting...\n");

    if (bss_probe == 0) {
        uart_puts("[OK] bss initialized\n");
    } else {
        uart_puts("[FAIL] bss initialization\n");
    }

    pmm_init(PHYS_MEM_END);

    void *page1 = page_alloc();
    void *page2 = page_alloc();

    if (page1 != 0 && page2 != 0 && page1 != page2) {
        uart_puts("[OK] page allocation\n");
    } else {
        uart_puts("[FAIL] page allocation\n");
    }

    unsigned long before = pmm_free_pages();
    page_free(page1);

    if (pmm_free_pages() == before + 1) {
        uart_puts("[OK] page free\n");
    } else {
        uart_puts("[FAIL] page free\n");
    }

    for (;;) {
    }
}
