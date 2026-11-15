#include "tss.h"
#include "../../string.h"
#include "../../printf.h"
#include "../../panic.h"

#define TSS_SEL 0x38

static tss_t tss __attribute__((aligned(16)));
static uint8_t kstack[16384] __attribute__((aligned(16)));

struct gdt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));


// main point
void tss_init(void) {
	memset(&tss,0,sizeof(tss));
	tss.rsp0 = (uint64_t)(kstack+sizeof(kstack));
	tss.iomap_base = sizeof(tss_t);
	struct gdt_ptr g;
	asm volatile("sgdt %0" : "=m"(g));
	//kprintf("!TSS! gdt base=0x%lx limit=0x%x\n", g.base, g.limit);
	if (g.limit < 0x47) {
		//kprintf("!TSS! error, gdt too small!\n");
		//panic("TSS initialization failed; GDT is too small. Error while initializing GDT.\n");
	}
	uint8_t *d = (uint8_t *)(g.base + TSS_SEL);
	uint64_t base = (uint64_t)&tss;
	uint16_t limit = sizeof(tss_t) -1;
	d[0] = limit & 0xFF;
	d[1] = (limit >> 8) & 0xFF;
	d[2] = base & 0xFF;
	d[3] = (base >> 8) & 0xFF;
	d[4] = (base >> 16) & 0xFF;
	d[5] = 0x89;
	d[6] = 0x00;
	d[7] = (base >> 24) & 0xFF;
	*(uint32_t *)(d + 8) = (uint32_t)(base >> 32);
	*(uint32_t *)(d + 12) = 0;
	asm volatile("ltr %0" :: "r"((uint16_t)TSS_SEL) : "memory");
	uint16_t tr;
	asm volatile("str %0" : "=r"(tr));
	//kprintf("!TSS! loaded, tr=0x%x, rsp0=0x%lx\n", tr, tss.rsp0);
	if (tr != TSS_SEL) {
		panic("TSS initialization failed, TR != 0x38.\n");
	}
}

// завтра доделаю (8/11/26:хз где-то ночью в 5 утра)
// делаю (8/11/26 | 7:55 AM)
// доделал (12/11/26 | 5:15 PM)
