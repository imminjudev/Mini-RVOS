#include "../include/research.h"
#include "../include/uart.h"

static unsigned long context_switch_count;
static unsigned long sfence_count;

static unsigned long
    address_space_switch_ticks_total;

static unsigned long
    address_space_switch_ticks_max;

static void print_unsigned_long(
    unsigned long value)
{
    char buffer[32];
    unsigned long position = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0) {
        buffer[position++] =
            (char)(
                '0' +
                (value % 10)
            );

        value /= 10;
    }

    while (position > 0) {
        uart_putc(
            buffer[--position]
        );
    }
}

static void print_value(
    const char *name,
    unsigned long value)
{
    uart_puts("[BENCH] ");
    uart_puts(name);
    uart_putc('=');

    print_unsigned_long(value);

    uart_putc('\n');
}

void research_reset(void)
{
    context_switch_count = 0;
    sfence_count = 0;

    address_space_switch_ticks_total = 0;
    address_space_switch_ticks_max = 0;
}

void research_record_context_switch(void)
{
    context_switch_count++;
}

void research_record_sfence(
    unsigned long count)
{
    sfence_count += count;
}

void research_record_address_space_switch(
    unsigned long ticks)
{
    address_space_switch_ticks_total +=
        ticks;

    if (ticks >
        address_space_switch_ticks_max) {

        address_space_switch_ticks_max =
            ticks;
    }
}

void research_print_summary(void)
{
    print_value(
        "context_switch_count",
        context_switch_count
    );

    print_value(
        "sfence_count",
        sfence_count
    );

    print_value(
        "address_space_switch_ticks_total",
        address_space_switch_ticks_total
    );

    print_value(
        "address_space_switch_ticks_max",
        address_space_switch_ticks_max
    );
}
