#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/*  Simple test to check whether the implementation of
    tfs_destroy_after_all_closed is correct.
    Note: This test uses TecnicoFS as a library, not
    as a standalone server.
    We recommend trying more elaborate tests of tfs_destroy_after_all_closed.
    Also, we suggest trying out a similar test once the
    client-server version is ready (calling the tfs_shutdown_after_all_closed 
    operation).
*/

char* path = "/a";
char* msg = "A";
int counter = 0;

void *fn_thread(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */

    ssize_t r;
    int f;

    while (1) {
        f = tfs_open(path, TFS_O_CREAT);
        if (f == -1) {
            break;
        } // No more files can be opened, system is being destroyed

        r = tfs_write(f, msg, 1);
        assert(r == (ssize_t)1);
        counter++;

        assert(tfs_close(f) != -1);
    }

    return NULL;
}

int main(int argc, char** argv) {
    int count = 0;
    assert(tfs_init() != -1);

    if (argc > 1)
        count = atoi(argv[1]);

    if (count <= 0)
        count = 1;

    pthread_t tid[count];

    for (int i=0; i<count; i++) {
        assert(pthread_create(&tid[i], NULL, fn_thread, NULL) == 0);
    }
    assert(tfs_destroy_after_all_closed() != -1);

    // No need to join threads, they've all finished by now
    printf("Wrote %d times.\n", counter);
    printf("Successful test.\n");

    return 0;
}
