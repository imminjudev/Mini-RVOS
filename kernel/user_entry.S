.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    /*
     * open("/hello.txt")
     * syscall 5
     */
    la a0, file_path
    li a7, 5
    ecall

    bltz a0, failed

    /*
     * fd 보관
     */
    mv s1, a0

    /*
     * user stack에 128-byte read buffer 확보.
     */
    addi sp, sp, -128

    /*
     * read(fd, sp, 128)
     * syscall 6
     */
    mv a0, s1
    mv a1, sp
    li a2, 128
    li a7, 6
    ecall

    blez a0, failed

    /*
     * 읽은 byte 수 보관
     */
    mv s0, a0

    /*
     * write(1, sp, bytes_read)
     */
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

file_path:
    .asciz "/hello.txt"
