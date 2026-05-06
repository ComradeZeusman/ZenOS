# ZenOS

A minimalist, educational x86 operating system built with a GRUB multiboot kernel.

## Overview

ZenOS is a simple operating system developed from scratch for x86 architecture. It uses a GRUB-compatible multiboot entry point, a kernel written in C, GDT/IDT setup, PIC remapping, and a memory map parsed directly from the BIOS via the multiboot info structure.

## Features

- GRUB multiboot 1 compatible entry point
- 32-bit protected mode kernel
- GDT installed from kernel space
- IDT with CPU exception handlers (ISRs 0–31)
- 8259 PIC remapped (IRQs → vectors 0x20–0x2F)
- Memory map from GRUB (`multiboot_mmap_entry_t`), falls back to `mem_upper`
- Physical memory block allocator (`kmalloc` / `kfree`)
- VGA text-mode terminal output

## Project Structure

```
boot.asm          Multiboot header + entry point (_start)
kernel.c          Kernel main, terminal, memory allocator
kernel_types.h    Primitive type definitions (uint32_t, etc.)
multiboot.h       Multiboot 1 structures and constants
gdt.h / gdt.c     Global Descriptor Table
gdt_flush.asm     LGDT + segment register reload
idt.h / idt.c     Interrupt Descriptor Table
isr.asm           ISR stubs (exceptions 0-31, IRQs 0-15)
pic.h / pic.c     8259 PIC remap and EOI helpers
port.h            Inline outb / inb helpers
linker.ld         Linker script (loads at 1 MB, exports _kernel_end)
Makefile          Build + run targets
grub.cfg          GRUB menu entry for ISO boot
```

## Prerequisites

| Tool | Purpose |
|------|---------|
| `i686-elf-gcc` | Cross-compiler for the kernel |
| `nasm` | Assembles `.asm` files |
| `GNU make` | Build system |
| `qemu-system-i386` | Run the OS in an emulator |
| WSL + `grub-common` + `xorriso` | Only needed for `make iso` |

Install WSL dependencies (one time):
```sh
sudo apt install grub-common xorriso
```

## Building

```sh
make          # compiles everything → kernel.elf
make clean    # removes all build artifacts
```

## Running

### Quickest way — no ISO needed

QEMU has built-in multiboot support and can load the ELF directly:

```sh
make run
# equivalent to: qemu-system-i386 -kernel kernel.elf -m 32M
```

### Bootable ISO via GRUB

Builds `zenos.iso` using `grub-mkrescue` inside WSL:

```sh
make iso
```

### Boot the ISO in QEMU

```sh
make run-iso
# equivalent to: qemu-system-i386 -cdrom zenos.iso -m 32M
```

## License

MIT — see [LICENSE](LICENSE).
