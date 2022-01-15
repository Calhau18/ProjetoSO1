#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT 100
#define MAX_DATA_SIZE 10+256*1024

struct arguments {
    int fd;
    int count;
    size_t size;
    char* str;
};

/*
* Write once and read "n" times, with "n" being the first argument passed to main
**/
void* mt_safety_test_1(void* arguments) {
    struct arguments* args = arguments;

    char buffer[args->size];

    for (int i=0; i<args->count; i++) {
        char* temp_buffer = buffer;

        tfs_read(args->fd, temp_buffer, args->size);
        assert(strcmp(temp_buffer, args->str) == 0);
    }

    return NULL;
}

int main(int argc, char** argv) {
    pthread_t tid[THREAD_COUNT];
    struct arguments args;
    ssize_t r = 0;
    char* path = "/teste_1";
    args.str = "ABC";
    args.size = strlen(args.str)+1;

    if (argc > 1)
        args.count = atoi(argv[1]);
    else
        args.count = 0;
    if (args.size*(size_t)args.count*THREAD_COUNT > MAX_DATA_SIZE) {
        printf("Input too big! Exiting ...\n");
        return 0;
    }

    assert(tfs_init() != -1);

    args.fd = tfs_open(path, TFS_O_CREAT); //Open file
    assert(args.fd != -1);

    for (size_t i=0; i<args.count*THREAD_COUNT; i++) { // Write string a bunch of times to file
        r = tfs_write(args.fd, args.str, args.size);
        assert(r == args.size);
    }

    assert(tfs_close(args.fd) != -1);
    
    args.fd = tfs_open(path, 0);
    assert(args.fd != -1);

    // Open THREAD_COUNT amount of threads each performing mt_safety_single_write
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_create(&tid[i], NULL, mt_safety_test_1, &args) != 0 ) {
            printf("Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }
    assert(tfs_close(args.fd) != -1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}
