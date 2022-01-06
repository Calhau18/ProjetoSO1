#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT 10

struct arguments {
    int count;
    char* path;
    char* str;
};

/*
* Write once and read "n" times, with "n" being the first argument passed to main
**/
void* mt_safety_single_write(void* arguments) {
    struct arguments* args = arguments;
    int fd;
    ssize_t r;

    char buffer[sizeof(args->str)];

    fd = tfs_open(args->path, TFS_O_CREAT); //Open file
    assert(fd != -1);

    r = tfs_write(fd, args->path, 4);//Write 4 bytes

    assert(tfs_close(fd) != -1);

    for (int i=0; i<args->count; i++) {
        char* temp_buffer = buffer;

        fd = tfs_open(args->path, 0); //Open file
        assert(fd != -1);
    
        r = tfs_read(fd, temp_buffer, 4);
        assert(r == 4);
 
        assert(tfs_close(fd) != -1);
    }

    return NULL;
}

int main(int argc, char** argv) {
    struct arguments args;
    args.path = "/teste_1";
    args.str = "ABC";

    if (argc > 1)
        args.count = atoi(argv[1]);
    else
        args.count = 0;

    assert(tfs_init() != -1);

    pthread_t tid[THREAD_COUNT];

    // Open THREAD_COUNT amount of threads each performing mt_safety_single_write
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_create(&tid[i], NULL, mt_safety_single_write, &args) != 0 ) {
            printf("Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<THREAD_COUNT; i++) {
        pthread_join(tid[i], NULL); // TODO: Check if we need to verify this succeeds
    }
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}