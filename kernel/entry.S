.section .text.boot
.globl _start

_start:
    la sp, stack_top

    call kernel_main

1:
    wfi
    j 1b

.section .bss.stack
.align 12

stack_bottom:
    .skip 4096

stack_top:
