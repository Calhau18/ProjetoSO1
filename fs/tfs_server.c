#include <pthread.h>
#include <time.h>
#include "tfs_server.h"

bool pc_buffer_is_full(PC_buffer_t * pc_buf){
	return (pc_buf->cons_ind-pc_buf->prod_ind+PC_BUF_SIZE) % PC_BUF_SIZE == 1;
}

bool pc_buffer_is_empty(PC_buffer_t * pc_buf){
	return pc_buf->cons_ind == pc_buf->prod_ind;
}

void pc_buffer_insert(int session_id, void * arg){
	pthread_mutex_lock(&sessions[session_id].lock);

	PC_buffer_t * pc_buf = &(sessions[session_id].pc_buffer);
	if(pc_buf == NULL) {
		printf("pc_buf is NULL\n"); 
		pthread_mutex_unlock(&sessions[session_id].lock);
		return;
	}

	if(!pc_buffer_is_full(pc_buf)){
		pc_buf->args[pc_buf->prod_ind] = arg;
		pc_buf->prod_ind = (pc_buf->prod_ind + 1) % PC_BUF_SIZE;
		
		pthread_mutex_unlock(&sessions[session_id].lock);
		return;
	}

	pthread_mutex_unlock(&sessions[session_id].lock);
	printf("[Main] !!! Cannot insert in PC_buffer associated with session %d\n", session_id);
}

void * pc_buffer_remove(int session_id){
	pthread_mutex_lock(&sessions[session_id].lock);

	PC_buffer_t * pc_buf = &(sessions[session_id].pc_buffer);
	if(pc_buf == NULL){
		pthread_mutex_unlock(&sessions[session_id].lock);
		return NULL;
	}

	if(!pc_buffer_is_empty(pc_buf)){
		void * ret = pc_buf->args[pc_buf->cons_ind];
		pc_buf->args[pc_buf->cons_ind] = NULL;
		pc_buf->cons_ind = (pc_buf->cons_ind + 1) % PC_BUF_SIZE;
		
		pthread_mutex_unlock(&sessions[session_id].lock);
		return ret;
	}

	pthread_mutex_unlock(&sessions[session_id].lock);
	printf("[Worker%d] !!! Cannot remove from PC_buffer.\n", session_id);
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

	while(!ex){
		void * arg = pc_buffer_remove(session_id);
		if(arg == NULL){ 
			return NULL;
			printf("Removed NULL from PC_buffer\n");
		}

		char op_code = *((char*) arg);
		printf("[Worker%d] Running operation %d.\n", session_id, op_code);

		switch(op_code){
			case TFS_OP_CODE_MOUNT:
				Mount_args* m_arg = (Mount_args*) arg;
				exec_mount(session_id, m_arg->client_pipe_name);
				free(m_arg);
				break;

			case TFS_OP_CODE_UNMOUNT:
				exec_unmount(session_id);
				free((Unmount_args*) arg);
				ex = true;
				break;

			case TFS_OP_CODE_OPEN:
				Open_args* o_arg = (Open_args*) arg;
				exec_open(session_id, o_arg->name, o_arg->flags);
				free(o_arg);
				break;

			case TFS_OP_CODE_CLOSE:
				Close_args* c_arg = (Close_args*) arg;
				exec_close(session_id, c_arg->fhandle);
				free(c_arg);
				break;

			case TFS_OP_CODE_WRITE:
				Write_args* w_arg = (Write_args*) arg;
				exec_write(session_id, w_arg->fhandle, w_arg->len, w_arg->buffer);
				free(w_arg->buffer);
				free(w_arg);
				break;

			case TFS_OP_CODE_READ:
				Read_args* r_arg = (Read_args*) arg;
				exec_read(session_id, r_arg->fhandle, r_arg->len);
				free(r_arg);
				break;

			case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
				exec_shutdown_aac(session_id);
				free((Shutdown_aac_args*) arg);
				break;

			default:
				printf("Did not read a legitimate op_code\n");
				return NULL;
		}

		pthread_mutex_lock(&sessions[session_id].lock);
		while(pc_buffer_is_empty(&sessions[session_id].pc_buffer) && !ex){
			printf("[Worker%d] Waiting for activity.\n", session_id);
			pthread_cond_wait(&sessions[session_id].cond_var, &sessions[session_id].lock);
		}
		pthread_mutex_unlock(&sessions[session_id].lock);
	}

	printf("[Worker%d] Exiting start_routine.\n", session_id);
	return NULL;
}

int process_mount(int fserv){
	Mount_args* arg = (Mount_args*) malloc(sizeof(Mount_args));
	arg->op_code = TFS_OP_CODE_MOUNT;

	ssize_t rd = read(fserv, arg->client_pipe_name, PIPE_NAME_LENGTH);
	if (rd <= 0)
		return -1;

	pthread_mutex_lock(&sessions_lock);
	for(int i=0; i<S; i++){
		if(sessions[i].file_desc == 0){
			sessions[i].file_desc = -1;
			pthread_mutex_unlock(&sessions_lock);

			printf("[Main] Calling pc_buffer_insert while mounting session %d\n", i);
			pc_buffer_insert(i, arg);
			int* x = (int*)malloc(sizeof(int)); *x = i;
			pthread_create(&sessions[i].thread_id, NULL, start_routine, x);

			return i;
		}
	}

	pthread_mutex_unlock(&sessions_lock);
	return -1;
}

int exec_mount(int session_id, char * client_pipe_name){
	/* Open client pipe for write */
	/* The pipe shall remain opened until the session is closed */
	int fcli = open(client_pipe_name, O_WRONLY);
	if(fcli == -1)
		return -1;

	pthread_mutex_lock(&sessions_lock);
	sessions[session_id].file_desc = fcli;
	pthread_mutex_unlock(&sessions_lock);

	if(write(fcli, &session_id, sizeof(int)) == -1)
		return -1;

	return 0;
}

/* Returns the value of tfs_unmount on success, -1 otherwise */
int process_unmount(int session_id){
	Unmount_args* arg = (Unmount_args*)malloc(sizeof(Unmount_args));
	arg->op_code = TFS_OP_CODE_UNMOUNT;

	/* Check if works */
	printf("[Main] Calling pc_buffer_insert while unmounting session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;
}

int exec_unmount(int session_id){
	int ret = 0;
	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
		return -1;

	if(close(sessions[session_id].file_desc) == -1)
		return -1;

	sessions[session_id].file_desc = 0;

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

	printf("[Main] Calling pc_buffer_insert while opening session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;
}

int exec_open(int session_id, char* name, int flags){
	int ret = tfs_open(name, flags);

	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
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

	printf("[Main] Calling pc_buffer_insert while closing session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;

}

int exec_close(int session_id, int fhandle){
	int ret = tfs_close(fhandle);

	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
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

	printf("[Main] Calling pc_buffer_insert while writing session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;
}

int exec_write(int session_id, int fhandle, size_t len, char* buffer){
	int ret = (int) tfs_write(fhandle, buffer, len);

	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
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

	printf("[Main] Calling pc_buffer_insert while reading session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;
}

int exec_read(int session_id, int fhandle, size_t len){
	char buf[len+1];
	int ret = (int)tfs_read(fhandle, buf, len);
	buf[ret] = 0;

	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
		return -1;
	printf("[Worker%d] exec_read read \"%s\" with size %d.\n", session_id, buf, ret);

	if(write(sessions[session_id].file_desc, buf, (size_t)ret) == -1){
		return -1;
	}
	return ret;
}

/* Returns the value of tfs_destroy_after_all_closed on success, -1 otherwise */
int process_shutdown_aac(int session_id){
	Shutdown_aac_args* arg = (Shutdown_aac_args*) malloc(sizeof(Shutdown_aac_args));
	arg->op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

	printf("[Main] Calling pc_buffer_insert while killing session %d\n", session_id);
	pc_buffer_insert(session_id, arg);
	pthread_cond_signal(&sessions[session_id].cond_var);

	return 0;

}

int exec_shutdown_aac(int session_id){	
	int ret = tfs_destroy_after_all_closed();
	if(ret == 0)
		shutdown = 0;
	if(write(sessions[session_id].file_desc, &ret, sizeof(int)) == -1)
		return -1;
	return ret;
}

/* Returns the value of the funcion requested by the client if successful, 
 * -1 otherwise */
int process_message(int fserv){
	printf("[Main] Reading input\n");

	/* get op code */
	char op_code;
	/*struct timespec tim, tim2;
   	tim.tv_sec = 0;
   	tim.tv_nsec = 100000000L;
	nanosleep(&tim , &tim2);*/
	ssize_t rd = read(fserv, &op_code, 1);
	if(rd == 0){
		return 1;
	}else if(rd == -1){
		return -1;
		printf("[Main] Ardeu\n");
	}
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
	printf("[Main] Read op_code %d from session %d.\n", op_code, session_id);

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

void sessions_init(){
	for(int i=0; i<S; i++){
		sessions[i].pc_buffer.cons_ind = 0;
		sessions[i].pc_buffer.prod_ind = 0;
		for(int k=0; k<PC_BUF_SIZE; k++){
			sessions[i].pc_buffer.args[k] = NULL;
		}
		pthread_mutex_init(&sessions[i].lock, NULL);
		pthread_cond_init(&sessions[i].cond_var, NULL);
		pthread_mutex_init(&sessions[i].lock, NULL);
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
	
	sessions_init();
	if(tfs_init() == -1)
		return -1;
	shutdown = false;

	while(!shutdown){
		if(process_message(fserv) == 1) {
			printf("RESETTING THE PIPE!\n");
			open(pipename, O_RDONLY);
		}
	}

	if(close(fserv) != 0) 
		return -1;

	/* Destruction process */
	if(unlink(pipename) == -1)
		return -1;
    return 0;
}
