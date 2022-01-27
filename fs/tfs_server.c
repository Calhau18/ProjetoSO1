#include "operations.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#define S 1
#define PIPE_NAME_LENGTH 40

enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

static char active_sessions_name[S][PIPE_NAME_LENGTH];
static int active_sessions[S];
static bool shutdown;

/*
 * Activates a session with a client (if it is possible).
 *
 * Returns the session_id where the session is created if successful, 
 * -1 otherwise.
 */
int tfs_mount(char const *client_pipe_name){
	/* open client pipe for write */
	int fcli = open(client_pipe_name, O_WRONLY);
	if(fcli == -1)
		return -1;

	if(active_sessions[0] == 0){
		active_sessions[0] = fcli;
		memcpy(active_sessions_name[0], client_pipe_name, PIPE_NAME_LENGTH);
	}
	return 0;
	/* return session_id */
}

/* Returns the value of tfs_mount on success, -1 otherwise */
int process_mount(int fserv){
	/* get the client pipe name */
	char client_pipe_name[PIPE_NAME_LENGTH];
	ssize_t rd = read(fserv, client_pipe_name, PIPE_NAME_LENGTH*sizeof(char));
	if (rd <= 0)
		return -1;

	int ret = tfs_mount(client_pipe_name);

	if(write(active_sessions[ret], &ret, sizeof(int)) == -1)
		return -1;

	return ret;
}

/* Returns the value of tfs_unmount on success, -1 otherwise */
int process_unmount(int session_id){
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
	if(rd <= 0)
		return -1;

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

	memset(active_sessions_name, '\0', S*PIPE_NAME_LENGTH);

	if(tfs_init() == -1)
		return -1;
	shutdown = false;

	while(!shutdown){
		process_message(fserv);
	}

	if(close(fserv) != 0) 
		return -1;

	/* Destruction process */
	if(unlink(pipename) == -1)
		return -1;
    return 0;
}
