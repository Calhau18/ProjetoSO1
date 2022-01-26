#include "tecnicofs_client_api.h"
#include <stdlib.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MSG_SIZE 40

static int session_id = -1;
static char c_pipe_path[MSG_SIZE];
static char s_pipe_path[MSG_SIZE];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
	/* create client pipe */
	unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0777) < 0) return -1;
    
    strcpy(c_pipe_path, client_pipe_path);
    strcpy(s_pipe_path, server_pipe_path);

	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_MOUNT;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    char buf[MSG_SIZE];
    size_t size = strlen(client_pipe_path);
    memcpy(buf, client_pipe_path, size);
	memset(buf+size, '\0', MSG_SIZE-size);
    if(write(fserv, buf, MSG_SIZE) == -1)
		return -1;

    if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
    if (fcli == -1) 
		return -1;

    read(fcli, &session_id, sizeof(int)); // Read the session_id from client pipe

    if (close(fcli) != 0) return -1;
	/* Answer received */

    return 0;
}

int tfs_unmount() {
	/* Send request to server */
    int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_UNMOUNT;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;

    if (close(fserv) != 0) return -1;

	/* TODO: receber resposta do servidor */

	if(unlink(c_pipe_path) == -1)
		return -1;
    session_id = -1; // Reset the session_id

    return 0;
}

int tfs_open(char const *name, int flags) {
	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_OPEN;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;

    char buf[MSG_SIZE];
    size_t size = strlen(name);
    memcpy(buf, name, size);
	memset(buf+size, '\0', MSG_SIZE-size);
    if(write(fserv, buf, MSG_SIZE) == -1)
		return -1;

    if(write(fserv, &flags, sizeof(int)) == -1)
		return -1;

    if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int fhandle;
    read(fcli, &fhandle, sizeof(int));

    if (close(fcli) != 0) return -1;
	/* Answer received */

    return fhandle;
}

int tfs_close(int fhandle) {
	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_CLOSE;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;

    if(write(fserv, &fhandle, sizeof(int)) == -1)
		return -1;

    if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_WRITE;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;
    
	if(write(fserv, &fhandle, sizeof(int)) == -1)
		return -1;
    
	if(write(fserv, &len, sizeof(size_t)) == -1)
		return -1;
    
	if(write(fserv, buffer, len) == -1)
		return -1;

    if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_READ;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;
    
	if(write(fserv, &fhandle, sizeof(int)) == -1)
		return -1;
    
	if(write(fserv, &len, sizeof(size_t)) == -1)
		return -1;
    
	if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

	int ret;
	read(fcli, &ret, sizeof(int));

    read(fcli, buffer, len);

    if (close(fcli) != 0) return -1;
	/* Answer received */

    return ret;
}

int tfs_shutdown_after_all_closed() {
	/* Send request to server */
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    if(write(fserv, &opcode, sizeof(char)) == -1)
		return -1;

    if(write(fserv, &session_id, sizeof(int)) == -1)
		return -1;

    if (close(fserv) != 0) return -1;
	/* Request sent */

	/* Receive answer from server */
	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;
	/* Answer received */

    session_id = -1; // Reset the session_id
	if(unlink(c_pipe_path) == -1)
		return -1;

    return ret;
}
