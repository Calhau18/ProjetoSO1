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
    ssize_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
	if(inum == -1){
		return -1;
	}

	offset = file_open(inum, flags);
	if(offset = -1){
		return -1;
	}

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { 
	return remove_from_open_file_table(fhandle); 
}

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

	ssize_t size = get_file_size(fhandle);
	if(size == -1)
		return -1;

	void* buffer[size];
	ssize_t total_read = tfs_read(fhandle, buffer, size);
	if(/*total_read == -1 ||*/ total_read < size)
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
