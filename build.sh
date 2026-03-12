#!/bin/bash
# --- SevenOS v3.3 Alpine Edition Build Script ---
# "Dili ta mo-undang hangtod dili mo-load!"

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${CYAN}[*] Hard Reset Build...${NC}"

# 1. Limpyo sa mga karaang files
rm -f *.bin *.o *.img os-temp.bin

# 2. Compile Bootloader
echo -e "${YELLOW}[>] Assembling Bootloader...${NC}"
nasm -f bin boot.asm -o boot.bin || { echo -e "${RED}NASM Error!${NC}"; exit 1; }

# 3. Compile Kernel
# Gi-add nato ang -fno-asynchronous-unwind-tables para mas limpyo ang binary
echo -e "${YELLOW}[>] Compiling Kernel...${NC}"
clang -target i386-pc-none-elf -m32 -ffreestanding \
      -fno-stack-protector -fno-pie -fno-stack-check \
      -fno-asynchronous-unwind-tables -O0 -nostdlib \
      -c kernel.c -o kernel.o || { echo -e "${RED}Clang Error!${NC}"; exit 1; }

# 4. Link Kernel ngadto sa Binary
echo -e "${YELLOW}[>] Linking Kernel...${NC}"
ld.lld -m elf_i386 -T linker.ld kernel.o --oformat binary -o kernel.bin || { echo -e "${RED}Linker Error!${NC}"; exit 1; }

# 5. Disk Image Construction (The "Cat" Strategy)
# Gi-combine ang boot.bin (512 bytes) ug kernel.bin para saktong alignment sa Sector 2
echo -e "${YELLOW}[>] Constructing Disk Image...${NC}"
cat boot.bin kernel.bin > os-temp.bin

# Paghimo og 1.44MB image ug i-burn ang combined binary
dd if=/dev/zero of=sevenos.img bs=1024 count=1440 status=none
dd if=os-temp.bin of=sevenos.img conv=notrunc status=none

# Limpyo sa temp file
rm os-temp.bin

# 6. Verification
echo -e "${CYAN}[*] Verifying Image...${NC}"
KERNEL_SIZE=$(stat -c%s "kernel.bin")
echo -e "Kernel Size: ${GREEN}$KERNEL_SIZE bytes${NC}"

if [ $KERNEL_SIZE -gt 32768 ]; then
    echo -e "${RED}[!] WARNING: Kernel is larger than 64 sectors (32KB)!${NC}"
    echo -e "${RED}I-update ang 'mov al, 64' sa boot.asm ngadto sa mas dako.${NC}"
fi

echo -e "${CYAN}[*] Verifying Boot Signature (Should be 55 aa)...${NC}"
hexdump -s 510 -n 2 sevenos.img

echo -e "${GREEN}[+] Build Successful! Launching QEMU...${NC}"

# 7. Execution
qemu-system-i386 -drive format=raw,file=sevenos.img -display curses
