#ifndef TFS_SERVER_H
#define TFS_SERVER_H

#include "operations.h"
#include "common/common.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
// TODO remove
#include <stdio.h>

#define S 20
#define PC_BUF_SIZE 5

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

typedef struct pc_buffer_t {
	int cons_ind;
	int prod_ind;
	void * args[PC_BUF_SIZE];
} PC_buffer_t;

typedef struct session {
	int file_desc;
	pthread_t thread_id;
	PC_buffer_t pc_buffer;
	pthread_mutex_t lock;
	pthread_cond_t cond_var;
} Session;

Session sessions[S];

static bool shutdown;
static pthread_mutex_t sessions_lock;

int exec_mount(int session_id, char* client_pipe_name);
int exec_unmount(int session_id);
int exec_open(int session_id, char* name, int flags);
int exec_close(int session_id, int fhandle);
int exec_write(int session_id, int fhandle, size_t len, char* buffer);
int exec_read(int session_id, int fhandle, size_t len);
int exec_shutdown_aac(int session_id);

#endif // TFS_SERVER_H
