.section .text
.balign 4

.globl trap_entry
trap_entry:
    addi sp, sp, -272

    sd ra,   0(sp)

    sd t0,  32(sp)
    addi t0, sp, 272
    sd t0,   8(sp)

    sd gp,  16(sp)
    sd tp,  24(sp)

    sd t1,  40(sp)
    sd t2,  48(sp)

    sd s0,  56(sp)
    sd s1,  64(sp)

    sd a0,  72(sp)
    sd a1,  80(sp)
    sd a2,  88(sp)
    sd a3,  96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)

    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)

    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    csrr t0, sepc
    sd t0, 248(sp)

    csrr t0, sstatus
    sd t0, 256(sp)

    mv a0, sp
    call trap_handler

    j trap_restore


.globl trap_resume
trap_resume:
    j trap_restore


trap_restore:
    /*
     * a0 = trap frame to restore.
     * t6 is used temporarily as its pointer.
     */
    mv t6, a0

    ld t0, 248(t6)
    csrw sepc, t0

    ld t0, 256(t6)
    csrw sstatus, t0

    ld ra,   0(t6)

    ld gp,  16(t6)
    ld tp,  24(t6)

    ld t1,  40(t6)
    ld t2,  48(t6)

    ld s0,  56(t6)
    ld s1,  64(t6)

    ld a0,  72(t6)
    ld a1,  80(t6)
    ld a2,  88(t6)
    ld a3,  96(t6)
    ld a4, 104(t6)
    ld a5, 112(t6)
    ld a6, 120(t6)
    ld a7, 128(t6)

    ld s2, 136(t6)
    ld s3, 144(t6)
    ld s4, 152(t6)
    ld s5, 160(t6)
    ld s6, 168(t6)
    ld s7, 176(t6)
    ld s8, 184(t6)
    ld s9, 192(t6)
    ld s10, 200(t6)
    ld s11, 208(t6)

    ld t3, 216(t6)
    ld t4, 224(t6)
    ld t5, 232(t6)

    ld t0,  32(t6)

    ld sp,   8(t6)
    ld t6, 240(t6)

    sret


.globl trigger_store_page_fault
trigger_store_page_fault:
    .option push
    .option norvc

    sw zero, 0(a0)
    ret

    .option pop
