void uart_puts(const char *s);

void kernel_main(void)
{
    uart_puts("Mini-RVOS booting...\n");
    uart_puts("Hello from kernel.\n");

    for (;;) {
    }
}
