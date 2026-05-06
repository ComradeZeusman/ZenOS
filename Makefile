CC = i686-elf-gcc
OBJCOPY = i686-elf-objcopy

CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -c -ffreestanding
LDFLAGS = -m32 -nostdlib -nostartfiles -nodefaultlibs -static -Wl,-Tlinker.ld

OBJECTS = kernel.o gdt.o gdt_flush.o idt.o isr.o pic.o

all: os-image

kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o

gdt.o: gdt.c gdt.h
	$(CC) $(CFLAGS) gdt.c -o gdt.o

gdt_flush.o: gdt_flush.asm
	nasm -f elf32 gdt_flush.asm -o gdt_flush.o

idt.o: idt.c idt.h pic.h
	$(CC) $(CFLAGS) idt.c -o idt.o

pic.o: pic.c pic.h port.h
	$(CC) $(CFLAGS) pic.c -o pic.o

isr.o: isr.asm
	nasm -f elf32 isr.asm -o isr.o

kernel.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o kernel.elf

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

os-image: bootloader.bin kernel.bin
	copy /b bootloader.bin+kernel.bin os-image.bin

bootloader.bin: bootloader.asm
	nasm -f bin bootloader.asm -o bootloader.bin

clean:
	del /Q *.bin *.o *.elf os-image