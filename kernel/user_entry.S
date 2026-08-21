.section .user.text, "ax"
.balign 4

.globl user_entry
user_entry:
    call user_shell_main

1:
    j 1b
