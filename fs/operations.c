#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

int tfs_init() {
	pthread_rwlock_wrlock(&lock);
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
		pthread_rwlock_unlock(&lock);
        return -1;
    }

	pthread_rwlock_unlock(&lock);
    return 0;
}

int tfs_destroy() {
	pthread_rwlock_wrlock(&lock);
    state_destroy();
	pthread_rwlock_unlock(&lock);
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

    int inum = tfs_lookup(name);
	if(inum == -1){
		return -1;
	}

    ssize_t offset = file_open(inum, flags);
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
	/* Editing content from outside the tfs context, do not screw up */
	pthread_rwlock_wrlock(&lock);
	int fhandle = tfs_open(source_path, 0);
	if(fhandle == -1){
		pthread_rwlock_unlock(&lock);
		return -1;
	}
	FILE* fd = fopen(dest_path, "w");
	if(fd == NULL){
		pthread_rwlock_unlock(&lock);
		return -1;
	}

	/* CHANGED: breadman says we're killing our stack */
	/* TODO: definir BUFFER_SIZE (maybe BLOCK_SIZE?) */
	void* buffer[BUFFER_SIZE];
	ssize_t bytes_read = 1;
	while(bytes_read > 0){
		bytes_read == tfs_read(fhandle, buffer, BUFFER_SIZE);
		if(bytes_read == -1){
			pthread_rwlock_unlock(&lock);
			return -1;
		}
		if(bytes_read != 0 && fwrite(buffer, (size_t)bytes_read, 1, fd) == 0){
			pthread_rwlock_unlock(&lock);
			return -1;
		}
	}

    if(tfs_close(fhandle) == -1){
		pthread_rwlock_unlock(&lock);
		return -1;
	}

    if(fclose(fd) == -1){
		pthread_rwlock_unlock(&lock);
		return -1;
	}

	pthread_rwlock_unlock(&lock);
	return 0;
}
