[org 0x7c00]
[bits 16]

KERNEL_OFFSET equ 0x1000    ; Changed to lower memory address for floppy compatibility

; Initialize segments
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; Print boot message
mov si, BOOT_MSG
call print_string

; Save boot drive number
mov [BOOT_DRIVE], dl
mov si, DRIVE_MSG
call print_string
mov al, dl
call print_hex
mov si, NEWLINE
call print_string

; Load kernel from disk
mov si, LOAD_MSG
call print_string

mov bx, KERNEL_OFFSET      ; Destination address
mov dh, 9                 ; Number of sectors to read
mov dl, [BOOT_DRIVE]      ; Drive number

; Reset disk system first
xor ax, ax
int 0x13
jc disk_reset_error

; Try disk read up to 3 times
mov cx, 3                 ; Retry counter
.retry:
    push cx               ; Save retry counter
    
    ; Read sectors
    mov ah, 0x02         ; BIOS read sector function
    mov al, dh           ; Number of sectors to read
    mov ch, 0x00         ; Cylinder 0
    mov dh, 0x00         ; Head 0
    mov cl, 0x02         ; Start from sector 2
    
    int 0x13             ; BIOS interrupt
    jnc .success         ; If carry flag is clear, read was successful
    
    ; Read failed, try reset and retry
    pop cx               ; Restore retry counter
    push ax              ; Save error code
    xor ax, ax           ; Reset disk system
    int 0x13
    pop ax              ; Restore error code
    
    dec cx
    jnz .retry          ; Try again if we haven't run out of retries
    
    ; All retries failed
    mov si, READ_ERROR_MSG
    call print_string
    call print_hex       ; Print error code from AH
    jmp disk_loop

.success:
    pop cx              ; Clean up stack
    
    ; If we get here, kernel loaded successfully
    mov si, SUCCESS_MSG
    call print_string

    ; Switch to protected mode
    cli                       ; Disable interrupts
    lgdt [gdt_descriptor]     ; Load GDT
    mov eax, cr0
    or eax, 0x1              ; Set protected mode bit
    mov cr0, eax
    jmp CODE_SEG:init_pm     ; Far jump to 32-bit code

disk_reset_error:
    mov si, RESET_ERROR_MSG
    call print_string
    mov al, ah              ; Error code in AH
    call print_hex
    jmp disk_loop

disk_loop:
    mov si, RETRY_MSG
    call print_string
    jmp $

[bits 32]
init_pm:
    mov ax, DATA_SEG     ; Update segment registers
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000     ; Update stack position
    mov esp, ebp

    ; Jump to kernel
    jmp KERNEL_OFFSET    ; Jump to our loaded kernel

; Print string routine
print_string:
    pusha
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Print hex value in AL
print_hex:
    pusha
    mov cx, 2           ; Loop counter for 2 hex digits
    mov bh, al          ; Save original value
.loop:
    rol bh, 4           ; Rotate left by 4 bits
    mov al, bh          ; Move current digit to AL
    and al, 0x0F        ; Mask off high nibble
    add al, '0'         ; Convert to ASCII
    cmp al, '9'         ; If greater than '9'
    jle .print
    add al, 7           ; Convert to A-F
.print:
    mov ah, 0x0e        ; BIOS teletype
    int 0x10
    loop .loop
    popa
    ret

; GDT
gdt_start:
gdt_null:               ; Null segment
    dd 0x0
    dd 0x0

gdt_code:              ; Code segment
    dw 0xffff          ; Limit
    dw 0x0             ; Base
    db 0x0             ; Base
    db 10011010b       ; Flags
    db 11001111b       ; Flags + Upper Limit
    db 0x0             ; Base

gdt_data:              ; Data segment
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; Data
BOOT_DRIVE db 0
BOOT_MSG db 'Booting ZenOS...', 13, 10, 0
DRIVE_MSG db 'Boot drive: ', 0
LOAD_MSG db 'Loading kernel...', 13, 10, 0
SUCCESS_MSG db 'Kernel loaded successfully!', 13, 10, 0
RESET_ERROR_MSG db 'Disk reset failed! Error code: ', 0
READ_ERROR_MSG db 'Disk read failed! Error code: ', 0
RETRY_MSG db ' - System halted, please restart', 13, 10, 0
NEWLINE db 13, 10, 0

; Padding and magic number
times 510-($-$$) db 0
dw 0xaa55