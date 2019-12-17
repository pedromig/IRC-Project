#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <ctype.h>
#include <pthread.h>
#include <sodium.h>
//My header files
#include "ircproxy.h"
#include "../Functions/functions.h"

int proxy_fd_tcp, active_clients;
int *descriptors;
int losses;
int save;
pthread_t *clients, proxy_thread;
pthread_mutex_t client_temination_mutex = PTHREAD_MUTEX_INITIALIZER;
show_t *info;
struct sockaddr_in server_address;
struct sockaddr_in proxy_address;

int main(int argc, char *argv[]) {
    int done = 0, i;
    int temp;
    char *token, delimiter[2] = " ";
    char command[BUFFER];
    char buffer[BUFFER];
    struct sockaddr_in client_address;
    proxy_settings_t settings;

    losses = 0;
    save = 0;

    signal(SIGINT, SIG_IGN);
    if (argc != 2) {
        printf("Usage: ./ircproxy <PORT>\n");
        exit(0);
    }
    settings = get_settings(argv);

    clients = (pthread_t *) calloc(NUM_CLIENTS_MAX, sizeof(pthread_t));
    descriptors = (int *) calloc(NUM_CLIENTS_MAX, sizeof(int));
    info = (show_t *) calloc(NUM_CLIENTS_MAX, sizeof(show_t));

    if ((proxy_fd_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error creating the proxy tcp socket\n");
        exit(0);
    }

    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = settings.network_address;
    proxy_address.sin_port = htons(settings.port);


    if (bind(proxy_fd_tcp, (struct sockaddr *) &proxy_address, (socklen_t) sizeof(proxy_address)) == -1) {
        printf("Error binding the proxy socket\n");
        exit(0);
    }

    if (listen(proxy_fd_tcp, LISTEN_LIMIT) == -1) {
        printf("Error preparing the socket to listen to new connections\n");
        exit(0);
    }


    signal(SIGINT, sig_handler);
    if (pthread_create(&proxy_thread, NULL, proxy, &client_address)) {
        printf("Proxy main thread creation failed\n");
        exit(0);
    }

    proxy_info(settings);

    while (!done) {

        printf(">>> ");
        fgets(command, BUFFER, stdin);
        command[strcspn(command, "\n")] = 0;
        fflush(stdin);

        if (!strncmp(command, "SAVE", 4)) {
            strtok(command, delimiter);
            if ((token = strtok(NULL, delimiter))) {
                if (!strcmp(token, "ON")) {
                    printf("FILE SAVE ACTIVATED!!\n");
                    save = 1;
                }

                if (!strcmp(token, "OFF")) {
                    printf("FILE SAVE DEACTIVATED!!\n");
                    save = 0;
                }
            } else {
                printf("Invalid Command!!\n");
            }
        } else if (!strncmp(command, "LOSSES", 6)) {

            strtok(command, delimiter);
            if ((token = strtok(NULL, delimiter))) {

                if (token && isnumber(token)) {
                    temp = atoi(token);
                    if (temp <= 100 && temp >= 0) {
                        losses = temp;
                        printf("DONE! (losses = %d %%) \n", losses);
                    } else {
                        printf("Invalid Command!!\n");
                    }
                }

                if (!strcmp(token, "RESET")) {
                    losses = 0;
                    printf("DONE! (losses = %d %%)\n", losses);
                }

            } else {
                printf("Invalid Command!!\n");
            }

        } else if (!strcmp(command, "SHOW")) {
            char ip[INET_ADDRSTRLEN];

            printf("<----------------- INFO ----------------->\n");
            for (i = 0; i < NUM_CLIENTS_MAX; i++) {
                if (clients[i] != STATE_FREE) {
                    printf("\nCLIENT [%d]\n", i + 1);
                    inet_ntop(AF_INET, &info[i].client_address, ip, INET_ADDRSTRLEN);
                    printf("Client Address: %s\n", ip);
                    printf("Client Port: %ld\n", info[i].client_port);

                    inet_ntop(AF_INET, &info[i].server_address, ip, INET_ADDRSTRLEN);
                    printf("Server Address: %s\n", ip);
                    printf("Server Port: %ld\n", info[i].server_port);
                }
            }
            printf("\n<---------------------------------------->\n");
        } else if (!strcmp(command, "QUIT")) {
            sig_handler(SIGINT);
            done = 1;
        } else {
            printf("Invalid Command!!\n");
        }

    }

    return 0;
}

void proxy_info(proxy_settings_t settings) {
    printf("<---------- PROXY STARTED ---------->\n");
    printf("\t-> IP ADDRESS: %s\n", settings.ip_address);
    printf("\t-> PORT: %d\n", settings.port);
    printf("<------------------------------------>\n\n");
}

proxy_settings_t get_settings(char *argv[]) {
    proxy_settings_t settings;

    if (!isnumber(argv[1])) {
        printf("Error getting port\n");
        exit(0);
    }
    settings.port = atoi(argv[1]);

    if (inet_pton(AF_INET, PROXY_IP_ADDRESS, &settings.network_address) == 0) {
        printf("Invalid ip address for the server");
        exit(0);
    }
    strcpy(settings.ip_address, PROXY_IP_ADDRESS);

    return settings;
}


void *proxy(void *arg) {
    struct sockaddr_in client_address = *((struct sockaddr_in *) arg);
    int client_fd, i, found = 0;
    socklen_t client_address_size;
    client_thread_t *client_thread;

    client_address_size = (socklen_t) sizeof(client_address);

    while (1) {

        client_fd = accept(proxy_fd_tcp, (struct sockaddr *) &client_address, (socklen_t *) &client_address_size);

        if (active_clients < NUM_CLIENTS_MAX && !(client_fd > 0 ? 0 : 1)) {
            active_clients++;

            client_thread = (client_thread_t *) malloc(sizeof(client_thread_t));

            client_thread->client_fd = client_fd;

            for (i = 0, found = 0; i < NUM_CLIENTS_MAX && !found; i++) {
                if (clients[i] == STATE_FREE) {
                    client_thread->thread_index = i;
                    info[i].client_address = client_address.sin_addr;
                    info[i].client_port = client_address.sin_port;
                    if (pthread_create(&clients[i], NULL, new_client, client_thread)) {
                        printf("Client Thread creation failed\n");
                        exit(0);
                    }
                    found = 1;
                }
            }
        } else {
            printf("Number of clients exceeded-- > Server Rejected\n");
            close(client_fd);
        }
    }

    return NULL;
}

void *new_client(void *arg) {
    int nread = 0, done = 0;
    int server_fd;
    char buffer[BUFFER];
    char **params;
    client_thread_t client = *((client_thread_t *) arg);
    socklen_t server_address_size;

    char nonce[crypto_secretbox_NONCEBYTES];

    //Config Connection
    nread = read(client.client_fd, buffer, BUFFER);
    buffer[nread] = '\0';

    if (!strcmp(buffer, "TCP")) {

        if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error creating the tcp client socket\n");
            cleanup(client);
        }

        server_address_size = read(client.client_fd, &server_address, sizeof(server_address));
        if (connect(server_fd, (struct sockaddr *) &server_address, server_address_size) < 0) {
            printf("Error connecting to the server\n");
            close(server_fd);
            cleanup(client);
        }
        info[client.thread_index].server_address = server_address.sin_addr;
        info[client.thread_index].server_port = server_address.sin_port;

        write(server_fd, "TCP", BUFFER);
        nread = read(server_fd, buffer, BUFFER);
        buffer[nread] = '\0';
        write(client.client_fd, buffer, BUFFER);

        if (!strcmp(buffer, "REFUSED")) {
            write(client.client_fd, "REFUSED", BUFFER);
            close(server_fd);
            cleanup(client);
            done = 1;
            printf("Server Rejected Connection\n");
        }

        while (!done) {

            nread = read(client.client_fd, buffer, BUFFER);
            buffer[nread] = '\0';

            if (!strcmp(buffer, "LIST")) {
                write(server_fd, "LIST", BUFFER);
                transmit_dir(server_fd, client.client_fd);
            } else if (!strncmp(buffer, "DOWNLOAD", 8)) {

                write(server_fd, buffer, BUFFER);
                params = parseCommand(buffer);

                if (!strcmp(params[0], "TCP")) {
                    read(server_fd, buffer, BUFFER);
                    buffer[nread] = '\0';

                    write(client.client_fd, buffer, BUFFER);

                    if (!strcmp(buffer, "FOUND")) {
                        if (!strcmp(params[1], "NOR")) {
                            transmit_file(params[2], server_fd, client.client_fd);
                        } else if (!strcmp(params[1], "ENC")) {
                            read(server_fd, nonce, sizeof(nonce));
                            write(client.client_fd, nonce, sizeof(nonce));
                            transmit_file(params[2], server_fd, client.client_fd);
                        }
                    }
                }

                if (!strcmp(params[0], "UDP")) {
                    read(server_fd, buffer, BUFFER);
                    buffer[nread] = '\0';

                    write(client.client_fd, buffer, BUFFER);
                    if (!strcmp(buffer, "FOUND")) {
                        if (!strcmp(params[1], "NOR")) {
                            udp_transmition(params[2]);
                        } else if (!strcmp(params[1], "ENC")) {
                            memset(nonce,0,crypto_secretbox_NONCEBYTES);
                            read(server_fd, nonce,crypto_secretbox_NONCEBYTES);
                            write(client.client_fd, nonce, crypto_secretbox_NONCEBYTES);
                            udp_transmition(params[2]);
                        }
                    }
                }

                free_double_ptr(params, 3);
            } else if (!strcmp(buffer, "QUIT")) {
                write(server_fd, "QUIT", BUFFER);
                done = 0;
                close(server_fd);

                pthread_mutex_lock(&client_temination_mutex);
                active_clients--;
                pthread_mutex_unlock(&client_temination_mutex);

                cleanup(client);
            }
        }
    } else if (!strcmp(buffer, "UDP")) {
        printf("NOT IMPLEMENTED: \n");
        printf("Nota: como protocolo a indicar ao cliente poderá "
               "considerar apenas o TCP (a utilizar na ligação inicial\n"
               "entre o cliente e o proxy, bem como entre o proxy e o servidor).");
    } else {
        printf("Wrong Protocol\n");
    }


    return NULL;
}

void cleanup(client_thread_t client) {
    clients[client.thread_index] = STATE_FREE;
    pthread_detach(pthread_self());

    pthread_mutex_lock(&client_temination_mutex);
    active_clients--;
    pthread_mutex_unlock(&client_temination_mutex);
    close(client.client_fd);
    pthread_exit(NULL);
}

int transmit_file(char *name, int server_fd, int client_fd) {
    int received = 0, nread = 0, sent = 0;
    char buffer[BUFFER];
    char path[BUFFER];
    off_t size;
    FILE *save_fp = NULL;

    read(server_fd, &size, sizeof(size));
    write(client_fd, &size, sizeof(size));

    if (save) {
        sprintf(path, "%s/%s", PROXY, name);
        save_fp = fopen(path, "wb");
        if (!save_fp) {
            printf("Error occurred while opening the file!\n");
            received = size;
        }
    }

    while (received < size) {
        nread = read(server_fd, buffer, BUFFER);
        sent += write(client_fd, buffer, nread);
        if (save && save_fp)
            fwrite(buffer, 1, nread, save_fp);
        received += nread;
    }

    if (save_fp)
        fclose(save_fp);
    return sent;
}

int transmit_dir(int server_fd, int client_fd) {
    int nread, done = 0;
    char buffer[BUFFER];

    nread = read(server_fd, buffer, BUFFER);
    write(client_fd, CHAR, BUFFER);

    while (!done) {
        read(server_fd, buffer, BUFFER);
        if (!strcmp(buffer, CHAR)) {
            done = 1;
            write(client_fd, CHAR, BUFFER);
        } else {
            write(client_fd, buffer, BUFFER);
        }
    }
    return nread;
}


int udp_transmition(char *name) {
    int received = 0, nread = 0, sent = 0;
    char path[BUFFER];
    int server_fd_udp, proxy_fd_udp;
    struct sockaddr_in client;
    struct sockaddr_in server;
    char buffer[BUFFER];
    socklen_t clen = sizeof(client);
    socklen_t slen = sizeof(server);
    off_t size;
    FILE *save_fp = NULL;

    if ((proxy_fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Error creating the proxy udp socket\n");
        exit(0);
    }

    if (bind(proxy_fd_udp, (struct sockaddr *) &proxy_address, sizeof(proxy_address)) == -1) {
        printf("Error binding the server udp socket\n");
        exit(0);
    }


    if ((server_fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Error creating the proxy udp socket\n");
        exit(0);
    }

    recvfrom(proxy_fd_udp, buffer, BUFFER, 0, (struct sockaddr *) &client, (socklen_t *) &clen);
    sendto(server_fd_udp, buffer, BUFFER, 0, (struct sockaddr *) &server_address, (socklen_t) slen);

    recvfrom(server_fd_udp, &size, sizeof(size), 0, (struct sockaddr *) &server, (socklen_t *) &clen);
    sendto(proxy_fd_udp, &size, sizeof(size), 0, (struct sockaddr *) &client, (socklen_t) slen);

    if (save) {
        sprintf(path, "%s/%s", PROXY, name);
        save_fp = fopen(path, "wb");
        if (!save_fp) {
            printf("Error occurred while opening the file!\n");
            received = size;
        }
    }


    while (received < size) {
        nread = recvfrom(server_fd_udp, buffer, BUFFER, 0, (struct sockaddr *) &server, (socklen_t *) &clen);
        //LOSSES
        if (rand() % 100 > losses) {
            sent += sendto(proxy_fd_udp, buffer, nread, 0, (struct sockaddr *) &client, (socklen_t) slen);
        }
        if (save && save_fp)
            fwrite(buffer, 1, nread, save_fp);
        received += nread;
    }
    sendto(proxy_fd_udp, "EOF", BUFFER, 0, (struct sockaddr *) &client, (socklen_t) slen);

    if (save_fp)
        fclose(save_fp);

    close(server_fd_udp);
    close(proxy_fd_udp);
    return sent;

}

void sig_handler(int signo) {
    signal(SIGINT, SIG_IGN);
    printf("\nProxy Shutdown...\n");

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
    free(info);
    close(proxy_fd_tcp);
    exit(0);
}
