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
    la s2, path_one

    la s3, message_one
    la t0, message_one_end
    sub s4, t0, s3

    j create_file


process_two:
    la s2, path_two

    la s3, message_two
    la t0, message_two_end
    sub s4, t0, s3


create_file:
    /*
     * create(path)
     * syscall 7
     *
     * return = opened fd
     */
    mv a0, s2
    li a7, 7
    ecall

    bltz a0, failed

    mv s1, a0

    /*
     * write(fd, message, length)
     */
    mv a0, s1
    mv a1, s3
    mv a2, s4
    li a7, 1
    ecall

    bne a0, s4, failed

    /*
     * open(path) again.
     * New fd gets offset 0.
     */
    mv a0, s2
    li a7, 5
    ecall

    bltz a0, failed

    mv s5, a0

    /*
     * read buffer on user stack.
     */
    addi sp, sp, -128

    mv a0, s5
    mv a1, sp
    li a2, 128
    li a7, 6
    ecall

    bne a0, s4, failed

    /*
     * print contents that were read
     * back from the file.
     */
    mv s0, a0

    li a0, 1
    mv a1, sp
    mv a2, s0
    li a7, 1
    ecall

    bne a0, s0, failed

    /*
     * syscall 2 = success
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

path_one:
    .asciz "/p1.txt"

path_two:
    .asciz "/p2.txt"


message_one:
    .ascii "File written by process 1!\n"
message_one_end:


message_two:
    .ascii "File written by process 2!\n"
message_two_end:
