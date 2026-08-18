ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

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
}
