#ifndef INITRAMFS_H
#define INITRAMFS_H

#include "types.h"
#include <stdbool.h>

#define MAX_NAME_LEN 64
#define MAX_CHILDREN 32

// node of file system
typedef struct fs_node {
	char name[MAX_NAME_LEN];
	uint32_t size;
	uint8_t *data;
	bool is_directory;
	struct fs_node *parent;
	struct fs_node *children[MAX_CHILDREN];
	int child_count;
} fs_node_t;

extern fs_node_t root_node;
extern fs_node_t *current_dir;

// all functions 
void initramfs_init(void *initrd_start, uint32_t initrd_size);
void initramfs_parse(void);
fs_node_t *find_child(fs_node_t *dir, const char *name);
fs_node_t *create_file(fs_node_t *dir, const char *name);
fs_node_t *create_dir(fs_node_t *dir, const char *name);
int write_file(fs_node_t *file, const uint8_t *data, uint32_t size);

#endif
