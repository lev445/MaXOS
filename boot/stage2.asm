; MaXOS -- bootloader, 16->32->64bit
; enable a20 line
; load kernel
; switch to pe
; enable pae+lme+pg and switch to lm
; jump to kernel

BITS 16
org 0x8000
KERNEL_LBA     equ 2048 ; kernel lives at 1 MiB on disk
KERNEL_SECTORS equ 827 ; 1 MiB budget for kernel.bin
KERNEL_STAGE   equ 0x40000 ; temp landing zone (real-mode reachable)
KERNEL_DEST    equ 0x100000 ; final linked address
CHUNK          equ 64 ; sectors per INT 13h call

BOOTINFO_ADDR equ 0x30400
E820_MAX      equ 32
; from kernel/types.h:boot_info_t (file:structure):
;   0:  magic (4) <BYTES:WHAT>
;   4:  ok (4)
;   8:  width (4)
;  12:  height (4)
;  16:  bpp (4)
;  20:  pitch (4)
;  24:  fb (8)
;  32:  rpos,gpos,bpos (3)
;  35:  rmask,gmask,bmask (3)
;  38:  _pad (2)
;  40:  e820_count (4)
;  44:  e820[] (massive)
BI_E820_COUNT equ 40
BI_E820_MAP   equ 44

; memory map used by this stage (STACK):
;   0x00500            scratch (saved DL)
;   --> 0x08000        stage2 code [Here :)]
;   0x10000            PML4
;   0x11000            PDPT (64 entries)
;   0x200000           512 PD tables (2 MiB, full 512 GiB identity)
;   0x20000            GDT
;   0x40000..          kernel staging area

; GDT selectors
; | null
; | code32
; | data32
; | code64
; | data64


; -------! CODE !-------
start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x8000
	sti
	mov [0x500], dl
	mov si, msg_a20
	call print16
	; enable a20 (fast gate)
	in al, 0x92
	or al, 2
	and al, 0xFE
	out 0x92, al
	mov ax, 0x2401
	int 0x15
	mov si, msg_kernel
	call print16
	mov dword [dap_k.lba], KERNEL_LBA
	mov word [dap_k.seg], (KERNEL_STAGE >> 4)
	mov ebx, KERNEL_SECTORS
.load_loop:
	test ebx, ebx
	jz .load_done
	mov eax, CHUNK
	cmp ebx, eax
	jae .full_chunk
	mov eax, ebx
.full_chunk:
	mov [dap_k.count], ax
	; INT 13h (ah=42h) extended read
	mov ah, 0x42
	mov dl, [0x500]
	mov si, dap_k
	int 0x13
	jc disk_error
	; advance buffer segment (eax sectors * 512 bytes /16)
	shl eax, 5
	add [dap_k.seg], ax
	; ADVANCE lba
	mov eax, [dap_k.count]
	add [dap_k.lba], eax
	sub ebx, eax
	jmp .load_loop
.load_done:
	; -------! VESA DRIVER (real mode!!) !-------
	; INFORMATION: VbeInfoBlock @ 0x30000, ModeInfoBlock @ 0x30200, boot info @ 0x30400
	Mov ax, 0x3000
	mov es, ax
	xor di, di
	mov ax, 0x4F00
	int 0x10
	cmp ax,0x004F
	jne vbe_fail
	mov si, [es:14] ; XXX/TODO: потом чекнуть в gdb че там [1]
	mov bx, [es:16] ; XXX/TODO: потом чекнуть в gdb че там [2]
	xor bp,bp
	mov word [pick_mode], 0xFFFF

vbe_scan:
	mov es, bx
	mov ax, [es:si]
	add si, 2
	cmp ax, 0xFFFF
	je vbe_pick
	mov [cur_mode], ax
	push bx
	push si
	mov ax, 0x3000
	mov es, ax
	mov di, 0x200
	mov cx, [cur_mode]
	mov ax, 0x4F01
	int 0x10
	pop si
	pop bx
	cmp ax, 0x004F
	jne vbe_scan        ; GetModeInfo не удался -> следующий режим
	; BUGFIX: было "mov es, ax", а ax тут = 0x004F (код возврата int 10h).
	; ModeInfoBlock читался из мусорного 0x04F0:0x200, поэтому VBE всегда падал.
	mov ax, 0x3000
	mov es, ax
	mov ax, [es:0x200]
	and ax, 0x0081
	cmp ax, 0x0081
	jne vbe_scan
	cmp byte [es:0x219], 32
	jne vbe_scan
	mov cx, [es:0x212] ; vector:x
	mov dx, [es:0x214] ; vector:y
	cmp cx, 1024
	jne vbe_try800
	cmp dx, 768
	jne vbe_scan
	mov ax, 3
	jmp vbe_prio
vbe_try800:
	cmp cx, 800
	jne vbe_try640
	cmp dx, 600
	jne vbe_scan
	mov ax, 2
	jmp vbe_prio
vbe_try640:
	cmp cx, 640
	jne vbe_scan
	cmp dx, 480
	jne vbe_scan
	mov ax, 1
vbe_prio: 
	cmp ax, bp ; keep strictly better matches only!
	jbe vbe_scan
	mov bp, ax
	mov ax, [cur_mode]
	mov [pick_mode], ax
	jmp vbe_scan
vbe_pick:
	cmp word [pick_mode], 0xFFFF
	je vbe_fail         ; BUGFIX: раньше результат сравнения не использовался,
	                    ; и при pick_mode=0xFFFF вызов 4F01h шёл с мусорным cx
	; fresh ModeInfo for the picked mode (block holds the last scanned one)
	mov ax, 0x3000
	mov es, ax
	mov di, 0x200
	Mov cx, [pick_mode]
	mov ax, 0x4F01
	int 0x10
	cmp ax, 0x004F
	jne vbe_fail
	mov bx, [pick_mode] ; soo.. set new mode: linear framebuffer
	or bx, 0x4000
	mov ax, 0x4F02
	int 0x10
	cmp ax, 0x004F
	jne vbe_fail
	; -- fill boot inf @ 0x30400 --
	mov ax, 0x3000                 ; ES again: 0x4F02 may clobber it
	mov es, ax
	mov esi, 0x30400
	mov dword [esi], 0x31454256    ; 'VBE1'
	mov dword [esi+4], 1           ; ok
	movzx eax, word [es:0x212]
	mov [esi+8], eax               ; width
	movzx eax, word [es:0x214]
	mov [esi+12], eax              ; height
	movzx eax, byte [es:0x219]
	mov [esi+16], eax              ; bpp
	movzx eax, word [es:0x210]
	mov [esi+20], eax              ; bytes per scanline
	mov eax, [es:0x228]
	mov [esi+24], eax              ; PhysBasePtr low
	mov dword [esi+28], 0          ; PhysBasePtr high
	mov al, [es:0x220]
	mov [esi+32], al               ; red field position
	mov al, [es:0x222]
	mov [esi+33], al               ; green field position
	mov al, [es:0x224]
	mov [esi+34], al               ; blue field position
	mov al, [es:0x21F]
	mov [esi+35], al               ; red mask size
	mov al, [es:0x221]
	mov [esi+36], al               ; green mask size
	mov al, [es:0x223]
	mov [esi+37], al               ; blue mask size
	jmp vbe_done
vbe_fail:
	mov esi, 0x30400
	mov dword [esi], 0x31454256
	mov dword [esi+4], 0 ; ok = 0 -> text mode fallback
vbe_done:
	mov si, msg_e820
	call print16
	call get_e820_map
	xor ax, ax
	mov es, ax ; restore es for the paging code
	; - build page tables -
	; pml4[0] -> pdpt @ 0x11000
	; WARNING! I'm using 1GB memory (first map identity), and 2 Mb pages
	; TODO/XXX: fix this (^^^). Idk how lol
	mov edi, 0x10000
	xor eax, eax
	cld
.zero1:
	mov ecx, 1024
	a32 rep stosd ; a32: use full edi in 16bit mode
	cmp edi, 0x12000 ; zero pml4+pdpt (0x10000..0x12000)
	jb .zero1
	mov edi, 0x200000
.zero2:
	mov ecx, 1024
	a32 rep stosd
	cmp edi, 0x400000
	jb .zero2
	mov edi, 0x10000
	mov dword [edi], 0x11007 ; pml4[0] = pdpt | p|rw|u (tree for all)
	mov edi, 0x11000
	mov eax, 0x200007
	mov ecx, 512
.pdpt_fill:
	mov [edi], eax
	add eax, 0x1000
	add edi, 8
	loop .pdpt_fill
	mov edi, 0x200000
	mov eax, 0x83 ; P|RW|PS(2MiB)
	mov ecx, 262144
.pd_fill:
	mov [edi], eax
	add eax, 0x200000
	add edi, 8
	a32 loop .pd_fill
	mov edi, 0x200000 + 16
	mov dword [edi], 0x400087 ; 0x400000 P|RW|U|PS
	mov edi, 0x200000 + 24
	mov dword [edi], 0x600087 ; 0x600000 P|RW|U|PS
	; -------! lets load 64bit GDT !-------
	lgdt [gdt_ptr]
	; -------! lets swith to pm (32bit) !-------
	cli
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	jmp 0x08:pm_entry ; code32 selector
disk_error:
	mov si, msg_disk_err
	call print16
.halt16:
	cli
	hlt
	jmp .halt16
; -------! tools !-------
print16:
	pusha
	mov ah, 0x0E
	mov bx, 0x0007
.ploop:
	lodsb
	test al, al
	jz .pdone
	int 0x10
	jmp .ploop
.pdone:
	popa
	ret

print_hex16:
    pusha
    mov cx, 4
.ph_loop:
    rol ax, 4
    mov bx, ax
    and bx, 0x0F
    add bx, 0x30
    cmp bx, 0x39
    jbe .ph_ok
    add bx, 7
.ph_ok:
    mov [print_buf], bl
    mov si, print_buf
    call print16
    loop .ph_loop
    popa
    ret

print_buf: db 0, 0

; ------! Get E820 memory map from BIOS !-----
get_e820_map:
    pushad
    
    mov ax, 0x3000
    mov es, ax
    
    mov di, BOOTINFO_ADDR & 0xFFFF
    add di, BI_E820_MAP
    
    xor ebx, ebx
    xor esi, esi
    xor bp, bp
    mov edx, 0x534D4150
    mov si, msg_e820_debug
    call debug_print
    
.e820_loop:
    mov eax, 0x0000E820
    mov ecx, 24
    int 0x15
    
    jc .e820_error
    cmp eax, 0x534D4150
    jne .e820_bad_sig
    
    inc bp
    add di, 24
    
    test ebx, ebx
    jz .e820_done
    cmp bp, E820_MAX
    jb .e820_loop
    jmp .e820_done

.e820_error:
    mov si, msg_e820_err
    call debug_print
    jmp .e820_done

.e820_bad_sig:
    mov si, msg_e820_sig
    call debug_print

.e820_done:
    mov si, msg_e820_count
    call debug_print
    mov ax, bp
    call debug_print_hex16
    mov al, 13
    out 0xE9, al
    mov al, 10
    out 0xE9, al
    mov ax, 0x3000
    mov es, ax
    mov di, BOOTINFO_ADDR & 0xFFFF
    add di, BI_E820_COUNT
    movzx eax, bp
    mov dword [es:di], eax
    
    popad
    ret

debug_print:
    pusha
.dp_loop:
    lodsb
    test al, al
    jz .dp_done
    out 0xE9, al
    jmp .dp_loop
.dp_done:
    popa
    ret

; Вывод 16-битного числа в hex через порт 0xE9
debug_print_hex16:
    pusha
    mov cx, 4
.dph_loop:
    rol ax, 4
    mov bx, ax
    and bx, 0x0F
    add bx, 0x30
    cmp bx, 0x39
    jbe .dph_ok
    add bx, 7
.dph_ok:
    mov al, bl
    out 0xE9, al
    loop .dph_loop
    popa
    ret
	
; -------! DATA !-------
msg_hello:    db 0
msg_a20:      db "[S2]: All ok, starting kernel", 13, 10, 0
msg_kernel:   db 0
msg_e820:     db "[S2:E820] Getting e820", 13,10,0
msg_disk_err: db "[S2]: Error, read failed", 13, 10, 0
msg_e820_debug:db "[S2:E820] Starting BIOS call", 13, 10, 0
msg_e820_err:  db "[S2:E820] CF=1 (error)", 13, 10, 0
msg_e820_sig:  db "[S2:E820] Bad signature", 13, 10, 0
msg_e820_count:db "[S2:E820] Entries: 0x", 0
; dap
align 4
cur_mode: dw 0
pick_mode: dw 0xFFFF
dap_k:
	db 0x10
	db 0
.count: dw CHUNK
.off:   dw 0x0000
.seg:   dw (KERNEL_STAGE >> 4)
.lba:   dd KERNEL_LBA
        dd 0

; -------! CODE !-------
align 8
gdt_base:
    dq 0x0000000000000000 ; 0x00 null
gdt_code32: ; 0x08 32-bit code
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data32: ; 0x10 32-bit data
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_code64: ; 0x18 64-bit code
    dw 0x0000, 0x0000
    db 0x00, 0x9A, 0x2F, 0x00
gdt_data64: ; 0x20 64-bit data
    dw 0x0000, 0x0000
    db 0x00, 0x92, 0xCF, 0x00   ; BUGFIX: было 0x0F (limit 64 KiB). В 64-bit
                                ; лимиты данных игнорируются, но 0xCF корректнее.
gdt_end:
gdt_ptr:
    dw gdt_end - gdt_base - 1
    dd gdt_base

; -------! 32-bit pm !-------
BITS 32
pm_entry:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov esp, 0x8000
	; copy kernel
	mov esi, KERNEL_STAGE
	mov edi, KERNEL_DEST
	mov ecx, (KERNEL_SECTORS * 512) / 4
	cld
	rep movsd
	; enable sse and pae
	mov eax, cr4
	or eax, (1 << 5) | (1 << 4) | (1 << 9) | (1 << 10)  ; PAE|PSE|OSFXSR|OSXMMEXCPT
	mov cr4, eax
	; load cr3 = pml4
	mov eax, 0x10000
	mov cr3, eax
	; set lme in efer msr
	mov ecx, 0xC0000080
	rdmsr
	or eax, (1 << 8)
	wrmsr
	; enable paging
	mov eax, cr0
	or eax, (1 << 31) ; pg
	mov cr0, eax
	jmp 0x18:lm_entry ; code64 selector

; -------! 64-bit lm !-------
BITS 64
lm_entry:
	mov ax, 0x20
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov rsp, 0x80000
	; --- LOAD IDT ---
	lidt [idt_ptr]
	; kernel entry expects: nothing special, it sets up its own stack
	mov rax, KERNEL_DEST
	jmp rax

%macro FAULTSTUB 1
align 8
fault_stub_%1:
  %if (%1 == 8) || ((%1 >= 10) && (%1 <= 14)) || (%1 == 17) || (%1 == 21) || (%1 == 29) || (%1 == 30)
  %else
	push qword 0
  %endif
	push qword %1
	jmp fault_common
%endmacro

align 8
%assign VEC 0
%rep 32
FAULTSTUB VEC
%assign VEC VEC+1
%endrep

align 8
fault_common:
	; stack: [vec][err][rip][cs][rflags][rsp][ss]
	push rax
	push rcx
	push rdx
	push rsi
	mov rsi, exc_msg1
	call put_str
	mov rax, [rsp + 4*8 + 0]     ; vector
	mov rcx, 2
	call put_hex
	mov rsi, exc_msg2
	call put_str
	mov rax, [rsp + 4*8 + 8]     ; error code
	mov rcx, 16
	call put_hex
	mov rsi, exc_msg3
	call put_str
	mov rax, [rsp + 4*8 + 16]    ; RIP REGISTER
	mov rcx, 16
	call put_hex
	cmp qword [rsp + 4*8 + 0], 14 ; if CPU get #PF -> also write CR2
	jne .no_cr2
	mov rsi, exc_msg4
	call put_str
	mov rax, cr2
	mov rcx, 16
	call put_hex
.no_cr2:
	mov al, 10
	out 0xE9, al
.fault_hang:
	cli
	hlt
	jmp .fault_hang

put_str:
	push rax
.ps_loop:
	lodsb
	test al, al
	jz .ps_done
	out 0xE9, al
	jmp .ps_loop
.ps_done:
	pop rax
	ret

put_hex:
	push rax
	push rcx
	push rdx
	shl rcx, 2
	sub rcx, 4
.ph_loop:
	mov rdx, rax
	shr rdx, cl
	and edx, 0x0F
	add edx, 0x30
	cmp edx, 0x39
	jbe .ph_ok
	add edx, 7
.ph_ok:
	push rax
	mov eax, edx
	out 0xE9, al
	pop rax
	sub rcx, 4
	jns .ph_loop
	pop rdx
	pop rcx
	pop rax
	ret

exc_msg1: db 13,10,"[KERNEL PANIC] exception vec=0x",0
exc_msg2: db " err=0x",0
exc_msg3: db " rip=0x",0
exc_msg4: db " cr2=0x",0

; --- INTERRUPT TABLE ---
align 8
idt_base:
%assign VEC 0
%rep 32
	dw fault_stub_%+VEC        ; offset 15..0
	dw 0x18                    ; code64 selector
	db 0                       ; IST = 0
	db 0x8E                    ; P=1, DPL=0, type=64-bit interrupt gate
	dw 0                       ; offset 31..16
	dd 0                       ; offset 63..32
	dd 0                       ; reserved
%assign VEC VEC+1
%endrep
idt_end:
idt_ptr:
	dw idt_end - idt_base - 1
	dq idt_base
