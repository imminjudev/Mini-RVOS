#define USER_TEXT \
    __attribute__((section(".user.text")))

USER_TEXT void user_benchmark_main(void)
{
    unsigned long value = 1;

    for (;;) {
        value =
            value * 1664525UL +
            1013904223UL;

        /*
         * value가 observable output으로 사용되지 않아도
         * compiler가 benchmark loop를 제거하지 못하게 한다.
         */
        __asm__ volatile(
            ""
            : "+r"(value)
        );
    }
}
