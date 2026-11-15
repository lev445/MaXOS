/* 4/11/26 MaXOS: VFS (virtual file system).
 * created for initramfs and inicpio.cpio.
*/

#ifndef VFS_H
#define VFS_H
#include "types.h"

#define VFS_MAX_PATH 256
#define VFS_MAX_FILES 1024

typedef struct vfs_file {
	char name[VFS_MAX_PATH];
	uint8_t *data; // ptr to data [for ram] or to NULL [for disk]
	uint32_t size;
	uint32_t flags; // access rights
	uint32_t uid, gid; // owner
	struct vfs_file *parent;
	struct vfs_file *children;
	struct vfs_file *next;
} vfs_file_t;

/* Initialization VFS*/
void vfs_init(void);
/* Mount file system*/
int vfs_mount(const char *path, const char *fs_type, void *device);
/* Read file*/
int vfs_read(const char *path, void *buffer, uint32_t size);
/* Write file*/
int vfs_write(const char *path, const void *buffer, uint32_t size);
/* File list in directory*/
int vfs_readdir(const char *path, char *buffer, uint32_t buffer_size);
vfs_file_t *vfs_create_file(const char *path, uint32_t flags, uint32_t uid, uint32_t gid);
vfs_file_t *vfs_get_root(void);
#endif
