[bits 32]
global gdt_flush

; void gdt_flush(uint32_t gdt_ptr_addr);
;
; Loads the new GDTR and reloads every segment register so they all
; point at the permanent kernel descriptors rather than the bootloader's
; temporary ones.
;
; Calling convention: cdecl – argument is at [esp+4].
gdt_flush:
    mov eax, [esp+4]        ; pointer to gdt_ptr_t {uint16_t limit; uint32_t base}
    lgdt [eax]              ; load new GDT register

    ; Reload data-segment registers with the kernel data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; CS cannot be changed with a plain MOV – use a far jump to flush
    ; the instruction pipeline and reload CS with the kernel code selector (0x08).
    jmp 0x08:.flush_cs
.flush_cs:
    ret
