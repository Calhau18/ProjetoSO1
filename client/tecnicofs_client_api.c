#include "tecnicofs_client_api.h"
#include <stdlib.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
// TODO remove
#include <stdio.h>

#define MSG_SIZE 40

static int session_id = -1;
static char c_pipe_path[MSG_SIZE];
static char s_pipe_path[MSG_SIZE];
static int fserv, fcli;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
	/* create client pipe */
	unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0777) < 0) return -1;

	strcpy(c_pipe_path, client_pipe_path);
    strcpy(s_pipe_path, server_pipe_path);

	/* Send request to server */
	fserv = open(server_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_MOUNT;
	size_t size = strlen(client_pipe_path);
    char buf[1+MSG_SIZE];

	memcpy(buf, &opcode, 1);
    memcpy(buf+1, client_pipe_path, size);
	memset(buf+1+size, '\0', MSG_SIZE-size);
    if(write(fserv, buf, 1+MSG_SIZE) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	/* Receive answer from server */
	fcli = open(client_pipe_path, O_RDONLY);
    if (fcli == -1) 
		return -1;

    if(read(fcli, &session_id, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return 0;
}

int tfs_unmount() {
	/* Send request to server */
    char opcode = TFS_OP_CODE_UNMOUNT;
	char buf[1+sizeof(int)];
	memcpy(buf, &opcode, 1);
	memcpy(buf+1, &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	int ret;
	if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;

	/* Destroy session */
    if (close(fserv) != 0) return -1;
	if (close(fcli) != 0) return -1;

	if (unlink(c_pipe_path) == -1)
		return -1;
    session_id = -1; // Reset the session_id

    return 0;
}

int tfs_open(char const *name, int flags) {
	/* Send request to server */
	char buf[1+sizeof(int)+MSG_SIZE+sizeof(int)];

    char opcode = TFS_OP_CODE_OPEN;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

    size_t size = strlen(name);
    memcpy(buf+1+sizeof(int), name, size);
	memset(buf+1+sizeof(int)+size, '\0', MSG_SIZE-size);
	memcpy(buf+1+sizeof(int)+MSG_SIZE, &flags, sizeof(int));
    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int fhandle;
    if(read(fcli, &fhandle, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return fhandle;
}

int tfs_close(int fhandle) {
	/* Send request to server */
	char buf[1+2*sizeof(int)];
    char opcode = TFS_OP_CODE_CLOSE;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
	/* Send request to server */
	char buf[1+2*sizeof(int)+sizeof(size_t)+len];

    char opcode = TFS_OP_CODE_WRITE;
    memcpy(buf, &opcode, 1);
	
	memcpy(buf+1, &session_id, sizeof(int));
    
	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));

	memcpy(buf+1+2*sizeof(int), &len, sizeof(size_t));

	memcpy(buf+1+2*sizeof(int)+sizeof(size_t), buffer, len);

	if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	/* Send request to server */
	char buf[1+2*sizeof(int)+sizeof(size_t)];

    char opcode = TFS_OP_CODE_READ;
	memcpy(buf, &opcode, 1);
	
	memcpy(buf+1, &session_id, sizeof(int));
	
	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));
	
	memcpy(buf+1+2*sizeof(int), &len, sizeof(size_t));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
	int ret;
	if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;

    if(read(fcli, buffer, len) <= 0) // can we do this too?
		return -1;
	/* Answer received */

    return ret;
}

int tfs_shutdown_after_all_closed() {
	/* Send request to server */
    char buf[1+sizeof(int)];
	char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */
	if (close(fserv) != 0) return -1;

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

	if (close(fcli) != 0) return -1;

    session_id = -1; // Reset the session_id
	if (unlink(c_pipe_path) == -1)
		return -1;

    return ret;
}
