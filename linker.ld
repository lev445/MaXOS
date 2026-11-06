ENTRY(_start)
PHDRS {
	text PT_LOAD FLAGS(5);
	rodata PT_LOAD FLAGS(4);
	data PT_LOAD FLAGS(6);
}
SECTIONS
{
    . = 0x100000;
    __kernel_start = .;

    .text : {
        *(.text.boot)
        *(.text)
        *(.text.*)
    } :text
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        _initramfs_start = .;
        *(.initramfs)
        . = ALIGN(4);
        _initramfs_end = .;
    } :rodata
    
    . = ALIGN(4096);
    .data : {
        *(.data)
        *(.data.*)
    } :data
    
    .bss : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } :data
    
    __kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
