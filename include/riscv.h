#ifndef RISCV_H
#define RISCV_H

#define SATP_MODE_SV39 (8UL << 60)

static inline void riscv_write_satp(unsigned long value)
{
    __asm__ volatile("csrw satp, %0" :: "r"(value) : "memory");
}

static inline unsigned long riscv_read_satp(void)
{
    unsigned long value;

    __asm__ volatile("csrr %0, satp" : "=r"(value));

    return value;
}

static inline void riscv_sfence_vma(void)
{
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");
}

#endif
