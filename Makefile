# MaXOS Makefile
ASM      = nasm
LD       = ld
OBJCOPY  = objcopy
CC       = gcc
QEMU     = qemu-system-x86_64

ASMFLAGS_BIN = -f bin -w-no-implicit-abs-deprecated
ASMFLAGS_ELF = -f elf64 -Werror
LDFLAGS      = -T linker.ld -nostdlib

KCFLAGS = -std=gnu11 -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
          -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-3dnow \
          -fno-asynchronous-unwind-tables \
          -Wall -Wextra -Wno-unused-parameter -O2 -g

all: os.img

boot.bin: boot/boot.asm
	$(ASM) $(ASMFLAGS_BIN) $< -o $@

stage2.bin: boot/stage2.asm
	$(ASM) $(ASMFLAGS_BIN) $< -o $@ -l stage2.lst

#kernel/cpuid.o: kernel/cpuid.asm
#	nasm -f elf64 $< -o $@

KASM_OBJS = $(patsubst %.asm,%.o,$(shell find kernel -name "*.asm")) \
            $(patsubst %.s,%.o,$(shell find kernel -name "*.s"))

C_SOURCES = $(shell find kernel -name "*.c")
KC_OBJS = $(patsubst %.c,%.o,$(C_SOURCES))
KOBJS     = $(KASM_OBJS) $(KC_OBJS)

%.o: %.asm
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

kernel/%.o: kernel/%.s
	as $< -o $@

kernel/initramfs_.o: kernel/initramfs_.s initramfs.cpio
	as $< -o $@

initramfs.cpio: $(shell find initramfs -type f)
	( cd initramfs && find . | LC_ALL=C sort | cpio -o -H newc --quiet ) > $@

kernel/%.o: kernel/%.c
	@mkdir -p kernel/listing
	$(CC) $(KCFLAGS) -Wa,-adhln=kernel/listing/$(notdir $(@:.o=.lst)) -c $< -o $@

kernel.elf: $(KOBJS)
	ld -n -T linker.ld -nostdlib $(KOBJS) -o kernel.elf

# build flat image (*.bin)
kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@
	@sz=$$(stat -c%s $@); \
	if [ $$sz -gt 1048576 ]; then \
	  echo "error, kernel.bin = $$sz b, but limit = 1048576"; \
	  exit 1; \
	fi; echo "kernel.bin ok: $$sz byte from 1048576"

# build image
os.img: boot.bin stage2.bin kernel.bin
	dd if=/dev/zero of=$@ bs=1M count=10
	dd if=boot.bin   of=$@ bs=512 seek=0 conv=notrunc
	dd if=stage2.bin of=$@ bs=512 seek=1 conv=notrunc
	dd if=kernel.bin of=$@ bs=512 seek=2048 conv=notrunc

# start image
run: os.img
	@echo "--- debug ---"
	$(QEMU) -drive file=$<,format=raw -m 128M -no-reboot -debugcon stdio
	
# full log -> qemu.log
log: os.img
	$(QEMU) -drive file=$<,format=raw -m 128M -no-reboot -debugcon stdio \
	        -d int,cpu_reset -D qemu.log

# debug (with gdb)
debug: os.img
	$(QEMU) -drive file=$<,format=raw -m 128M -no-reboot -s -S -debugcon stdio &
	@echo "----- QEMU is started via gdb-debug. Use:"
	@echo "+++++ gdb -ex 'target remote :1234' -ex 'symbol-file kernel.elf'"

# clean *.object files and kernel bin
clean:
	rm -f *.bin *.o *.img kernel.elf qemu.log kernel/*.o *.lst
	find . -name "*.o" -type f -delete
	
.PHONY: all run log debug clean
