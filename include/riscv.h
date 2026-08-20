#ifndef RISCV_H
#define RISCV_H

#define SATP_MODE_SV39 (8UL << 60)

#define SSTATUS_SIE  (1UL << 1)
#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SPP  (1UL << 8)
#define SSTATUS_SUM  (1UL << 18)

#define SIE_STIE     (1UL << 5)

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

static inline void riscv_write_stvec(unsigned long value)
{
    __asm__ volatile("csrw stvec, %0" :: "r"(value) : "memory");
}

static inline unsigned long riscv_read_scause(void)
{
    unsigned long value;

    __asm__ volatile("csrr %0, scause" : "=r"(value));

    return value;
}

static inline unsigned long riscv_read_sepc(void)
{
    unsigned long value;

    __asm__ volatile("csrr %0, sepc" : "=r"(value));

    return value;
}

static inline void riscv_write_sepc(unsigned long value)
{
    __asm__ volatile("csrw sepc, %0" :: "r"(value) : "memory");
}

static inline unsigned long riscv_read_stval(void)
{
    unsigned long value;

    __asm__ volatile("csrr %0, stval" : "=r"(value));

    return value;
}

static inline unsigned long riscv_read_time(void)
{
    unsigned long value;

    __asm__ volatile("rdtime %0" : "=r"(value));

    return value;
}

static inline void riscv_enable_interrupts(void)
{
    __asm__ volatile(
        "csrs sstatus, %0"
        ::
        "r"(SSTATUS_SIE)
        : "memory"
    );
}

static inline void riscv_enable_timer_interrupt(void)
{
    __asm__ volatile(
        "csrs sie, %0"
        ::
        "r"(SIE_STIE)
        : "memory"
    );
}

static inline void riscv_disable_timer_interrupt(void)
{
    __asm__ volatile(
        "csrc sie, %0"
        ::
        "r"(SIE_STIE)
        : "memory"
    );
}

static inline void riscv_wfi(void)
{
    __asm__ volatile("wfi");
}

static inline void riscv_enable_user_memory_access(void)
{
    __asm__ volatile(
        "csrs sstatus, %0"
        ::
        "r"(SSTATUS_SUM)
        : "memory"
    );
}

static inline void riscv_disable_user_memory_access(void)
{
    __asm__ volatile(
        "csrc sstatus, %0"
        ::
        "r"(SSTATUS_SUM)
        : "memory"
    );
}

#endif
