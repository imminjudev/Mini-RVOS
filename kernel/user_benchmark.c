#include "../include/benchmark.h"

#define USER_TEXT \
    __attribute__((section(".user.text")))

USER_TEXT void user_benchmark_main(void)
{
#if BENCHMARK_WORKLOAD_MEMORY

    unsigned long sweep = 1;

    for (;;) {
        for (unsigned long page = 0;
             page < BENCHMARK_WORKING_SET_PAGES;
             page++) {

            unsigned long address =
                BENCHMARK_WORKING_SET_BASE +
                page * BENCHMARK_PAGE_SIZE;

            volatile unsigned long *slot =
                (volatile unsigned long *)address;

            unsigned long value =
                *slot;

            *slot =
                value +
                sweep +
                page +
                1;
        }

        sweep++;

        __asm__ volatile(
            ""
            : "+r"(sweep)
            :
            : "memory"
        );
    }

#else

    unsigned long value = 1;

    for (;;) {
        value =
            value * 1664525UL +
            1013904223UL;

        __asm__ volatile(
            ""
            : "+r"(value)
        );
    }

#endif
}
