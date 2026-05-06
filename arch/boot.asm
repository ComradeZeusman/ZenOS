[bits 32]

; ── Multiboot header ───────────────────────────────────────────────────────────
; Must appear within the first 8 KB of the image and be 4-byte aligned.
; GRUB scans for the magic value and validates the checksum.

MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ (1 << 0) | (1 << 1)   ; align modules + include memory map
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; ── Kernel stack ───────────────────────────────────────────────────────────────
; Allocated in .bss so it doesn't bloat the binary.

section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KiB
stack_top:

; ── Entry point ────────────────────────────────────────────────────────────────
; GRUB calls _start in 32-bit protected mode with:
;   EAX = 0x2BADB002  (multiboot magic)
;   EBX = physical address of multiboot_info_t
; Interrupts are disabled and a flat GDT is already loaded by GRUB.

section .text
global _start
_start:
    mov esp, stack_top          ; set up the kernel stack

    ; Push arguments for kernel_main(uint32_t mb_magic, multiboot_info_t *mbi).
    ; cdecl: push rightmost argument first.
    push ebx                    ; arg 2: multiboot_info_t *mbi
    push eax                    ; arg 1: uint32_t mb_magic

    extern kernel_main
    call kernel_main

    ; kernel_main must never return; if it does, halt the CPU.
    cli
.hang:
    hlt
    jmp .hang
