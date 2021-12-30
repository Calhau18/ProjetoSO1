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
            if (inode->i_size > 0) {
				if(inode_delete_content(inum) == -1){
					return -1;
				}
                inode->i_size = 0;
				memset(inode->i_data_blocks, -1, 11*sizeof(int));
            }
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

/*
 * Returns:
 * - the index of the nth block with data relative to the inode on success
 * - -2 if the inode does not have that many blocks
 * - -1 on failure
 */
int get_nth_block(inode_t * inode, int n){
	if(inode == NULL || n < 0 || n >= 10 + BLOCK_SIZE / sizeof(int))
		return -1;
	if(n > inode->i_size / BLOCK_SIZE)
		return -2;
	if(n < 10)
		return inode->i_data_blocks[n];
	int* reference_block = (int*)data_block_get(inode->i_data_blocks[10]);
	return *(reference_block + n - 10);
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) { return -1; }

    size_t total_written = 0;
    while (to_write > 0) {
        int starting_block = (int) (file->of_offset / BLOCK_SIZE);
		int block_number = get_nth_block(inode, starting_block);
		if(block_number == -2)
			block_number = data_block_alloc();
		if(block_number == -1)
			return -1;
		void* block = data_block_get(block_number);
		if(block == NULL) return -1;

        size_t position = file->of_offset % BLOCK_SIZE;

        size_t write_now = BLOCK_SIZE - position;
		if(write_now > to_write) write_now = to_write;

        /* Perform the actual write */
        memcpy(block + position, buffer, write_now);

        buffer += write_now;
        file->of_offset += write_now;
        total_written += write_now;
        to_write -= write_now;
    }

	if(file->of_offset > inode->i_size)
		inode->i_size = file->of_offset;

    return (ssize_t)total_written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
	if(to_read > len) to_read = len;
	
	size_t total_read = 0;
	while(to_read > 0){
		int starting_block = (int) (file->of_offset / BLOCK_SIZE);
		int block_number = get_nth_block(inode, starting_block);
		if(block_number == -1) return -1;
		void* block = data_block_get(block_number);
		if(block == NULL) return -1;

		int position = file->of_offset % BLOCK_SIZE;

		size_t read_now = (size_t) (BLOCK_SIZE - position);
		if(read_now > to_read) read_now = to_read;
		
		memcpy(buffer, block + position, read_now);
		
		buffer += read_now;
		file->of_offset += read_now;
		total_read += read_now;
		to_read -= read_now;
	}

    return (ssize_t)total_read;
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
	ssize_t total_read = tfs_read(fhandle, buffer, inode->i_size);
	if(/*total_read == -1 ||*/ total_read < inode->i_size)
		return -1;
    tfs_close(fhandle);

	FILE* fd = fopen(dest_path, "w");
	if(fd == NULL)
		return -1;

	size_t total_written = fwrite(buffer, (size_t)total_read, 1, fd);
	if(total_written != total_read)
		return -1;
    fclose(fd);

	return 0;
}
