// to view how the '*.max' is work, view: doc/x86_64/executable/max.md

/* 9/11/26 -- executable '*.max' files. */

#ifndef LOADER_H
#define LOADER_H

#include "types.h"
#define MAX_MAGIC "MAX1"

typedef struct {
	char magic[4];
	uint32_t entry;
	uint32_t num_sections;
} __attribute__((packed)) max_header_t;

typedef struct {
	uint32_t type; // 0=text, 1=data, 2=bss, 3=rodata
	uint32_t offset; // offset in file
	uint32_t size; // code size
	uint32_t vaddr; // virtual load address
} __attribute__((packed)) max_section_t;

#define SECTION_TEXT   0
#define SECTION_DATA   1
#define SECTION_BSS    2
#define SECTION_RODATA 3

// load and start .max
int load_program(const char *filename);

#endif
