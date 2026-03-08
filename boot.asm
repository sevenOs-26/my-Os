[bits 16]
[org 0x7c00]

_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [BOOT_DRIVE], dl

    ; --- DEBUG SIGNAL 1: 'B' (Bootloader started) ---
    mov al, 'B'
    mov ah, 0x0e
    int 0x10

    ; LOAD KERNEL
    mov ah, 0x02
    mov al, 32          ; Load 32 sectors
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02        ; Sugod sa Sector 2
    mov dl, [BOOT_DRIVE]
    mov bx, 0x1000      ; I-load sa address 0x1000
    int 0x13
    jc disk_error

    ; --- DEBUG SIGNAL 2: 'K' (Kernel Loaded in RAM) ---
    mov al, 'K'
    mov ah, 0x0e
    int 0x10

    ; ENABLE A20 LINE (Importante sa Alpine/QEMU)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; SWITCH SA 32-BIT PROTECTED MODE
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp 0x08:init_32bit

[bits 32]
init_32bit:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; --- FINAL JUMP SA KERNEL ---
    jmp 0x1000

disk_error:
    ; Print 'E' kon naay problema sa disk
    mov al, 'E'
    mov ah, 0x0e
    int 0x10
    jmp $

; --- GDT SETTINGS ---
gdt_start:
    dq 0x0
gdt_code:
    dw 0xffff, 0x0
    db 0x0, 10011010b, 11001111b, 0x0
gdt_data:
    dw 0xffff, 0x0
    db 0x0, 10010010b, 11001111b, 0x0
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

BOOT_DRIVE db 0
times 510-($-$$) db 0
dw 0xAA55
