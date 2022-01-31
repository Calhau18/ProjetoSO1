#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "operations.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
// TODO remove
#include <stdio.h>

#define S 5
#define PIPE_NAME_LENGTH 40
#define FILE_NAME_LENGTH 40
#define PC_BUF_SIZE 5

enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

typedef struct pc_buffer_t {
	int cons_index;
	int prod_index;
	void * args[PC_BUF_SIZE];
} PC_buffer_t;

/* Operation_args struct definitions */

typedef struct mount_args {
	char op_code;
	char client_pipe_name[PIPE_NAME_LENGTH];
} Mount_args;

typedef struct unmount_args {
	char op_code;
} Unmount_args;

typedef struct open_args {
	char op_code;
	char name[FILE_NAME_LENGTH];
	int flags;
} Open_args;

typedef struct close_args {
	char op_code;
	int fhandle;
} Close_args;

typedef struct write_args {
	char op_code;
	int fhandle;
	char * buffer;
	size_t len;
} Write_args;

typedef struct read_args {
	char op_code;
	int fhandle;
	size_t len;
} Read_args;

typedef struct shutdown_aac_args {
	char op_code;
} Shutdown_aac_args;

static char active_sessions_name[S][PIPE_NAME_LENGTH];
static int active_sessions[S];
PC_buffer_t pc_buffers[S];
pthread_t tid[S];
// TODO: check if can be changed
pthread_cond_t session_conds[S];
pthread_mutex_t session_locks[S];
static bool shutdown;

static pthread_mutex_t mount_lock;

int exec_mount(int session_id, char* client_pipe_name);
int exec_unmount(int session_id);
int exec_open(int session_id, char* name, int flags);
int exec_close(int session_id, int fhandle);
int exec_write(int session_id, int fhandle, size_t len, char* buffer);
int exec_read(int session_id, int fhandle, size_t len);
int exec_shutdown_aac(int session_id);

#endif // TFS_SERVER_H
