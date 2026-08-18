#define UART0 0x10000000UL

#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_TX_IDLE (1 << 5)

static volatile unsigned char *const uart =
    (volatile unsigned char *)UART0;

void uart_putc(char c)
{
    while ((uart[UART_LSR] & UART_LSR_TX_IDLE) == 0) {
    }

    uart[UART_THR] = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}
