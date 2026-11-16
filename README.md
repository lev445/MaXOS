# MaXOS
**MaXOS** -- an open-source operating system
Created by github.com/lev445.
Also thanks for: github.com/Master-Kaban.

# Navigation
Bootloader: `boot/`
Kernel Modules: `kernel/` (main folder for most code)
Drivers here: `kernel/drivers`
File system drivers here: `kernel/fs`
Architectural files: `kernel/arch`
Scheduler/Operations with multitasking: `kernel/sched`.
Test-userspace programs (Hello world and etc.): `userspace/` (Also have `build.sh` -- script for build userspace programs)
initramfs files: `initramfs/`
Documentation: `doc/`

Branches:
`main` -- main branch
`test` -- rc (Release-Candidate) branch

# Contributing 
Contributing rules can be found in: `CONTRIBUTING.md`
**Please follow these rules.**
Thanks for support MaXOS.

# How build
WORK NORMALLY ONLY ON `LINUX/UNIX-LIKE` OS.
If you in Windows, please, use `WSL`.
`make` -- build project
`make run` -- run project in QEMU
`make log` -- save QEMU log to qemu.log
`make debug` -- run gdb for debugging
After build, you can find `os.img` in root-project folder.

# Requirements
**Assembler**
`NASM` -- for bootloder and another kernel code (Using how main assembly)
`GNU assembly` -- less than half is used in the kernel
**Compilation main code**
`GCC` -- for compiling C code
`GNU Make` -- for `makefile`
**run in make run**
`qemu-system-x86_64` -- if you want run MaXOS in qemu (Or download `qemu-full`)

# License
You may use the MaXOS code for both educational purposes and for creating a fork. 
License: MIT.
