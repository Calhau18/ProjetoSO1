#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
	return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

	return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
			inode_empty_content(inum);
			inode->i_size = 0;
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    return file_write_content(fhandle, buffer, to_write);
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    return file_read_content(fhandle, buffer, len);
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
	int fhandle = tfs_open(source_path, 0);
	if(fhandle == -1)
		return -1;

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if(file == NULL) return -1;

    inode_t *inode = inode_get(file->of_inumber);
    if(inode == NULL) return -1;
	
	void* buffer[inode->i_size];
	size_t read = inode->i_size;
	
	ssize_t total_read = tfs_read(fhandle, buffer, read);
	if(/*total_read == -1 ||*/ total_read < inode->i_size)
		return -1;
    if(tfs_close(fhandle) == -1)
		return -1;

	FILE* fd = fopen(dest_path, "w");
	if(fd == NULL)
		return -1;

	size_t total_written = fwrite(buffer, (size_t)total_read, 1, fd);
	if(total_written != 1)
		return -1;
    if(fclose(fd) == -1)
		return -1;

	return 0;
}
