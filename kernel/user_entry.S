.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
#ifdef BENCHMARK_MODE
    call user_benchmark_main
#else
    call user_shell_main
#endif

1:
    j 1b
