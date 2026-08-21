ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    . = ALIGN(4096);
    __text_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    . = ALIGN(4096);
    __text_end = .;


    /*
     * User executable code.
     */
    __user_text_start = .;

    .usertext : {
        KEEP(*(.user.text))
        KEEP(*(.user.text.*))
    }

    . = ALIGN(4096);
    __user_text_end = .;


    /*
     * User read-only data.
     *
     * user_shell.c에서 명시적으로 만든
     * .user.rodata뿐 아니라 GCC가 자동으로
     * 생성하는 constant pool도 포함한다.
     *
     * 예:
     * division/modulo 최적화용 magic constant
     * -> .rodata.cst8
     */
    __user_rodata_start = .;

    .userrodata : {
        KEEP(*(.user.rodata))
        KEEP(*(.user.rodata.*))

        build/user_shell.o(
            .rodata
            .rodata.*
            .srodata
            .srodata.*
        )
    }

    . = ALIGN(4096);
    __user_rodata_end = .;


    /*
     * Kernel read-only data.
     */
    __rodata_start = .;

    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(4096);
    __rodata_end = .;


    /*
     * Kernel writable data.
     */
    __data_start = .;

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    .bss : {
        __bss_start = .;

        *(.bss .bss.*)
        *(.sbss .sbss.*)

        __bss_end = .;
    }

    .stack : {
        *(.stack)
    }

    . = ALIGN(4096);
    __data_end = .;
    __kernel_end = .;
}
