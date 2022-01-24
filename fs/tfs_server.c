#include "operations.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#define S 1
#define PIPE_NAME_LENGTH 40

static char active_sessions[S][PIPE_NAME_LENGTH];
static bool shutdown;

pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;

int tfs_mount(char const *client_pipe_name){
	/* Only one client shall mount/unmount at one time */
	pthread_mutex_lock(&session_lock);
	if(active_sessions[0] == 0){
		memcpy(active_sessions[0], client_pipe_name, PIPE_NAME_LENGTH);
	}
	pthread_mutex_unlock(&session_lock);
	return 0;
}

int tfs_unmount(int session_id){
	/* Only one client shall mount/unmount at one time */
	pthread_mutex_lock(&session_lock);
	// TODO: change this disgusting shit
	char * str = "0000000000000000000000000000000000000000";
	memcpy(active_sessions[session_id], str, PIPE_NAME_LENGTH);
	pthread_mutex_unlock(&session_lock);
	return 0;
}

int process_mount(int fserv){
	/* get the client pipe name */
	char client_pipe_name[PIPE_NAME_LENGTH];
	read(fserv, client_pipe_name, PIPE_NAME_LENGTH*sizeof(char));

	int ret = tfs_mount(client_pipe_name);

	/* open client pipe for write */
	int fcli = open(client_pipe_name, O_WRONLY);
	if(fcli == -1)
		return -1;

	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_unmount(int session_id, int fcli){
	int ret = tfs_unmount(session_id);
	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_open(int fserv, int fcli){
	char name[PIPE_NAME_LENGTH];
	read(fserv, name, PIPE_NAME_LENGTH*sizeof(char));

	int flags;
	read(fserv, &flags, sizeof(int));

	int ret = tfs_open(name, flags);
	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_close(int fserv, int fcli){
	int fhandle;
	read(fserv, &fhandle, sizeof(int));

	int ret = tfs_close(fhandle);
	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_write(int fserv, int fcli){
	int fhandle;
	read(fserv, &fhandle, sizeof(int));

	size_t len;
	read(fserv, &len, sizeof(size_t));

	char content[len];
	read(fserv, content, len*sizeof(char));

	int ret = (int) tfs_write(fhandle, content, len);
	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_read(int fserv, int fcli){
	int fhandle;
	read(fserv, &fhandle, sizeof(int));

	size_t len;
	read(fserv, &len, sizeof(size_t));

	char buf[len];

	ssize_t ret = tfs_read(fhandle, buf, len);
	write(fcli, &ret, sizeof(int));

	write(fcli, buf, (size_t)ret*sizeof(char));
	return 0;
}

int process_shutdown_aac(int fcli){
	int ret = tfs_destroy_after_all_closed();
	if(ret == 0)
		shutdown = 0;
	write(fcli, &ret, sizeof(int));
	return 0;
}

int process_message(int fserv){
	/* get op code */
	char op_code;
	read(fserv, &op_code, sizeof(char));

	if(op_code == 1){
		process_mount(fserv);
		return 0;
	}

	/* get session id */
	int session_id;
	read(fserv, &session_id, sizeof(int));

	/* open client pipe for write */
	char const * client_pipe_name = active_sessions[session_id];
	int fcli = open(client_pipe_name, O_WRONLY);
	if(fcli == -1){
		return -1;
	}

	switch(op_code){
		// TODO: tentar mudar para os codes como deve ser

		case 2:
			process_unmount(session_id, fcli);
			break;

		case 3:
			process_open(fserv, fcli);
			break;

		case 4:
			process_close(fserv, fcli);
			break;

		case 5:
			process_write(fserv, fcli);
			break;
		
		case 6:
			process_read(fserv, fcli);
			break;
		
		case 7:
			process_shutdown_aac(fcli);
			break;

		default:
	}

	return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

	if(tfs_init() == -1)
		return -1;
	shutdown = false;

	unlink(pipename);
	if(mkfifo(pipename, 0777) < 0)
		exit(1);

	int fserv = open(pipename, O_RDONLY);
	if(fserv < 0) exit(1);

	while(!shutdown){
		process_message(fserv);
	}

	/* Destruction process */
	unlink(pipename);

    return 0;
}

// TODO: check locks/unlocks work
// TODO: implement thread beahviour for different sessions
// TODO: o que acontece se o servidor não conseguir abrir o pipe do cliente?
