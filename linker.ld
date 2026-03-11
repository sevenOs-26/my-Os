/* SevenOS v3.3 Linker Script - Alpine Edition */
OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS {
    /* Sugod sa 4KB address */
    . = 0x1000;

    .text : ALIGN(4) {
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

    /* KANI ANG IMPORTANTE: I-marka ang tumoy sa kernel */
    _kernel_end = .; 
    end = .;

    /* I-discard ang Alpine/Linux specific sections */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
