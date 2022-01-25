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

int session_id = -1;
int fserv, fcli;
char* c_pipe_path;
char* s_pipe_path;

int server_open(char* path) {
    if ((fserv = open(path, O_WRONLY)) < 0) 
        return -1;
    return 0;
}

int client_open(char* path) {
    if ((fcli = open(path, O_RDONLY)) < 0)
        return -1;
    return 0;
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char opcode = TFS_OP_CODE_MOUNT;
    strcpy(c_pipe_path, client_pipe_path);
    strcpy(s_pipe_path, server_pipe_path);

    unlink(client_pipe_path); // Make sure the client pipe doesn't exist
    if (mkfifo(client_pipe_path, 0777) < 0) return -1;
    
    char buf[MSG_SIZE];
    size_t size = sizeof(client_pipe_path);
    
    memcpy(buf, client_pipe_path, size);
    if (size < MSG_SIZE) 
        memset(buf+size, '\0', MSG_SIZE-size);
    
    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, buf, MSG_SIZE); // Send the client_pipe_path to the server
    if (close(fserv) != 0) return -1;

    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, &session_id, sizeof(int)); // Read the session_id from the client pipe
    if (close(fcli) != 0) return -1;

    return 0;
}

int tfs_unmount() {
    char opcode = TFS_OP_CODE_UNMOUNT;

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    if (close(fserv) != 0) return -1;

    unlink(c_pipe_path); // Delete the client pipe
    session_id = -1; // Reset the session_id

    return 0;
}

int tfs_open(char const *name, int flags) {
    char opcode = TFS_OP_CODE_OPEN;
    char buf[MSG_SIZE];
    size_t size = sizeof(*name); // IS THIS OK??
    int fhandle;

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    memcpy(buf, name, size);
    if (size < MSG_SIZE) {
        memset(buf+size, '\0', MSG_SIZE-size);
    }
    write(fserv, buf, MSG_SIZE); // Send the opcode, file name and flags to the server
    write(fserv, &flags, sizeof(int));
    if (close(fserv) != 0) return -1;

    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, &fhandle, sizeof(int)); // Read the file handle from the client pipe
    if (close(fcli) != 0) return -1;

    return fhandle;
}

int tfs_close(int fhandle) {
    char opcode = TFS_OP_CODE_CLOSE;
    int ret;

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    write(fserv, &fhandle, sizeof(int));
    if (close(fserv) != 0) return -1;

    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
    if (close(fcli) != 0) return -1;

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char opcode = TFS_OP_CODE_WRITE;
    int ret;

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    write(fserv, &fhandle, sizeof(int));
    write(fserv, &len, sizeof(size_t));
    write(fserv, buffer, sizeof(buffer)); // THIS OK??
    if (close(fserv) != 0) return -1;

    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
    if (close(fcli) != 0) return -1;

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char opcode = TFS_OP_CODE_READ;
    char buf[sizeof(int)+len]; // This ok??

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    write(fserv, &fhandle, sizeof(int));
    write(fserv, &len, sizeof(size_t));
    if (close(fserv) != 0) return -1;


    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, buf, sizeof(int)+len);
    if (close(fcli) != 0) return -1;
    memcpy(buf+sizeof(int), buffer, len); // THIS OK?

    return buf[0];
}

int tfs_shutdown_after_all_closed() {
    char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    int ret;

    if (server_open(s_pipe_path) != 0) return -1;
    write(fserv, &opcode, 1);
    write(fserv, &session_id, sizeof(int));
    if (close(fserv) != 0) return -1;

    if (client_open(c_pipe_path) != 0) return -1;
    read(fcli, &ret, sizeof(int)); // Read the return value from the client pipe
    if (close(fcli) != 0) return -1;

    session_id = -1; // Reset the session_id
    unlink(c_pipe_path); // Delete the client pipe
    return ret;
}