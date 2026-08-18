void uart_puts(const char *s);

volatile unsigned long bss_probe;

void kernel_main(void)
{
    uart_puts("Mini-RVOS booting...\n");
    uart_puts("Hello from kernel.\n");

    if (bss_probe == 0) {
        uart_puts("[OK] bss initialized\n");
    } else {
        uart_puts("[FAIL] bss initialization\n");
    }

    for (;;) {
    }
}
