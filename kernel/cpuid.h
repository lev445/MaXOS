#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>
void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, 
           uint32_t* ecx, uint32_t* edx);

// CPU brend (48 bytes via null-terminate (\0))
void cpuid_get_brand(char* out);

int cpuid_has_nx(void);
int cpuid_has_sse(void);
int cpuid_has_sse2(void);
int cpuid_has_avx(void);
#endif
