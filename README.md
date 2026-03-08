# 🚀 SevenOS v3.3 - Alpine Edition
**Architect Elite: Master Aloy**

SevenOS is a custom-built, 32-bit x86 Operating System developed from scratch using C and Assembly. It features a persistent file system, real-time clock, and a custom shell environment.

---

## 🛠 Features
* **Persistent File System:** Save and load files directly to disk sectors (Sector 10+).
* **Custom 7shell:** Interactive terminal with commands like `LS`, `SV`, `RD`, and `DT`.
* **Real-Time Clock (RTC):** Live time tracking displayed on a custom status bar.
* **Security Tools:** Built-in [F5-CRACK] and session restoration logic.
* **Bootloader:** Custom x86 entry point with VGA debug signals.

---

## 📸 Screenshots
> *Operating in Termux/Alpine environment using QEMU.*
(Diri nimo i-upload imong screenshots puhon!)

---

## 💻 How to Build
1. **Toolchain:** Requires `gcc` (i686-elf-gcc recommended) and `nasm`.
2. **Compile:**
   ```bash
   nasm -f bin boot.asm -o boot.bin
   gcc -m32 -c kernel.c -o kernel.o
   ld -m elf_i386 -T linker.ld -o sevenos.bin
