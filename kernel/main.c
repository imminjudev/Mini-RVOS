#include "../include/memory.h"
#include "../include/vm.h"
#include "../include/riscv.h"
#include "../include/trap.h"
#include "../include/sbi.h"

void uart_puts(const char *s);

#define RAM_END 0x88000000UL
#define UART0   0x10000000UL

#define TIMER_INTERVAL 10000000UL

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
            PTE_R | PTE_X | PTE_A) != 0 ||
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

        uart_puts("[FAIL] kernel mappings\n");

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

    trap_init();
    vm_enable(root);

    uart_puts("[OK] paging enabled\n");
    uart_puts("[OK] trap vector ready\n");

    riscv_enable_timer_interrupt();

    sbi_set_timer(
        riscv_read_time() + TIMER_INTERVAL
    );

    riscv_enable_interrupts();

    uart_puts("[OK] timer armed\n");

    while (trap_get_timer_ticks() < 3) {
        riscv_wfi();
    }

    uart_puts("[OK] timer test complete\n");

    for (;;) {
        riscv_wfi();
    }
}
