#include "client/tecnicofs_client_api.h"
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  This test just boots up different clients which 
    communicate with the server and compare the results.
*/

#define MAX_CLIENTS 4 // Just making 10 clients to have single char distinction

int fair(char* client_path, char* server_path) {
    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    int f;
    ssize_t r;
    printf("Opening client on %s\n", client_path);

    assert(tfs_mount(client_path, server_path) == 0);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("You must provide the following arguments: 'base_client_pipe_path "
               " server_pipe_path'\n");
        printf("The clients will use the same base path but with an incremental last value"
               " to distinguish them(e.g. /tmp/client1 and /tmp/client2\n");
        return 1;
    }
    char* base_path = malloc(sizeof(char)*(strlen(argv[1])+1));
    strcpy(base_path, argv[1]);

    char* clients[MAX_CLIENTS];
    int open_clients = -1;

    int pid = 1; // Random value different from 0
    while (pid != 0 && open_clients < MAX_CLIENTS) {
        pid = fork();
        assert(pid != -1);
        open_clients++;
    }
    if (pid == 0) {
        clients[open_clients] = malloc(sizeof(char)*(strlen(base_path)+1));
        strcpy(clients[open_clients], base_path);
        char* last_char = clients[open_clients] + strlen(base_path);
        *last_char = (char)((int)'0' + open_clients);
        assert(fair(clients[open_clients], argv[2]) == 0);
        free(clients[open_clients]);

        printf("Client %d finished sucessfully.\n", open_clients);
    } else {
        printf("Finished booting up the clients.\n");
    }

    return 0;
}
