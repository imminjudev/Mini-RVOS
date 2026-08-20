.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    /*
     * getpid()
     */
    li a7, 4
    ecall

    li t0, 1
    beq a0, t0, process_one

    li t0, 2
    beq a0, t0, process_two

    j failed


process_one:
    la a1, message_one
    la t0, message_one_end
    sub a2, t0, a1
    j do_write


process_two:
    la a1, message_two
    la t0, message_two_end
    sub a2, t0, a1


do_write:
    mv s0, a2

    li a0, 1
    li a7, 1
    ecall

    bne a0, s0, failed

    /*
     * syscall 2:
     * 이 process의 syscall 테스트 완료.
     */
    li a7, 2
    ecall

idle:
    j idle


failed:
    li a7, 3
    ecall

failed_loop:
    j failed_loop


.section .user.rodata, "a"

message_one:
    .ascii "Hello from process 1!\n"
message_one_end:

message_two:
    .ascii "Hello from process 2!\n"
message_two_end:
