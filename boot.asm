[bits 16]
[org 0x7c00]

_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00          ; Stack sa Real Mode
    mov [BOOT_DRIVE], dl

    ; --- DEBUG SIGNAL 1: 'B' ---
    mov al, 'B'
    mov ah, 0x0e
    int 0x10

    ; --- LOAD KERNEL ---
    ; Sigurohon nato ang ES:BX address (0x0000:0x1000)
    xor ax, ax
    mov es, ax              ; ES = 0
    mov bx, 0x1000          ; BX = 0x1000. Kernel loads at 0x1000

    mov ah, 0x02
    mov al, 32              ; Balik sa 32 sectors (16KB) para mas safe sa BIOS
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02            ; Sector 2
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error           ; Mo-print og 'E' kon naay problema sa disk

    ; --- DEBUG SIGNAL 2: 'K' ---
    mov al, 'K'
    mov ah, 0x0e
    int 0x10

    ; ENABLE A20 LINE
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

    ; --- THE STACK RE-BORE ---
    ; Gi-ubos nato gamay sa 0x7FFFF para sigurado nga layo sa BIOS/VGA reserved areas
    mov esp, 0x7FFFF
    mov ebp, esp

    ; --- FINAL JUMP SA KERNEL ---
    jmp 0x1000

disk_error:
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
