#include "tfs_server.h"

void pc_buffer_insert(int session_id, void * arg){
	PC_buffer_t * pc_buf = &pc_buffers[session_id];
	if(pc_buf == NULL)
		return;

	if((pc_buf->cons_index - pc_buf->prod_index + PC_BUF_SIZE) % PC_BUF_SIZE != 1){
		pc_buf->args[pc_buf->prod_index] = arg;
		pc_buf->prod_index = (pc_buf->prod_index + 1) % PC_BUF_SIZE;
		return;
	}
	printf("Cannot insert in PC_buffer\n");
}

void * pc_buffer_remove(int session_id){
	PC_buffer_t * pc_buf = &pc_buffers[session_id];
	if(pc_buf == NULL)
		return NULL;
	
	if(pc_buf->cons_index != pc_buf->prod_index){
		void * ret = pc_buf->args[pc_buf->cons_index];
		pc_buf->cons_index = (pc_buf->cons_index + 1) % PC_BUF_SIZE;
		return ret;
	}

	printf("Cannot remove from PC_buffer\n");
	return NULL;
}

void * start_routine(void * args){
	bool ex = false;
	int session_id = *((int*) args);
	free(args);
	if(session_id < 0 || session_id >= S){
		printf("Did not receive valid session id\n");
		return NULL;
	}

	pthread_mutex_lock(session_locks+session_id);

	while(!ex){
		void * arg = pc_buffer_remove(session_id);
		if(arg == NULL){ 
			return NULL;
			printf("Removed NULL from PC_buffer\n");
		}

		char op_code = *((char*) arg);
		printf("op_code is %d\n", op_code);

		switch(op_code){
			case TFS_OP_CODE_MOUNT:
				Mount_args* m_arg = (Mount_args*) arg;
				exec_mount(session_id, m_arg->client_pipe_name);
				break;

			case TFS_OP_CODE_UNMOUNT:
				exec_unmount(session_id);
				ex = true;
				break;

			case TFS_OP_CODE_OPEN:
				Open_args* o_arg = (Open_args*) arg;
				exec_open(session_id, o_arg->name, o_arg->flags);
				break;

			case TFS_OP_CODE_CLOSE:
				Close_args* c_arg = (Close_args*) arg;
				exec_close(session_id, c_arg->fhandle);
				break;

			case TFS_OP_CODE_WRITE:
				Write_args* w_arg = (Write_args*) arg;
				exec_write(session_id, w_arg->fhandle, w_arg->len, w_arg->buffer);
				break;

			case TFS_OP_CODE_READ:
				Read_args* r_arg = (Read_args*) arg;
				exec_read(session_id, r_arg->fhandle, r_arg->len);
				break;

			case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
				exec_shutdown_aac(session_id);
				break;

			default:
				printf("Did not read a legitimate op_code\n");
				return NULL;
		}

		pthread_cond_wait(session_conds+session_id, session_locks+session_id);
	}

	pthread_mutex_unlock(session_locks+session_id);
	return NULL;
}

int process_mount(int fserv){
	Mount_args* arg = (Mount_args*) malloc(sizeof(Mount_args));
	arg->op_code = TFS_OP_CODE_MOUNT;

	ssize_t rd = read(fserv, arg->client_pipe_name, PIPE_NAME_LENGTH);
	if (rd <= 0)
		return -1;

	pthread_mutex_lock(&mount_lock);

	for(int i=0; i<S; i++){
		if(active_sessions[i] == 0){
			pc_buffer_insert(i, arg);
			int* x = (int*)malloc(sizeof(int)); *x = i;
			pthread_create(tid+i, NULL, start_routine, x);

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
	arg->op_code = TFS_OP_CODE_UNMOUNT;

	pthread_mutex_lock(&mount_lock);

	/* Check if works */
	pc_buffer_insert(session_id, arg);
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
	Open_args* arg = (Open_args*) malloc(sizeof(Open_args));
	arg->op_code = TFS_OP_CODE_OPEN;

	ssize_t rd = read(fserv, arg->name, PIPE_NAME_LENGTH);
	if(rd <= 0)
		return -1;

	rd = read(fserv, &(arg->flags), sizeof(int));
	if(rd <= 0)
		return -1;

	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(session_conds+session_id);

	return 0;
}

int exec_open(int session_id, char* name, int flags){
	int ret = tfs_open(name, flags);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_close on success, -1 otherwise */
int process_close(int fserv, int session_id){
	Close_args* arg = (Close_args*) malloc(sizeof(Close_args));
	arg->op_code = TFS_OP_CODE_CLOSE;

	ssize_t rd = read(fserv, &(arg->fhandle), sizeof(int));
	if(rd <= 0)
		return -1;

	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(session_conds+session_id);

	return 0;

}

int exec_close(int session_id, int fhandle){
	int ret = tfs_close(fhandle);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_write on success, -1 otherwise */
int process_write(int fserv, int session_id){
	Write_args* arg = (Write_args*) malloc(sizeof(Write_args));
	arg->op_code = TFS_OP_CODE_WRITE;

	ssize_t rd = read(fserv, &(arg->fhandle), sizeof(int));
	if(rd <= 0)
		return -1;

	rd = read(fserv, &(arg->len), sizeof(size_t));
	if(rd <= 0)
		return -1;

	arg->buffer = (char*)malloc(sizeof(arg->len));
	rd = read(fserv, arg->buffer, arg->len);
	if(rd <= 0)
		return -1;

	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(session_conds+session_id);

	return 0;
}

int exec_write(int session_id, int fhandle, size_t len, char* buffer){
	int ret = (int) tfs_write(fhandle, buffer, len);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of tfs_read on success, -1 otherwise */
int process_read(int fserv, int session_id){
	Read_args* arg = (Read_args*) malloc(sizeof(Read_args));
	arg->op_code = TFS_OP_CODE_READ;

	ssize_t rd = read(fserv, &(arg->fhandle), sizeof(int));
	if(rd <= 0)
		return -1;

	rd = read(fserv, &(arg->len), sizeof(size_t));
	if(rd <= 0)
		return -1;

	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(session_conds+session_id);

	return 0;
}

int exec_read(int session_id, int fhandle, size_t len){
	char buf[len];
	int ret = (int)tfs_read(fhandle, buf, len);

	if(write(active_sessions[session_id], &ret, sizeof(int)) == -1)
		return -1;

	if(ret != -1)
		if(write(active_sessions[session_id], buf, (size_t)ret) == -1)
			return -1;
	return ret;
}

/* Returns the value of tfs_destroy_after_all_closed on success, -1 otherwise */
int process_shutdown_aac(int session_id){
	Shutdown_aac_args* arg = (Shutdown_aac_args*) malloc(sizeof(Shutdown_aac_args));
	arg->op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(session_conds+session_id);

	return 0;

}

int exec_shutdown_aac(int session_id){	
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
	ssize_t rd = read(fserv, &op_code, 1);
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

void server_init(){
	for(int i=0; i<S; i++){
		pc_buffers[i].cons_index = 0;
		pc_buffers[i].prod_index = 0;
		for(int k=0; k<PC_BUF_SIZE; k++){
			pc_buffers[i].args[k] = NULL;
		}
	}
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

	server_init();
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
