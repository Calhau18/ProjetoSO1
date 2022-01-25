#include "tecnicofs_client_api.h"
#include <stdlib.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

// Notes:
// - Possibly unnecessary session_id check
// - Possibly unnecessary valid_pathname check
// - do i need to verify close has succesfully closed?
// Check the rest of stuff

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

	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_MOUNT;
    write(fserv, &opcode, sizeof(char));

    char buf[MSG_SIZE];
    size_t size = strlen(client_pipe_path);
    memcpy(buf, client_pipe_path, size);
	memset(buf+size, '\0', MSG_SIZE-size);
    write(fserv, buf, MSG_SIZE); // Send the client_pipe_path to the server

    if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
    if (fcli == -1) 
		return -1;

    read(fcli, &session_id, sizeof(int)); // Read the session_id from client pipe

    if (close(fcli) != 0) return -1;

    return 0;
}

int tfs_unmount() {
    int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_UNMOUNT;
    write(fserv, &opcode, sizeof(char));
    write(fserv, &session_id, sizeof(int));

    if (close(fserv) != 0) return -1;

    unlink(c_pipe_path);
    session_id = -1; // Reset the session_id

    return 0;
}

int tfs_open(char const *name, int flags) {
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_OPEN;
    write(fserv, &opcode, sizeof(char));

    write(fserv, &session_id, sizeof(int));

    char buf[MSG_SIZE];
	// TODO
    size_t size = strlen(name);
    memcpy(buf, name, size);
	memset(buf+size, '\0', MSG_SIZE-size);
    write(fserv, buf, MSG_SIZE); // Send the opcode, file name and flags to the server

    write(fserv, &flags, sizeof(int));

    if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int fhandle;
    read(fcli, &fhandle, sizeof(int)); // Read the file handle from the client pipe

    if (close(fcli) != 0) return -1;

    return fhandle;
}

int tfs_close(int fhandle) {
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_CLOSE;
    write(fserv, &opcode, sizeof(char));

    write(fserv, &session_id, sizeof(int));

    write(fserv, &fhandle, sizeof(int));

    if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_WRITE;
    write(fserv, &opcode, sizeof(char));

    write(fserv, &session_id, sizeof(int));
    
	write(fserv, &fhandle, sizeof(int));
    
	write(fserv, &len, sizeof(size_t));
    
	write(fserv, buffer, len);

    if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_READ;
    write(fserv, &opcode, sizeof(char));

    write(fserv, &session_id, sizeof(int));
    
	write(fserv, &fhandle, sizeof(int));
    
	write(fserv, &len, sizeof(size_t));
    
	if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

	int ret;
	read(fcli, &ret, sizeof(int));

    read(fcli, buffer, len);

    if (close(fcli) != 0) return -1;

    return ret;
}

int tfs_shutdown_after_all_closed() {
	int fserv = open(s_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    write(fserv, &opcode, sizeof(char));

    write(fserv, &session_id, sizeof(int));

    if (close(fserv) != 0) return -1;

	int fcli = open(c_pipe_path, O_RDONLY);
	if(fcli == -1)
		return -1;

    int ret;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe

    if (close(fcli) != 0) return -1;

    session_id = -1; // Reset the session_id
	// TODO: temos de fazer isto?
    unlink(c_pipe_path); // Delete the client pipe
    return ret;
}

// TODO: podemos assumir que read e write funcionam?
