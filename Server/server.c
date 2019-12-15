#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <semaphore.h>
#include <sys/stat.h>
// My header files
#include "server.h"
#include "../Functions/functions.h"

int server_fd_tcp, server_fd_udp, active_clients;
int *descriptors;
server_settings_t settings;
pthread_t *clients;
pthread_mutex_t client_temination_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned char key[crypto_secretbox_KEYBYTES] = "uB,$-k6??J_g/)^CwrBbD+kR_BH,z[Dk";

int main(int argc, char *argv[]) {
    int nread = 0, client_fd, found, ignore, i;
    char buffer[BUFFER];
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size;

    client_thread_t *client_thread;


    if (argc != 3) {
        printf("Usage: ./server <PORT> <NUMBER OF CLIENTS>\n");
        exit(0);
    }
    settings = get_settings(argv);
    server_info(settings);

    clients = (pthread_t *) calloc(settings.num_clients, sizeof(pthread_t));
    descriptors = (int *) calloc(settings.num_clients, sizeof(int));

    if ((server_fd_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error creating the server tcp socket\n");
        exit(0);
    }

    if ((server_fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Error creating the udp socket\n");
        exit(0);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = settings.network_address;
    server_address.sin_port = htons(settings.port);

    if (bind(server_fd_tcp, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) == -1) {
        printf("Error binding the server socket\n");
        exit(0);
    }

    if (listen(server_fd_tcp, LISTEN_LIMIT) == -1) {
        printf("Error preparing the socket to listen to new connections\n");
        exit(0);
    }

    if (bind(server_fd_udp, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        printf("Error binding the udp socket\n");
        exit(0);
    }

    signal(SIGINT, sig_handler);
    active_clients = 0;
    while (1) {
        client_fd = accept(server_fd_tcp, (struct sockaddr *) &client_address, (socklen_t *) &client_address_size);

        if (active_clients < settings.num_clients && !(client_fd > 0 ? 0 : 1)) {
            active_clients++;

            //Config connection
            nread = read(client_fd, buffer, BUFFER);
            buffer[nread] = '\0';

            client_thread = (client_thread_t *) malloc(sizeof(client_thread_t));

            client_thread->client_fd = client_fd;

            write(client_fd, "Connection Successfull!", BUFFER);

            for (i = 0, found = 0; i < settings.num_clients && !found; i++) {
                if (clients[i] == STATE_FREE) {
                    client_thread->thread_index = i;
                    descriptors[i] = client_fd;
                    if (pthread_create(&clients[i], NULL, new_client, client_thread)) {
                        printf("Client Thread creation failed\n");
                        exit(0);
                    } else {
                        printf("Client thread creation successful\n");
                    }
                    found = 1;
                }
            }
        } else {
            nread = read(client_fd, buffer, BUFFER);
            buffer[nread] = '\0';

            printf("Number of clients exceeded\n");
            write(client_fd, "REFUSED", BUFFER);
            close(client_fd);
        }
        printf("Active Clients: %d \n", active_clients);

    }

    return 0;
}

server_settings_t get_settings(char *argv[]) {

    if (!isnumber(argv[1])) {
        printf("Error getting port\n");
        exit(0);
    }
    settings.port = atoi(argv[1]);

    if (!isnumber(argv[2])) {
        printf("Error getting number of clients\n");
        exit(0);
    }
    settings.num_clients = atoi(argv[2]);

    if (inet_pton(AF_INET, SERVER_IP_ADDRESS, &settings.network_address) == 0) {
        printf("Invalid ip address for the server");
        exit(0);
    }
    strcpy(settings.ip_address, SERVER_IP_ADDRESS);

    return settings;
}

void server_info() {
    printf("<---------- SERVER STARTED ---------->\n");
    printf("\t-> IP ADDRESS: %s\n", settings.ip_address);
    printf("\t-> PORT: %d\n", settings.port);
    printf("\t-> MAX CLIENTS: %d\n", settings.num_clients);
    printf("<------------------------------------>\n\n");
}

void *new_client(void *arg) {
    int nread = 0, done = 0;
    char file_path[BUFFER * 2];
    char buffer[BUFFER];
    char **params;
    char *encrypted;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    client_thread_t client = *((client_thread_t *) arg);

    free(arg);
    printf("Client Active!!\n");

    while (!done) {

        nread = read(client.client_fd, buffer, BUFFER);
        buffer[nread] = '\0';

        printf("Command: %s\n", buffer);

        if (!strcmp(buffer, "LIST")) {
            list_dir(client.client_fd, SERVER_FILES);
        } else if (!strncmp(buffer, "DOWNLOAD", 8)) {
            printf("> %s\n", buffer);
            if ((params = parseCommand(buffer))) {

                if (isInDirectory(params[2], SERVER_FILES)) {
                    write(client.client_fd, "FOUND", BUFFER);

                    sprintf(file_path, "%s/%s", SERVER_FILES, params[2]);
                    printf("DOWNLOAD FILE: %s\n", file_path);

                    if (!strcmp(params[0], "TCP")) {

                        if (!strcmp(params[1], "NOR")) {
                            printf("SENDING FILE...\n");
                            send_file(client.client_fd, file_path);
                            printf("DONE!\n");
                        } else if (!strcmp(params[1], "ENC")) {

                            randombytes_buf(nonce, sizeof(nonce));
                            encrypted = encrypt(file_path, nonce);
                            write(client.client_fd, nonce, sizeof(nonce));

                            printf("SENDING ENCRYPTED FILE...\n");
                            send_file(client.client_fd, encrypted);
                            printf("DONE!\n");
                            remove(encrypted);
                            free(encrypted);
                        }

                    } else if (!strcmp(params[0], "UDP")) {

                    } else {
                        printf("Error occurred!!\n");
                    }
                    free_double_ptr(params, 3);
                } else {
                    write(client.client_fd, "NOT FOUND", BUFFER);
                }
            } else {
                printf("Wrong command!\n");
            }
        } else if (!strcmp(buffer, "QUIT")) {
            done = 1;
            clients[client.thread_index] = STATE_FREE;
            pthread_detach(pthread_self());

            pthread_mutex_lock(&client_temination_mutex);
            active_clients--;
            pthread_mutex_unlock(&client_temination_mutex);

            printf("Client closed connection\n");
            printf("Active Clients: %d \n", active_clients);
        } else {
            // Kill server if something unexpected occurs
            sig_handler(SIGINT);
        }
    }

    close(client.client_fd);
    pthread_exit(NULL);
}

char *encrypt(char *path, unsigned char *nonce) {
    int nread;
    unsigned char buffer[BUFFER], ciphertext[EBUFFER];
    char encrypted_path[256];
    FILE *original_fp;
    FILE *encrypted_fp;


    sprintf(encrypted_path, "%s/%ld", SERVER_FILES, time(NULL));

    original_fp = fopen(path, "rb");
    encrypted_fp = fopen(encrypted_path, "wb");

    if (!original_fp || !encrypted_fp) {
        printf("Failed to open files!\n");
        exit(0);
    }

    while ((nread = fread(buffer, 1, BUFFER, original_fp))) {
        crypto_secretbox_easy(ciphertext, buffer, nread, nonce, key);
        fwrite(ciphertext, 1, nread + crypto_secretbox_MACBYTES, encrypted_fp);
    }
    fclose(original_fp);
    fclose(encrypted_fp);

    return strdup(encrypted_path);
}

void *new_udp_client(void *arg) {
    client_thread_t client = *((client_thread_t *) arg);
    pthread_detach(pthread_self());
    printf("Client Active[%d]\n", client.client_fd);

    return NULL;
}

void list_dir(int client_fd, char *directory) {
    int nwrite = 0;
    char buffer[BUFFER];
    struct dirent *file;
    DIR *dir;

    if ((dir = opendir(directory)) != NULL) {
        nwrite += write(client_fd, CHAR, BUFFER);

        strcpy(buffer, "<-------- SERVER FILES -------->\n");
        nwrite += write(client_fd, buffer, BUFFER);
        while ((file = readdir(dir)))
            if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
                sprintf(buffer, "\t-> %s\n", file->d_name);
                nwrite += write(client_fd, buffer, BUFFER);
            }
        strcpy(buffer, "<------------------------------>\n");
        nwrite += write(client_fd, buffer, BUFFER);

        nwrite += write(client_fd, CHAR, BUFFER);
        closedir(dir);
    } else if (errno == ENOENT) {
        printf("Directory does not exist!\n");
    } else if (errno == ENOTDIR) {
        printf("There is no directory with such name!\n");
    } else {
        printf("Error opening directory!\n");
    }
}

int isInDirectory(char *name, char *directory) {
    int found = 0;
    struct dirent *file;
    DIR *dir;

    if ((dir = opendir(directory)) != NULL) {
        while ((file = readdir(dir)) && !found)
            if (!strcmp(file->d_name, name)) {
                found = 1;
            }
    }
    return found;
}

int send_file(int dst_fd, char *path) {
    FILE *src;
    struct stat f_status;
    int nread, sent = 0, src_fd;
    char buffer[BUFFER];

    if (!(src = fopen(path, "rb"))) {
        printf("Error opening the file\n");
        exit(0);
    }

    src_fd = fileno(src);
    fstat(src_fd, &f_status);
    write(dst_fd, &f_status.st_size, sizeof(off_t));

    while ((nread = fread(buffer, 1, BUFFER, src)) != 0) {
        sent += write(dst_fd, buffer, nread);
    }

    printf("Sent: %d\n", sent);
    fclose(src);
    return sent;
}


void sig_handler(int signo) {
    signal(SIGINT, SIG_IGN);
    printf("\nServer Shutdown...\n");
    for (int i = 0; i < active_clients; i++) {
        if (clients[i] != STATE_FREE) {
            if (pthread_cancel(clients[i])) {
                printf("Failed to cancel client thread\n");
            }
            if (pthread_join(clients[i], NULL)) {
                printf("Failed to join client thread\n");
            }
            close(descriptors[i]);
            printf("Killed client\n");
        }
    }
    pthread_mutex_destroy(&client_temination_mutex);
    free(clients);
    free(descriptors);
    close(server_fd_tcp);
    close(server_fd_udp);
    exit(0);
}