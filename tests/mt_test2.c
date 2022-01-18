#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT 100

struct arguments {
    int count;
    char* path;
    char* str;
};

// Read and write "n" times, with "n" being the first argument passed to main
void* mt_safety_reads_writes(void* arguments) {
    struct arguments* args = (struct arguments*) arguments;
    int fd;
    ssize_t r;

    char buffer[sizeof(args->str)];

    for (int i=0; i<args->count; i++) {
        char* temp_buffer = buffer;

        fd = tfs_open(args->path, TFS_O_CREAT); //Open file
        assert(fd != -1);

        r = tfs_write(fd, args->str, sizeof(args->str));//Write 4 bytes
        assert(r==sizeof(args->str));

        assert(tfs_close(fd) != -1);

        fd = tfs_open(args->path, 0); //Open file
        assert(fd != -1);

        r = tfs_read(fd, temp_buffer, sizeof(args->str));
        assert(strcmp(temp_buffer, args->str) == 0);

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

    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_create(&tid[i], NULL, mt_safety_reads_writes, &args) != 0 ) {
            printf("Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}
