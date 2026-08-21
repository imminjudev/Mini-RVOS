.section .user.text, "ax"
.balign 4

.globl user_syscall3
user_syscall3:
    /*
     * C ABI 진입:
     *
     * a0 = syscall number
     * a1 = arg0
     * a2 = arg1
     * a3 = arg2
     *
     * Mini-RVOS syscall ABI:
     *
     * a7 = syscall number
     * a0 = arg0
     * a1 = arg1
     * a2 = arg2
     */

    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3

    ecall

    /*
     * kernel이 a0에 반환값을 넣어준다.
     */
    ret
