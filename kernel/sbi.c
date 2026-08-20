#include "../include/sbi.h"

#define SBI_EXT_TIME 0x54494D45UL
#define SBI_FID_SET_TIMER 0UL

void sbi_set_timer(unsigned long time)
{
    register unsigned long a0 __asm__("a0") = time;
    register unsigned long a6 __asm__("a6") = SBI_FID_SET_TIMER;
    register unsigned long a7 __asm__("a7") = SBI_EXT_TIME;

    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a6), "r"(a7)
        : "memory"
    );
}
