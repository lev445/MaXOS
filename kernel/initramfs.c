// 5/11/26 MaXOS - initramfs subsystem
#include "types.h"
#include "arch/x86_64/lib/io.h"
#include "printf.h"
#include "string.h"
#include "heap.h"
#include <stdbool.h>

extern uint8_t _initramfs_start[];
extern uint8_t _initramfs_end[];

#define MAX_NAME_LEN 64
#define MAX_CHILDREN 32

typedef struct fs_node {
    char name[MAX_NAME_LEN];
    uint32_t size;
    uint8_t *data;
    bool is_directory;
    struct fs_node *parent;
    struct fs_node *children[MAX_CHILDREN];
    int child_count;
} fs_node_t;

fs_node_t root_node;
fs_node_t *current_dir;

typedef struct {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} __attribute__((packed)) cpio_header_t;

static void debug_puts(const char *s) {
    while (*s) outb(0xE9, *s++);
}

static uint32_t parse_hex8(const char *s) {
    uint32_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 4;
        if (s[i] >= '0' && s[i] <= '9') result += s[i] - '0';
        else if (s[i] >= 'A' && s[i] <= 'F') result += s[i] - 'A' + 10;
        else if (s[i] >= 'a' && s[i] <= 'f') result += s[i] - 'a' + 10;
    }
    return result;
}

static fs_node_t *create_node(const char *name, bool is_dir, fs_node_t *parent) {
    fs_node_t *node = (fs_node_t *)kmalloc(sizeof(fs_node_t));
    if (!node) {
        debug_puts("*** kmalloc failed!\n");
        return NULL;
    }
    
    memset(node, 0, sizeof(fs_node_t));
    strncpy(node->name, name, MAX_NAME_LEN - 1);
    node->is_directory = is_dir;
    node->parent = parent;
    
    if (parent && parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = node;
    }
    
    return node;
}

fs_node_t *find_child(fs_node_t *dir, const char *name) {
    if (!dir || !dir->is_directory) return NULL;
    for (int i = 0; i < dir->child_count; i++) {
        if (strcmp(dir->children[i]->name, name) == 0) {
            return dir->children[i];
        }
    }
    return NULL;
}

static fs_node_t *resolve_path(const char *path, fs_node_t *root) {
    fs_node_t *current = root;
    const char *p = path;
    
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;
        const char *next_slash = p;
        while (*next_slash != '/' && *next_slash != '\0') {
            next_slash++;
        }
        
        size_t len = next_slash - p;
        if (len == 0 || len >= MAX_NAME_LEN) {
            p = next_slash;
            continue;
        }
        char token[MAX_NAME_LEN];
        memcpy(token, p, len);
        token[len] = '\0';
        fs_node_t *child = find_child(current, token);
        if (!child) {
            child = create_node(token, true, current);
        }
        current = child;
        p = next_slash;
    }
    
    return current;
}

void initramfs_parse(void) {
    uint8_t *ptr = _initramfs_start;
    uint8_t *end = _initramfs_end;
    uint32_t file_count = 0;
    memset(&root_node, 0, sizeof(fs_node_t));
    strncpy(root_node.name, "/", MAX_NAME_LEN);
    root_node.is_directory = true;
    current_dir = &root_node;
    
    while (ptr + 110 <= end) {
        cpio_header_t *hdr = (cpio_header_t *)ptr;
        
        if (hdr->magic[0] != '0' || hdr->magic[1] != '7' || 
            hdr->magic[2] != '0' || hdr->magic[3] != '7' || 
            hdr->magic[4] != '0' || hdr->magic[5] != '1') {
            break;
        }
        
        uint32_t namesize = parse_hex8(hdr->namesize);
        uint32_t filesize = parse_hex8(hdr->filesize);
        uint32_t mode = parse_hex8(hdr->mode);
        
        char *filename = (char *)(ptr + 110);
        
        if (strcmp(filename, "TRAILER!!!") == 0) {
            break;
        }
        
        if (strcmp(filename, ".") == 0 || strcmp(filename, "./") == 0) {
            uint32_t name_padded = (110 + namesize + 3) & ~3;
            uint32_t data_padded = (filesize + 3) & ~3;
            ptr = ptr + name_padded + data_padded;
            continue;
        }
        
        char *clean_path = filename;
        if (filename[0] == '.' && filename[1] == '/') {
            clean_path = filename + 2;
        }
        bool is_dir = (mode & 0040000) != 0;
        const char *last_slash = NULL;
        const char *p = clean_path;
        while (*p) {
            if (*p == '/') last_slash = p;
            p++;
        }
        
        fs_node_t *parent_dir = &root_node;
        const char *actual_name = clean_path;
        
        if (last_slash != NULL) {
            size_t dir_len = last_slash - clean_path;
            if (dir_len >= MAX_NAME_LEN) dir_len = MAX_NAME_LEN - 1;
            
            char dir_path[MAX_NAME_LEN];
            memcpy(dir_path, clean_path, dir_len);
            dir_path[dir_len] = '\0';
            
            parent_dir = resolve_path(dir_path, &root_node);
            actual_name = last_slash + 1;
        }
        fs_node_t *node = create_node(actual_name, is_dir, parent_dir);
        if (node && !is_dir && filesize > 0) {
            uint32_t name_padded = (110 + namesize + 3) & ~3;
            uint8_t *data = ptr + name_padded;
            
            node->data = (uint8_t *)kmalloc(filesize);
            if (node->data) {
                memcpy(node->data, data, filesize);
                node->size = filesize;
            }
            file_count++;
        }
        
        uint32_t name_padded = (110 + namesize + 3) & ~3;
        uint32_t data_padded = (filesize + 3) & ~3;
        ptr = ptr + name_padded + data_padded;
    }
    
    //debug_puts("Tree built! Root has ");
    //char count_buf[12];
    //int len = 0;
    //uint32_t temp = root_node.child_count;
    //if (temp == 0) count_buf[len++] = '0';
    //else {
        //char rev[12]; int rlen = 0;
        //while (temp > 0) { rev[rlen++] = '0' + (temp % 10); temp /= 10; }
        //for (int i = rlen - 1; i >= 0; i--) count_buf[len++] = rev[i];
    //}
    //count_buf[len] = 0;
    //debug_puts(count_buf);
    //debug_puts(" children.\n");
}

fs_node_t *create_file(fs_node_t *dir, const char *name) {
    if (!dir || !dir->is_directory) return NULL;
    if (dir->child_count >= MAX_CHILDREN) return NULL;

    if (find_child(dir, name) != NULL) return NULL;
    
    fs_node_t *node = (fs_node_t *)kmalloc(sizeof(fs_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(fs_node_t));
    strncpy(node->name, name, MAX_NAME_LEN - 1);
    node->is_directory = false;
    node->parent = dir;
    node->size = 0;
    node->data = NULL;
    
    dir->children[dir->child_count++] = node;
    return node;
}


fs_node_t *create_dir(fs_node_t *dir, const char *name) {
	if (!dir || !dir->is_directory) {
		return NULL;
	}
	if (dir->child_count >= MAX_CHILDREN) {
		return NULL;
	}
	// checking : if file is exists
	if (find_child(dir,name) != NULL) {
		return NULL;
	}
	fs_node_t *node = (fs_node_t *)kmalloc(sizeof(fs_node_t)); // PLEASE KERNEL GIVE ME A SOME MEMORY PLEASE 
	if (!node) {
		return NULL;
	}
	// omg kernel huge thanks for some memory
	memset(node,0,sizeof(fs_node_t));
	strncpy(node->name,name,MAX_NAME_LEN -1);
	node->is_directory = true; // bro i'm eat this code and i'm writed false, but need true xD
	node->parent = dir;
	node->size =0;
	node->data = NULL;

	dir->children[dir->child_count++] = node;
	return node;
}

int write_file(fs_node_t *file, const uint8_t *data, uint32_t size) {
	if (!file || file->is_directory) {
		return -1;
	}
	if (file->data) {
		kfree(file->data); // thanks kernel, here your memory
	}
	if (size==0) {
		file->data = NULL;
		file->size = 0;
		return 0;
	}
	file->data = (uint8_t *)kmalloc(size);
	if (!file->data) {
		return -1;
	}
	memcpy(file->data, data,size);
	file->size = size;
	return 0;
}
// 7/11/26 so, initramfs is done???7!!!!!!!
