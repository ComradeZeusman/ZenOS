[bits 32]

; ISR stub for exceptions WITHOUT an error code pushed by the CPU.
; Push a dummy 0 so the stack layout is uniform.
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

; ISR stub for exceptions WITH an error code already pushed by the CPU.
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1       ; interrupt number (CPU already pushed the error code)
    jmp isr_common_stub
%endmacro

; CPU exception stubs (0-31)
ISR_NOERRCODE  0    ; #DE  Division By Zero
ISR_NOERRCODE  1    ; #DB  Debug
ISR_NOERRCODE  2    ;      Non-Maskable Interrupt
ISR_NOERRCODE  3    ; #BP  Breakpoint
ISR_NOERRCODE  4    ; #OF  Overflow
ISR_NOERRCODE  5    ; #BR  Bound Range Exceeded
ISR_NOERRCODE  6    ; #UD  Invalid Opcode
ISR_NOERRCODE  7    ; #NM  Device Not Available
ISR_ERRCODE    8    ; #DF  Double Fault
ISR_NOERRCODE  9    ;      Coprocessor Segment Overrun (legacy)
ISR_ERRCODE   10    ; #TS  Invalid TSS
ISR_ERRCODE   11    ; #NP  Segment Not Present
ISR_ERRCODE   12    ; #SS  Stack-Segment Fault
ISR_ERRCODE   13    ; #GP  General Protection Fault
ISR_ERRCODE   14    ; #PF  Page Fault
ISR_NOERRCODE 15    ;      Reserved
ISR_NOERRCODE 16    ; #MF  x87 FPU Floating-Point Error
ISR_ERRCODE   17    ; #AC  Alignment Check
ISR_NOERRCODE 18    ; #MC  Machine Check
ISR_NOERRCODE 19    ; #XM  SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; #VE  Virtualization Exception
ISR_NOERRCODE 21    ;      Reserved
ISR_NOERRCODE 22    ;      Reserved
ISR_NOERRCODE 23    ;      Reserved
ISR_NOERRCODE 24    ;      Reserved
ISR_NOERRCODE 25    ;      Reserved
ISR_NOERRCODE 26    ;      Reserved
ISR_NOERRCODE 27    ;      Reserved
ISR_NOERRCODE 28    ;      Reserved
ISR_NOERRCODE 29    ;      Reserved
ISR_ERRCODE   30    ; #SX  Security Exception
ISR_NOERRCODE 31    ;      Reserved

; Hardware IRQ stubs (vectors 0x20-0x2F after PIC remapping).
; None of these have an error code; use ISR_NOERRCODE for all.
ISR_NOERRCODE 32    ; IRQ0  – PIT Timer
ISR_NOERRCODE 33    ; IRQ1  – Keyboard
ISR_NOERRCODE 34    ; IRQ2  – Cascade (internal, never raised)
ISR_NOERRCODE 35    ; IRQ3  – COM2
ISR_NOERRCODE 36    ; IRQ4  – COM1
ISR_NOERRCODE 37    ; IRQ5  – LPT2
ISR_NOERRCODE 38    ; IRQ6  – Floppy disk
ISR_NOERRCODE 39    ; IRQ7  – LPT1 / spurious
ISR_NOERRCODE 40    ; IRQ8  – CMOS RTC
ISR_NOERRCODE 41    ; IRQ9  – Free / ACPI
ISR_NOERRCODE 42    ; IRQ10 – Free
ISR_NOERRCODE 43    ; IRQ11 – Free
ISR_NOERRCODE 44    ; IRQ12 – PS/2 Mouse
ISR_NOERRCODE 45    ; IRQ13 – FPU / coprocessor
ISR_NOERRCODE 46    ; IRQ14 – Primary ATA
ISR_NOERRCODE 47    ; IRQ15 – Secondary ATA / spurious

extern isr_handler

; Common stub called by all ISR stubs.
; Stack on entry (low → high address):
;   int_no | err_code | eip | cs | eflags   (CPU-pushed items are at higher addresses)
; After pusha and push ds the full registers_t struct is on the stack.
isr_common_stub:
    pusha                   ; push eax, ecx, edx, ebx, orig_esp, ebp, esi, edi

    mov ax, ds
    push eax                ; save data segment selector (widened to 32-bit)

    mov ax, 0x10            ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; pass pointer to registers_t as first argument
    call isr_handler
    add esp, 4              ; discard the pointer argument

    pop eax                 ; restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                    ; restore general-purpose registers
    add esp, 8              ; discard int_no and err_code
    iret                    ; restore eip, cs, eflags (and ss/esp if ring change)
