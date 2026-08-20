.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    /*
     * syscall 1:
     * basic U-mode syscall test
     */
    li a7, 1
    ecall

    /*
     * 여기까지 왔다는 것은 첫 syscall에서
     * 다시 U-mode로 정상 복귀했다는 뜻.
     */
    li a7, 2
    ecall

1:
    j 1b
