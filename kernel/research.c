#include "../include/research.h"
#include "../include/benchmark.h"
#include "../include/riscv.h"
#include "../include/uart.h"

#if BENCHMARK_USE_ASID
#define RESEARCH_SWITCH_MODE "ASID"
#else
#define RESEARCH_SWITCH_MODE "FULL_FLUSH"
#endif

static unsigned long context_switch_count;
static unsigned long sfence_count;

static unsigned long
    address_space_switch_ticks_total;

static unsigned long
    address_space_switch_ticks_max;

static unsigned long benchmark_start_ticks;

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

static void print_csv_row(
    unsigned long elapsed_ticks)
{
    uart_puts(
        "[CSV_HEADER] "
        "mode,workload,quantum_ticks,target_switches,"
        "elapsed_ticks,context_switches,sfence_count,"
        "address_space_switch_ticks_total,"
        "address_space_switch_ticks_max\n"
    );

    uart_puts(
        "[CSV] "
        RESEARCH_SWITCH_MODE
        ",cpu,"
    );

    print_unsigned_long(
        BENCHMARK_QUANTUM_TICKS
    );

    uart_putc(',');

    print_unsigned_long(
        BENCHMARK_SWITCHES
    );

    uart_putc(',');

    print_unsigned_long(
        elapsed_ticks
    );

    uart_putc(',');

    print_unsigned_long(
        context_switch_count
    );

    uart_putc(',');

    print_unsigned_long(
        sfence_count
    );

    uart_putc(',');

    print_unsigned_long(
        address_space_switch_ticks_total
    );

    uart_putc(',');

    print_unsigned_long(
        address_space_switch_ticks_max
    );

    uart_putc('\n');
}

void research_reset(void)
{
    context_switch_count = 0;
    sfence_count = 0;

    address_space_switch_ticks_total = 0;
    address_space_switch_ticks_max = 0;

    benchmark_start_ticks =
        riscv_read_time();
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
    unsigned long benchmark_end_ticks =
        riscv_read_time();

    unsigned long elapsed_ticks =
        benchmark_end_ticks -
        benchmark_start_ticks;

    uart_puts(
        "[BENCH] switch_mode="
        RESEARCH_SWITCH_MODE
        "\n"
    );

    print_value(
        "quantum_ticks",
        BENCHMARK_QUANTUM_TICKS
    );

    print_value(
        "target_switches",
        BENCHMARK_SWITCHES
    );

    print_value(
        "elapsed_ticks",
        elapsed_ticks
    );

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

    print_csv_row(
        elapsed_ticks
    );
}
