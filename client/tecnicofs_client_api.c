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
    char buf[sizeof(char)+MSG_SIZE];

	memcpy(buf, &opcode, sizeof(char));
    memcpy(buf+sizeof(char), client_pipe_path, size);
	memset(buf+sizeof(char)+size, '\0', MSG_SIZE-size);
    if(write(fserv, buf, sizeof(char)+MSG_SIZE*sizeof(char)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	/* Receive answer from server */
	fcli = open(client_pipe_path, O_RDONLY);
    if (fcli == -1) 
		return -1;

    read(fcli, &session_id, sizeof(int)); // Read the session_id from client pipe
	/* Answer received */

    return 0;
}

int tfs_unmount() {
	/* Send request to server */
    char opcode = TFS_OP_CODE_UNMOUNT;
	char buf[sizeof(char)+sizeof(int)];
	memcpy(buf, &opcode, sizeof(char));
	memcpy(buf+sizeof(char), &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	/* TODO: receber resposta do servidor */

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
	char buf[sizeof(char)+sizeof(int)+MSG_SIZE*sizeof(char)+sizeof(int)];

    char opcode = TFS_OP_CODE_OPEN;
	memcpy(buf, &opcode, sizeof(char));

	memcpy(buf+sizeof(char), &session_id, sizeof(int));

    size_t size = strlen(name);
    memcpy(buf+sizeof(char)+sizeof(int), name, size);
	memset(buf+sizeof(char)+sizeof(int)+size, '\0', MSG_SIZE-size);
	memcpy(buf+sizeof(char)+sizeof(int)+MSG_SIZE*sizeof(char), &flags, sizeof(int));
    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int fhandle;
    read(fcli, &fhandle, sizeof(int));
	/* Answer received */

    return fhandle;
}

int tfs_close(int fhandle) {
	/* Send request to server */
	char buf[sizeof(char)+2*sizeof(int)];
    char opcode = TFS_OP_CODE_CLOSE;
	memcpy(buf, &opcode, sizeof(char));

	memcpy(buf+sizeof(char), &session_id, sizeof(int));

	memcpy(buf+sizeof(char)+sizeof(int), &fhandle, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */

	/* Receive answer from server */
    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
	
	/* Answer received */

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
	/* Send request to server */
	char buf[sizeof(char)+2*sizeof(int)+sizeof(size_t)+len*sizeof(char)];

    char opcode = TFS_OP_CODE_WRITE;
    memcpy(buf, &opcode, sizeof(char));
	
	memcpy(buf+sizeof(char), &session_id, sizeof(int));
    
	memcpy(buf+sizeof(char)+sizeof(int), &fhandle, sizeof(int));

	memcpy(buf+sizeof(char)+2*sizeof(int), &len, sizeof(size_t));

	memcpy(buf+sizeof(char)+2*sizeof(int)+sizeof(size_t), buffer, len*sizeof(char));

	if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
	/* Answer received */

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	/* Send request to server */
	char buf[sizeof(char)+2*sizeof(int)+sizeof(size_t)];

    char opcode = TFS_OP_CODE_READ;
	memcpy(buf, &opcode, sizeof(char));
	
	memcpy(buf+sizeof(char), &session_id, sizeof(int));
	
	memcpy(buf+sizeof(char)+sizeof(int), &fhandle, sizeof(int));
	
	memcpy(buf+sizeof(char)+2*sizeof(int), &len, sizeof(size_t));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;

	/* Request sent */

	/* Receive answer from server */
	int ret;
	read(fcli, &ret, sizeof(int));

    read(fcli, buffer, len); // can we do this too?
	/* Answer received */

    return ret;
}

int tfs_shutdown_after_all_closed() {
	/* Send request to server */
    char buf[sizeof(char)+sizeof(int)];
	char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
	memcpy(buf, &opcode, sizeof(char));

	memcpy(buf+sizeof(char), &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1 || errno == EPIPE)
		return -1;
	/* Request sent */
	if (close(fserv) != 0) return -1;

	/* Receive answer from server */
    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
	/* Answer received */

	if (close(fcli) != 0) return -1;

    session_id = -1; // Reset the session_id
	if (unlink(c_pipe_path) == -1)
		return -1;

    return ret;
}
