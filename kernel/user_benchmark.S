#include "../include/benchmark.h"

.section .user.text, "ax"
.balign 4

.globl user_benchmark_main
user_benchmark_main:

#if BENCHMARK_WORKLOAD_MEMORY

    /*
     * s11 = completed full working-set sweeps.
     *
     * The trap path preserves s11 in struct trap_frame,
     * so the kernel can read progress without adding
     * memory traffic for a separate progress counter.
     */
    li s11, 0

    li t0, BENCHMARK_WORKING_SET_BASE
    li t1, BENCHMARK_WORKING_SET_PAGES

1:
    li t2, 0

2:
    slli t3, t2, 12
    add t4, t0, t3

    ld t5, 0(t4)

    add t5, t5, s11
    add t5, t5, t2
    addi t5, t5, 1

    sd t5, 0(t4)

    addi t2, t2, 1
    bltu t2, t1, 2b

    addi s11, s11, 1
    j 1b

#else

    /*
     * s11 = completed arithmetic iterations.
     */
    li s11, 0

    li t0, 1
    li t1, 1664525
    li t2, 1013904223

1:
    mul t0, t0, t1
    add t0, t0, t2

    addi s11, s11, 1

    j 1b

#endif
