#include "tecnicofs_client_api.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

static int session_id;
static char const *server_pipe;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
	/* TODO: verificar que server_pipe_path é um pipe do servidor ??? */

	unlink(client_pipe_path);
	if(mkfifo(client_pipe_path, 0777) < 0)
		return -1;

	int fcli = open(client_pipe_path, O_RDONLY);
	if(fcli < 0) 
		return -1;
	int fserv = open(server_pipe_path, O_WRONLY);
	if(fserv < 0)
		return -1;

	int op_code = TFS_OP_CODE_MOUNT;
	write(fserv, &op_code, sizeof(char));
	write(fserv, client_pipe_path, sizeof(char)*PIPE_NAME_LENGTH);

	read(fcli, &session_id, sizeof(int));

    return 0;
}

int tfs_unmount() {
	int fserv = open(server_pipe, O_WRONLY);
	if(fserv < 0)
		return -1;

	int op_code = TFS_OP_CODE_UNMOUNT;
	write(fserv, &op_code, sizeof(char));
	write(fserv, &session_id, sizeof(int));

	session_id = -1;

    return 0;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
