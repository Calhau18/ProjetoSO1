#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT 10

struct arguments {
    int count;
    char* path;
    char* dest_path;
    char* str;
};

void* mt_safety_write_export(void* arguments) {
    struct arguments* args = arguments;
    int fd;
    ssize_t r;

    for (int i=0; i<args->count; i++) {
        fd = tfs_open(args->path, TFS_O_APPEND+TFS_O_CREAT); //Open file
        assert(fd != -1);

        r = tfs_write(fd, args->str, strlen(args->str)+1);//Write 4 bytes
        assert(r == strlen(args->str)+1);

        assert(tfs_close(fd) != -1);
        
        assert(tfs_copy_to_external_fs(args->path, args->dest_path) != -1);
    }

    return NULL;
}

int main(int argc, char** argv) {
    struct arguments args;
    args.path = "/teste_3";
    args.dest_path = "teste_3.txt";
    args.str = "ABC";

    if (argc > 1)
        args.count = atoi(argv[1]);
    else
        args.count = 0;

    char* to_read = malloc(sizeof(char)*strlen(args.str)+1);

    assert(tfs_init() != -1);

    pthread_t tid[THREAD_COUNT];

    // Open THREAD_COUNT amount of threads each performing mt_safety_write_export
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_create(&tid[i], NULL, mt_safety_write_export, &args) != 0 ) {
            printf("Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(args.dest_path, "r");
    assert(fp != NULL);

    for (size_t i=0; i<args.count*THREAD_COUNT; i++) {
        fread(to_read, sizeof(char), strlen(args.str)+1, fp);
        assert(strcmp(args.str, to_read) == 0);
    }

    assert(fclose(fp) != -1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}
