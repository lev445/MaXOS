#ifndef MAXOS_MSR_H
#define MAXOS_MSR_H
#include <stdint.h>

/* ---- MSR'ы, нужные для SYSCALL / SYSRET ---- */
#define IA32_EFER  		0xC0000080
#define IA32_STAR  		0xC0000081
#define IA32_LSTAR 		0xC0000082
#define IA32_CSTAR 		0xC0000083
#define IA32_FMASK 		0xC0000084
#define IA32_KERNEL_GS_BASE 	0xC0000102

/* ---- биты EFER ---- */
#define EFER_SCE (1ULL << 0)  /* SYSCALL Enable — разрешает SYSCALL/SYSRET */
#define EFER_LME (1ULL << 8)  /* Long Mode Enable                          */
#define EFER_LMA (1ULL << 10) /* Long Mode Active                          */
#define EFER_NXE (1ULL << 11) /* No-Execute Enable                         */

/* ---- биты RFLAGS (для IA32_SFMASK) ---- */
#define EFLAGS_CF (1ULL << 0)
#define EFLAGS_TF (1ULL << 8)   /* trap / single step   */
#define EFLAGS_IF (1ULL << 9)   /* interrupt enable     */
#define EFLAGS_DF (1ULL << 10)  /* direction            */
#define EFLAGS_NT (1ULL << 14)  /* nested task          */
#define EFLAGS_AC (1ULL << 18)  /* alignment check      */

static inline uint64_t rdmsr(uint32_t msr) {
	uint32_t lo, hi;
	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" :: "c"(msr),
                     "a"((uint32_t)val),
                     "d"((uint32_t)(val >> 32)));
}

extern uint64_t syscall_stack_top;

void syscall_init(void);

#endif
