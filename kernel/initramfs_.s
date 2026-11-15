.section .initramfs, "a"
.global _initramfs_start
.global _initramfs_end

_initramfs_start:
	.incbin "initramfs.cpio"
_initramfs_end:
