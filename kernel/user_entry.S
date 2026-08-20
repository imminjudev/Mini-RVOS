.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    /*
     * syscall 4 = getpid()
     */
    li a7, 4
    ecall

    li t0, 1
    bne a0, t0, 1f

    /*
     * write(1, message, length)
     */
    li a0, 1

    la a1, user_message
    la t0, user_message_end
    sub a2, t0, a1

    mv s0, a2

    li a7, 1
    ecall

    bne a0, s0, 1f

    /*
     * syscall 2 = success
     */
    li a7, 2
    ecall

2:
    j 2b

1:
    /*
     * syscall 3 = failure
     */
    li a7, 3
    ecall

3:
    j 3b


.section .user.rodata, "a"

user_message:
    .ascii "Hello from process 1!\n"

user_message_end:
