/* 4/11/26 MaXOS -- VFS (Virtual File System)*/
#include "vfs.h"
#include "printf.h"
#include "string.h"
#include "heap.h"

static vfs_file_t *root = NULL;
static int file_count = 0;
static vfs_file_t file_pool[VFS_MAX_FILES];

void vfs_init(void) {
	/* create root dir */
	root = &file_pool[file_count++];
	strcpy(root->name, "/");
	root->data = NULL;
	root->size = 0;
	root->flags = 0755;
	root->uid = 0;
	root->gid = 0;
	root->parent = NULL;
	root->children = NULL;
	root->next = NULL;
	//kprintf("VFS initialized.\n");
}


int vfs_read(const char *path, void *buffer, uint32_t size) {
	/* read file from dirr */
    vfs_file_t *root = vfs_get_root();
    vfs_file_t *file = root->children;
    while (file) {
        if (strcmp(file->name, path) == 0) {
            if (file->data && file->size > 0) {
                uint32_t bytes_to_copy = (size < file->size) ? size : file->size;
                memcpy(buffer, file->data, bytes_to_copy);
                return bytes_to_copy;
                }
            return 0;
        }
        file = file->next;
    }
    return -1; // if file is not found
}

int vfs_write(const char *path, const void *buffer, uint32_t size) {
	/* vfs_write is write to memory file via kmalloc(). */
    if (!path || !buffer || size == 0) return -1;
    vfs_file_t *root = vfs_get_root();
    vfs_file_t *file = root->children;
    while (file) {
        if (strcmp(file->name, path) == 0) {
            if (file->data != NULL) {
                kfree(file->data);
            }
            
            file->data = (uint8_t *)kmalloc(size);
            if (!file->data) {
                kprintf("write error: out of memory for '%s'\n", path);
                return -1;
            }
            
            memcpy(file->data, buffer, size);
            file->size = size;
            return size;
        }
        file = file->next;
    }

    vfs_file_t *new_file = vfs_create_file(path, 0644, 0, 0); // 0644 = rw-r--r--
    if (!new_file) {
        kprintf("write error: failed to create file '%s'\n", path);
        return -1;
    }

    new_file->data = (uint8_t *)kmalloc(size);
    if (!new_file->data) {
        kprintf("write error: out of memory for new file '%s'\n", path);
        return -1;
    }

    memcpy(new_file->data, buffer, size);
    new_file->size = size;

    return size; // Ok!
}
int vfs_readdir(const char *path, char *buffer, uint32_t buffer_size) {
	// XXX: at now moment, this function is not created, i'm going to sleep (i'm lazy)
	return 0;
	/* read dir (for example, bin/)*/
}

vfs_file_t *vfs_get_root(void) {
    return root;
}

vfs_file_t *vfs_create_file(const char *path, uint32_t flags, uint32_t uid, uint32_t gid) {
	/* create new file */
    if (file_count >= VFS_MAX_FILES) {
        kprintf("vfs limit.\n");
        return NULL;
    }

    vfs_file_t *file = &file_pool[file_count++];
    
    strncpy(file->name, path, VFS_MAX_PATH - 1);
    file->name[VFS_MAX_PATH - 1] = '\0';
    
    file->data = NULL;
    file->size = 0;
    file->flags = flags;
    file->uid = uid;
    file->gid = gid;
    file->parent = root;
    
    file->next = root->children;
    root->children = file;
    
    return file;
}
