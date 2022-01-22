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

int tfs_destroy_after_all_closed(){
	state_destroy_after_all_closed();
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
    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

	/* Only one file to be oppened at a time */
	file_open_lock();
    int inum = tfs_lookup(name);
    int answer = file_open(inum, name, flags);
	file_open_unlock();
	return answer;
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
	size_t BUFFER_SIZE = 1024;

	int fhandle = tfs_open(source_path, 0);
	if(fhandle == -1)
		return -1;

	FILE* fd = fopen(dest_path, "w");
	if(fd == NULL){
		return -1;
	}

	char* buffer[BUFFER_SIZE];
	ssize_t bytes_read = 1;
	while(bytes_read > 0){
		bytes_read = tfs_read(fhandle, buffer, BUFFER_SIZE);
		if(bytes_read == -1){
			return -1;
		}
		if(bytes_read != 0 && fwrite(buffer, (size_t)bytes_read, 1, fd) == 0){
			return -1;
		}
	}

    if(tfs_close(fhandle) == -1)
		return -1;

    if(fclose(fd) == -1)
		return -1;

	return 0;
}
