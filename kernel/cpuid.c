#include "cpuid.h"
#include "string.h"
#include "printf.h"

void cpuid(uint32_t leaf, uint32_t* eax_out, uint32_t* ebx_out, 
           uint32_t* ecx_out, uint32_t* edx_out) {
    uint32_t a, b, c, d;
    __asm__ __volatile__(
        "cpuid"
        : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
        : "a" (leaf), "c" (0)
    );
    if (eax_out) *eax_out = a;
    if (ebx_out) *ebx_out = b;
    if (ecx_out) *ecx_out = c;
    if (edx_out) *edx_out = d;
}


void cpuid_get_brand(char* out) {
	uint32_t eax, ebx, ecx, edx;
	cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	if (eax < 0x80000004) {
		strcpy(out, "Unknown");
		return;
	}
	cpuid(0x80000002, &eax, &ebx, &ecx, &edx);
	memcpy(out + 0,  &eax, 4);
	memcpy(out + 4,  &ebx, 4);
	memcpy(out + 8,  &ecx, 4);
	memcpy(out + 12, &edx, 4);

	cpuid(0x80000003, &eax, &ebx, &ecx, &edx);
	memcpy(out + 16, &eax, 4);
	memcpy(out + 20, &ebx, 4);
	memcpy(out + 24, &ecx, 4);
	memcpy(out + 28, &edx, 4);

	cpuid(0x80000004, &eax, &ebx, &ecx, &edx);
	memcpy(out + 32, &eax, 4);
	memcpy(out + 36, &ebx, 4);
	memcpy(out + 40, &ecx, 4);
	memcpy(out + 44, &edx, 4);
	out[48] = '\0';
}

int cpuid_has_nx(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    return (edx >> 20) & 1;  // bit 20
}

int cpuid_has_sse(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx >> 25) & 1;
}

int cpuid_has_sse2(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx >> 26) & 1;
}

int cpuid_has_avx(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (ecx >> 28) & 1;
}
