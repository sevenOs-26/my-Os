/* SevenOS v3.3 Linker Script - Alpine Edition */
OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS {
    /* Sugod sa 4KB address */
    . = 0x1000;

    .text : ALIGN(4) {
        /* I-force ang _start sa pinaka-unahan sa binary */
        *(.text.start)
        *(.text)
    }

    .rodata : ALIGN(4) {
        *(.rodata*)
    }

    .data : ALIGN(4) {
        *(.data*)
    }

    .bss : ALIGN(4) {
        *(.bss*)
        *(COMMON)
    }

    /* I-discard ang Alpine/Linux specific sections nga makaguba sa flat binary */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
