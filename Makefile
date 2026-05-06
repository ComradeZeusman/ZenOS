CC = i686-elf-gcc
OBJCOPY = i686-elf-objcopy

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -c -ffreestanding
LDFLAGS = -m32 -nostdlib -nostartfiles -nodefaultlibs -static -Wl,-Tlinker.ld

OBJECTS = boot.o kernel.o gdt.o gdt_flush.o idt.o isr.o pic.o pmm.o paging.o terminal.o

all: kernel.elf

boot.o: boot.asm
	nasm -f elf32 boot.asm -o boot.o

kernel.o: kernel.c gdt.h idt.h multiboot.h pmm.h paging.h terminal.h
	$(CC) $(CFLAGS) kernel.c -o kernel.o

gdt.o: gdt.c gdt.h
	$(CC) $(CFLAGS) gdt.c -o gdt.o

gdt_flush.o: gdt_flush.asm
	nasm -f elf32 gdt_flush.asm -o gdt_flush.o

idt.o: idt.c idt.h pic.h
	$(CC) $(CFLAGS) idt.c -o idt.o

pic.o: pic.c pic.h port.h
	$(CC) $(CFLAGS) pic.c -o pic.o

pmm.o: pmm.c pmm.h multiboot.h kernel_types.h
	$(CC) $(CFLAGS) pmm.c -o pmm.o

paging.o: paging.c paging.h kernel_types.h
	$(CC) $(CFLAGS) paging.c -o paging.o

terminal.o: terminal.c terminal.h kernel_types.h
	$(CC) $(CFLAGS) terminal.c -o terminal.o

isr.o: isr.asm
	nasm -f elf32 isr.asm -o isr.o

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

bootloader.bin: bootloader.asm
	nasm -f bin bootloader.asm -o bootloader.bin

os-image: bootloader.bin kernel.bin
	copy /b bootloader.bin+kernel.bin os-image.bin

clean:
	del /Q *.o *.elf *.iso *.bin 2>nul
	if exist isodir rmdir /S /Q isodir