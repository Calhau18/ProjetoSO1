#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define THREAD_COUNT 10
#define MAX_DATA_SIZE 10+256*1024

struct arguments {
    int fd;
    char* str;
    size_t size;
    size_t count;
};

/*
* Write once and read "n" times, with "n" being the first argument passed to main
**/
void* mt_safety_test_3(void* arguments) {
    struct arguments* args = arguments;
    ssize_t r;

    for (int i=0; i<args->count; i++) {
        r = tfs_write(args->fd, args->str, args->size);
        assert(r == args->size);
    }

    return NULL;
}

int main() {
    struct arguments args[THREAD_COUNT];
    pthread_t tid[THREAD_COUNT];
    int fd;
    ssize_t r = 0;
    char* path = "/teste_3";
    char* chars[THREAD_COUNT] = {"aa", "bb", "cc", "dd", "ee", "ff", "gg", "hh", "ii", "jj"}; // Assuming there is a pair of letters per thread
    size_t count = MAX_DATA_SIZE / ((strlen(chars[0])+1) * THREAD_COUNT);
    size_t size = (size_t)strlen(chars[0])+1;

    assert(tfs_init() != -1);
    fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    for (int i=0; i<THREAD_COUNT; i++) { // Initialize arguments
        args[i].fd = fd;
        args[i].str = chars[i];
        args[i].size = size;
        args[i].count = count;
    }

    // Open THREAD_COUNT amount of threads each performing mt_safety_test_3
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_create(&tid[i], NULL, mt_safety_test_3, &args[i]) != 0 ) {
            printf("Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_join(tid[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }
    assert(tfs_close(fd) != -1);

    fd = tfs_open(path, 0); //Open file
    assert(fd != -1);

    char buffer[size];
    for (size_t i=0; i<count*THREAD_COUNT; i++) { // Check for each write that was made if it was made correctly
        char* temp_buffer = buffer;
        int check = -1;
        int c = 0; 
        r = tfs_read(fd, temp_buffer, size);
        assert(r == size);
        do {
            check = strcmp(temp_buffer, chars[c++]);
        } while (check != 0 && c < THREAD_COUNT);
        assert(check == 0);
    }

    assert(tfs_close(fd) != -1);
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}
