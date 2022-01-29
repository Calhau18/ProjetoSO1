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
#define PC_BUFFER_SIZE 5

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
	void * args[PC_BUFFER_SIZE];
} PC_buffer_t;

typedef struct mount_args {
	char op_code;
	char client_pipe_name[PIPE_NAME_LENGTH];
} Mount_args;

typedef struct unmount_args {
	char op_code;
} Unmount_args;

static char active_sessions_name[S][PIPE_NAME_LENGTH];
static int active_sessions[S];
PC_buffer_t pc_buffers[S];
// TODO: check if can be changed
pthread_t tid[S];
pthread_cond_t session_conds[S];
pthread_mutex_t session_locks[S];
static bool shutdown;

static pthread_mutex_t mount_lock;

int exec_mount(int session_id, char* client_pipe_name);
int exec_unmount(int session_id);
/*
int 

// SUSPEITO: tudo isto é suspeito
void pc_buffer_insert(int session_id, void * arg){
	PC_buffer_t pc_buf = pc_buffers[session_id];
	if((pc_buf.cons_index - pc_buf.prod_index + 5) % 5 != 1){
		pc_buf.args[pc_buf.prod_index] = arg;
		pc_buf.prod_index = (pc_buf.prod_index + 1) % 5;
	}
}

// TODO: check paralelism
void * pc_buffer_remove(int session_id){
	PC_buffer_t pc_buf = pc_buffers[session_id];
	if((pc_buf.cons_index - pc_buf.prod_index + 5) % 5 == 0){
		void * ret = pc_buf.args[pc_buf.cons_index];
		pc_buf.cons_index = (pc_buf.cons_index + 1) % 5;
		return ret;
	}
	return NULL;
}

void * start_routine(void * args){
	bool ex = false;
	int session_id = *((int*) args);
	printf("%d\n", session_id);
	pthread_mutex_lock(session_locks+session_id);
	while(!ex){
		void * arg = pc_buffer_remove(session_id);
		if(arg == NULL){
			printf("aaaaaaaaaaaaah\n");
			return NULL;
		}
		char op_code = *((char*) arg);
		switch(op_code){
			case TFS_OP_CODE_MOUNT:
				Mount_args* m_arg = (Mount_args*) arg;
				exec_mount(session_id, m_arg->client_pipe_name);
				break;
			case TFS_OP_CODE_UNMOUNT:
				exec_unmount(session_id);
				ex = true;
				break;

			/*
			case TFS_OP_CODE_OPEN:
				exec_open();
				break;

			case TFS_OP_CODE_CLOSE:
				exec_close();
				break;

			case TFS_OP_CODE_WRITE:
				exec_write();
				break;

			case TFS_OP_CODE_READ:
				exec_read();
				break;

			case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
				exec_shutdown_aac();
				break;
			*/

			default:
		}

		pthread_cond_wait(session_conds+session_id, session_locks+session_id);
	}

	pthread_mutex_unlock(session_locks+session_id);
	return NULL;
}

int process_mount(int fserv){
	Mount_args* arg = (Mount_args*) malloc(sizeof(Mount_args));
	ssize_t rd = read(fserv, arg->client_pipe_name, PIPE_NAME_LENGTH*sizeof(char));
	if (rd <= 0)
		return -1;

	pthread_mutex_lock(&mount_lock);

	for(int i=0; i<S; i++){
		if(active_sessions[i] == 0){
			printf("%d\n", i);
			pc_buffer_insert(i, (void*)arg);
			pthread_create(tid+i, NULL, start_routine, &i);
			pthread_mutex_unlock(&mount_lock);
			return i;
		}
	}

	pthread_mutex_unlock(&mount_lock);
	return -1;
}

int exec_mount(int session_id, char * client_pipe_name){
	/* Open client pipe for write */
	/* The pipe shall remain opened until the session is closed */
	int fcli = open(client_pipe_name, O_WRONLY);
	if(fcli == -1)
		return -1;

	active_sessions[session_id] = fcli;
	memcpy(active_sessions_name[session_id], client_pipe_name, PIPE_NAME_LENGTH);

	if(write(fcli, &session_id, sizeof(int)) == -1)
		return -1;

	return 0;
}

/* Returns the value of tfs_unmount on success, -1 otherwise */
int process_unmount(int session_id){
	Unmount_args* arg = (Unmount_args*)malloc(sizeof(Unmount_args));

	pthread_mutex_lock(&mount_lock);

	/* Check if works */
	pc_buffer_insert(session_id, (void*)arg);
	pthread_cond_signal(session_conds+session_id);

	pthread_mutex_unlock(&mount_lock);

	return 0;
}

int exec_unmount(int session_id){
	int ret = 0;
	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;

	if(close(active_sessions[session_id]) == -1)
		return -1;

	active_sessions[session_id] = 0;
	memset(active_sessions_name[session_id], '\0', PIPE_NAME_LENGTH);

	return ret;
}


/* Returns the value of tfs_open on success, -1 otherwise */
int process_open(int fserv, int session_id){
	char name[PIPE_NAME_LENGTH];
	ssize_t rd = read(fserv, name, PIPE_NAME_LENGTH*sizeof(char));
	if(rd <= 0)
		return -1;

	int flags;
	rd = read(fserv, &flags, sizeof(int));
	if(rd <= 0)
		return -1;

	int ret = tfs_open(name, flags);
	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_close on success, -1 otherwise */
int process_close(int fserv, int session_id){
	int fhandle;
	ssize_t rd = read(fserv, &fhandle, sizeof(int));
	if(rd <= 0)
		return -1;

	int ret = tfs_close(fhandle);
	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_write on success, -1 otherwise */
int process_write(int fserv, int session_id){
	int fhandle;
	ssize_t rd = read(fserv, &fhandle, sizeof(int));
	if(rd <= 0)
		return -1;

	size_t len;
	rd = read(fserv, &len, sizeof(size_t));
	if(rd <= 0)
		return -1;

	char content[len];
	rd = read(fserv, content, len*sizeof(char));
	if(rd <= 0)
		return -1;

	int ret = (int) tfs_write(fhandle, content, len);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_read on success, -1 otherwise */
int process_read(int fserv, int session_id){
	int fhandle;
	ssize_t rd = read(fserv, &fhandle, sizeof(int));
	if(rd <= 0)
		return -1;

	size_t len;
	rd = read(fserv, &len, sizeof(size_t));
	if(rd <= 0)
		return -1;

	char buf[len];

	int ret = (int)tfs_read(fhandle, buf, len);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;

	if(ret != -1)
		if(write(active_sessions[session_id], buf, (size_t)ret*sizeof(char)) == -1)
			return -1;
	return ret;
}

/* Returns the value of tfs_destroy_after_all_closed on success, -1 otherwise */
int process_shutdown_aac(int session_id){
	int ret = tfs_destroy_after_all_closed();
	if(ret == 0)
		shutdown = 0;
	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of the funcion requested by the client if successful, 
 * -1 otherwise */
int process_message(int fserv){
	/* get op code */
	char op_code;
	ssize_t rd = read(fserv, &op_code, sizeof(char));
	if(rd == 0)
		return 1;
	else if(rd == -1)
		return -1;
	// TODO: pensar se temos de ver se o pipe foi fechado em todos os reads

	if(op_code == TFS_OP_CODE_MOUNT){
		return process_mount(fserv);
	}
	/* else */
	/* get session id */
	int session_id;
	rd = read(fserv, &session_id, sizeof(int));
	if(rd <= 0)
		return -1;

	switch(op_code){
		case TFS_OP_CODE_UNMOUNT:
			return process_unmount(session_id);
			break;

		case TFS_OP_CODE_OPEN:
			return process_open(fserv, session_id);
			break;

		case TFS_OP_CODE_CLOSE:
			return process_close(fserv, session_id);
			break;

		case TFS_OP_CODE_WRITE:
			return process_write(fserv, session_id);
			break;
		
		case TFS_OP_CODE_READ:
			return process_read(fserv, session_id);
			break;
		
		case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
			return process_shutdown_aac(session_id);
			break;

		default:
	}

	return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

	/* creating server pipe and opening for read */
	unlink(pipename);
	if(mkfifo(pipename, 0777) < 0)
		exit(1);

	int fserv = open(pipename, O_RDONLY);
	if(fserv < 0) exit(1);
	
	for(int i=0; i<S; i++){
		pthread_mutex_init(session_locks+i, NULL);
		pthread_cond_init(session_conds+i, NULL);
	}

	memset(active_sessions_name, '\0', S*PIPE_NAME_LENGTH);

	if(tfs_init() == -1)
		return -1;
	shutdown = false;

	while(!shutdown){
		if(process_message(fserv) == 1)
			open(pipename, O_RDONLY);
	}

	if(close(fserv) != 0) 
		return -1;

	/* Destruction process */
	if(unlink(pipename) == -1)
		return -1;
    return 0;
}
