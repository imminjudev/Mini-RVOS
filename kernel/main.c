#include "../include/memory.h"

void uart_puts(const char *s);

#define PHYS_MEM_END 0x88000000UL
#define TEST_PAGE_COUNT 8

volatile unsigned long bss_probe;

static int pages_are_unique(void **pages, unsigned long count)
{
    for (unsigned long i = 0; i < count; i++) {
        for (unsigned long j = i + 1; j < count; j++) {
            if (pages[i] == pages[j]) {
                return 0;
            }
        }
    }

    return 1;
}

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

    unsigned long initial_free = pmm_free_pages();

    if (initial_free > 0 &&
        pmm_total_pages() == initial_free) {
        uart_puts("[OK] pmm initialized\n");
    } else {
        uart_puts("[FAIL] pmm initialization\n");
    }

    void *pages[TEST_PAGE_COUNT];
    int allocation_ok = 1;
    int alignment_ok = 1;

    for (unsigned long i = 0; i < TEST_PAGE_COUNT; i++) {
        pages[i] = page_alloc();

        if (pages[i] == 0) {
            allocation_ok = 0;
            continue;
        }

        if (((unsigned long)pages[i] & (PAGE_SIZE - 1)) != 0) {
            alignment_ok = 0;
        }
    }

    if (allocation_ok) {
        uart_puts("[OK] page allocation\n");
    } else {
        uart_puts("[FAIL] page allocation\n");
    }

    if (alignment_ok) {
        uart_puts("[OK] page alignment\n");
    } else {
        uart_puts("[FAIL] page alignment\n");
    }

    if (allocation_ok &&
        pages_are_unique(pages, TEST_PAGE_COUNT)) {
        uart_puts("[OK] page uniqueness\n");
    } else {
        uart_puts("[FAIL] page uniqueness\n");
    }

    if (allocation_ok &&
        pmm_free_pages() == initial_free - TEST_PAGE_COUNT) {
        uart_puts("[OK] allocation accounting\n");
    } else {
        uart_puts("[FAIL] allocation accounting\n");
    }

    for (unsigned long i = 0; i < TEST_PAGE_COUNT; i++) {
        if (pages[i] != 0) {
            page_free(pages[i]);
        }
    }

    if (pmm_free_pages() == initial_free) {
        uart_puts("[OK] free accounting\n");
    } else {
        uart_puts("[FAIL] free accounting\n");
    }

    for (;;) {
    }
}
