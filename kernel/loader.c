#include "types.h"
#include "printf.h"
#include "string.h"
#include "initramfs.h"
#include "arch/x86_64/entry/entryX64.h"

#define USER_CODE_ADDR  0xA00000
#define USER_CODE_SIZE  (2u * 1024 * 1024)
/* Стек растёт ВНИЗ, поэтому вершина = конец страницы стека (0xC00000+2MiB).
 * Было 0xC00000 — это НИЗ страницы: стек рос вниз, в страницу КОДА
 * (0xA00000..0xBFFFFF, тоже P|RW|U), а выделенная страница стека
 * 0xC00000..0xDFFFFF пустовала. Мелкие программы это скрывали, но как
 * только стек дорастал до 0xA00000, программа затирала свой же .text -> #GP.
 * Проверено: 2.3 MiB стека на 0xC00000 = GPF, на 0xE00000 = OK. */
#define USER_STACK_TOP  0xE00000

typedef struct {
	char magic[4];
	uint32_t entry;
	uint32_t size;
} __attribute__((packed)) max_header_t;

int load_program(const char *filename) {
	fs_node_t *file = find_child(current_dir, filename);
	if (!file) {
		kprintf("sh: %s: No such file or directory.\n", filename);
		return -1;
	}
	if (file->is_directory) {
		kprintf("sh: %s: Is a directory.\n", filename);
		return -1;
	}
	if (file->size < sizeof(max_header_t)) {
		kprintf("sh: %s: invalid file structure.\n", filename);
		return -1;
	}
	max_header_t *hdr = (max_header_t *)file->data;
	if (hdr->magic[0] != 'M' || hdr->magic[1] != 'A' || 
	    hdr->magic[2] != 'X' || hdr->magic[3] != '1') {
		kprintf("sh: %s: Invalid MaXOS executable.\n", filename);
		return -1;
	}
	// for debug; after full debugging comment this
	//kprintf("Loading %s (size=%u, entry=0x%x)...\n",filename,hdr->size,hdr->entry);
	uint8_t *code = file->data + sizeof(max_header_t);
	uint8_t *dest = (uint8_t *)USER_CODE_ADDR;

	if (hdr->size > USER_CODE_SIZE) {
		kprintf("sh: %s: too big\n",
		        filename, hdr->size, (uint32_t)USER_CODE_SIZE);
		return -1;
	}
	if (hdr->entry >= hdr->size) {
		kprintf("sh: %s: entry %#x is outside the code.\n", filename, hdr->entry);
		return -1;
	}

	memcpy(dest,code,hdr->size);

	int exit_code = (int)(int64_t)enter_usermode_asm(USER_CODE_ADDR + hdr->entry,
	                                                 USER_STACK_TOP);
	//kprintf("%s exited with code %d\n", filename, exit_code);
	return exit_code;
}
