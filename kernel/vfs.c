/* 4/11/26 MaXOS -- VFS (Virtual File System)*/
#include "vfs.h"
#include "printf.h"
#include "string.h"

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
	kprintf("[initramfs] VFS initialized\n");
}


int vfs_read(const char *path, void *buffer, uint32_t size) {
	/* fint file via path */
	vfs_file_t *file = root->children;
	while (file) {
	    if (strcmp(file->name, path) == 0) {
	        if (file->data && file->size <= size) {
	            memcpy(buffer, file->data, file->size);
	            return file->size;
	        }
	        return -1;
	    }
	    file = file->next;
	}
	return -1; // file not found
}

int vfs_write(const char *path, const void *buffer, uint32_t size) {
	/* write file to dirr*/
	// first, if file is available
	vfs_file_t *file = root->children;
	while (file) {
		if (strcmp(file->name, path) ==0) {
			// file is availabe, update date
			/*warning, this is save the ptr! 
             * Please, variable 'buffer' MUST to be 'static'.
			*/
			file->data = (uint8_t *)buffer;
			file->size = size;
			return size;
		}
		file = file->next;
	}
	// second, is file not found, we create new!
	vfs_file_t *new_file = vfs_create_file(path, 0644,0,0);
	if (new_file) {
		new_file->data = (uint8_t *)buffer;
		new_file->size = size;
		return size;
	}
	return -1; // 
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
    if (file_count >= VFS_MAX_FILES) {
        kprintf("[vfs] Error: VFS_MAX_FILES limit reached!\n");
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
