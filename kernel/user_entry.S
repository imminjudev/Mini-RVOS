.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    /*
     * write(
     *     fd  = 1,
     *     buf = user_message,
     *     len = user_message_end - user_message
     * )
     */
    li a0, 1

    la a1, user_message
    la t0, user_message_end
    sub a2, t0, a1

    /*
     * write() 반환값 검증용으로
     * 요청 길이를 s0에 보관.
     */
    mv s0, a2

    li a7, 1
    ecall

    /*
     * a0 == write() 반환값
     */
    bne a0, s0, 1f

    /*
     * syscall 2 = 성공
     */
    li a7, 2
    ecall

2:
    j 2b

1:
    /*
     * syscall 3 = 실패
     */
    li a7, 3
    ecall

3:
    j 3b


.section .user.rodata, "a"

user_message:
    .ascii "Hello from U-mode via write syscall!\n"

user_message_end:
