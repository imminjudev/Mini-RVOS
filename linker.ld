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


    __user_text_start = .;

    .usertext : {
        KEEP(*(.user.text))
    }

    . = ALIGN(4096);
    __user_text_end = .;


    __user_rodata_start = .;

    .userrodata : {
        KEEP(*(.user.rodata))
    }

    . = ALIGN(4096);
    __user_rodata_end = .;


    __rodata_start = .;

    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);
    __rodata_end = .;


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
