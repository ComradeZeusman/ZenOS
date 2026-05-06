CC      = i686-elf-gcc
OBJCOPY = i686-elf-objcopy

# -I flags let every source file use plain #include "foo.h" regardless
# of which subdirectory it lives in.
CFLAGS  = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
          -nostartfiles -nodefaultlibs -Wall -Wextra -c -ffreestanding \
          -I include -I kernel -I drivers -I mm

LDFLAGS = -m32 -nostdlib -nostartfiles -nodefaultlibs -static -Wl,-Tlinker.ld

OBJECTS = arch/boot.o \
          kernel/kernel.o kernel/gdt.o arch/gdt_flush.o kernel/idt.o arch/isr.o \
          kernel/klog.o kernel/shell.o kernel/task.o \
          drivers/pic.o drivers/terminal.o drivers/keyboard.o drivers/timer.o \
          mm/pmm.o mm/paging.o mm/heap.o

all: kernel.elf

# ── arch ────────────────────────────────────────────────────────────────
arch/boot.o: arch/boot.asm
	nasm -f elf32 arch/boot.asm -o arch/boot.o

arch/gdt_flush.o: arch/gdt_flush.asm
	nasm -f elf32 arch/gdt_flush.asm -o arch/gdt_flush.o

arch/isr.o: arch/isr.asm
	nasm -f elf32 arch/isr.asm -o arch/isr.o

# ── kernel ──────────────────────────────────────────────────────────────
kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) kernel/kernel.c -o kernel/kernel.o

kernel/gdt.o: kernel/gdt.c kernel/gdt.h
	$(CC) $(CFLAGS) kernel/gdt.c -o kernel/gdt.o

kernel/idt.o: kernel/idt.c kernel/idt.h
	$(CC) $(CFLAGS) kernel/idt.c -o kernel/idt.o

kernel/shell.o: kernel/shell.c kernel/shell.h
	$(CC) $(CFLAGS) kernel/shell.c -o kernel/shell.o

kernel/klog.o: kernel/klog.c kernel/klog.h
	$(CC) $(CFLAGS) kernel/klog.c -o kernel/klog.o

kernel/task.o: kernel/task.c kernel/task.h
	$(CC) $(CFLAGS) kernel/task.c -o kernel/task.o

# ── drivers ─────────────────────────────────────────────────────────────
drivers/pic.o: drivers/pic.c drivers/pic.h
	$(CC) $(CFLAGS) drivers/pic.c -o drivers/pic.o

drivers/terminal.o: drivers/terminal.c drivers/terminal.h
	$(CC) $(CFLAGS) drivers/terminal.c -o drivers/terminal.o

drivers/keyboard.o: drivers/keyboard.c drivers/keyboard.h
	$(CC) $(CFLAGS) drivers/keyboard.c -o drivers/keyboard.o

drivers/timer.o: drivers/timer.c drivers/timer.h
	$(CC) $(CFLAGS) drivers/timer.c -o drivers/timer.o

# ── mm ──────────────────────────────────────────────────────────────────
mm/pmm.o: mm/pmm.c mm/pmm.h
	$(CC) $(CFLAGS) mm/pmm.c -o mm/pmm.o

mm/paging.o: mm/paging.c mm/paging.h
	$(CC) $(CFLAGS) mm/paging.c -o mm/paging.o

mm/heap.o: mm/heap.c mm/heap.h
	$(CC) $(CFLAGS) mm/heap.c -o mm/heap.o

# ── link ────────────────────────────────────────────────────────────────
kernel.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o kernel.elf

# Boot directly with QEMU's built-in multiboot support (no GRUB needed)
run: kernel.elf
	qemu-system-i386 -kernel kernel.elf -m 32M

# Build a bootable ISO with GRUB.
# Requires WSL with grub-common and xorriso: sudo apt install grub-common xorriso
iso: kernel.elf grub.cfg
	if not exist isodir\boot\grub mkdir isodir\boot\grub
	copy kernel.elf isodir\boot\kernel.elf
	copy grub.cfg isodir\boot\grub\grub.cfg
	wsl grub-mkrescue -o zenos.iso isodir

run-iso: zenos.iso
	qemu-system-i386 -cdrom zenos.iso -m 32M

# Legacy floppy image (raw BIOS bootloader, pre-multiboot)
kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

arch/bootloader.bin: arch/bootloader.asm
	nasm -f bin arch/bootloader.asm -o arch/bootloader.bin

os-image: arch/bootloader.bin kernel.bin
	copy /b arch\bootloader.bin+kernel.bin os-image.bin

clean:
	del /Q arch\*.o kernel\*.o drivers\*.o mm\*.o *.elf *.iso *.bin 2>nul
	if exist isodir rmdir /S /Q isodir