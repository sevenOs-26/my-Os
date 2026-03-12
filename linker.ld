/* SevenOS v3.3 Linker Script - Optimized */
OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS {
    /* Sugod sa 4KB address - Match sa boot.asm */
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

    /* BSS section (Zero-initialized data) */
    .bss : ALIGN(4) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }

    /* Mao ni ang gigamit sa imong 'ME' command para i-print ang KERNEL END */
    _kernel_end = .;
    end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
