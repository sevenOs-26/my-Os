#!/bin/bash
# --- SevenOS v3.3 Alpine Edition Build ---
CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}[*] Hard Reset Build...${NC}"

# 1. Limpyo
rm -f *.bin *.o *.img

# 2. Bootloader
nasm -f bin boot.asm -o boot.bin || { echo "NASM Error"; exit 1; }

# 3. Kernel (Dugangan og Alpine-specific flags: -fno-pie ug -fno-stack-check)
clang -target i386-pc-none-elf -m32 -ffreestanding -fno-stack-protector -fno-pie -fno-stack-check -O0 -nostdlib -c kernel.c -o kernel.o
ld.lld -m elf_i386 -T linker.ld kernel.o --oformat binary -o kernel.bin

# 4. Disk Image (1.44MB Floppy standard para Alpine-friendly)
dd if=/dev/zero of=sevenos.img bs=1024 count=1440 status=none
dd if=boot.bin of=sevenos.img conv=notrunc status=none
dd if=kernel.bin of=sevenos.img seek=1 conv=notrunc status=none

# 5. Verification
echo -e "${CYAN}[*] Verifying Boot Signature...${NC}"
hexdump -s 510 -n 2 sevenos.img

echo -e "${GREEN}[+] Ready! Launching QEMU...${NC}"

# Kon naggamit kag Alpine sa Termux o Headless, i-try ni:
qemu-system-i386 -drive format=raw,file=sevenos.img -display curses
