; 29/10/26 -- MaXOS bootloader state 1
; Using address:offset (stage2_seg:stage2_off) for jmp to stage2.asm
; Loading from 0x7C00.
; 16 bits.

BITS 16
org 0x7C00

stage_2_lba equ 1
stage_2_sect equ 64
stage2_off equ 0x8000
stage2_seg equ 0x0000


; code section
start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7C00
	sti
	mov [boot_drive], dl
	mov si, msg_hello
	call print16
	; --- extended read via int13 and dap
	mov ah, 0x42
	mov dl, [boot_drive]
	mov si, dap_stage2
	int 0x13
	jc disk_error
	mov dl, [boot_drive]
	jmp stage2_seg:stage2_off
disk_error:
	mov si, msg_disk_err
	call print16
.halt:
	cli
	hlt
	jmp .halt
; tools functions
print16:
	pusha
	mov ah, 0x0E
	mov bx, 0x0007
.loop:
	lodsb
	test al, al
	jz .done
	int 0x10
	jmp .loop
.done:
	popa
	ret

; data section
msg_hello: db "[S1], welcome to maxos. Loading stage2.",13,10,0
msg_disk_err: db "[S1, fatal]: cannot read stage2 from disk. Maybe error in dap, or stage2 not found.",13,10,0

; dap for int13h [ah=42h]
align 4
dap_stage2:
	db 0x10
	db 0
	dw stage_2_sect
	dw stage2_off
	dw stage2_seg
	dd stage_2_lba
	dd 0
boot_drive: db 0

times 510 - ($ - $$) db 0
dw 0xAA55
