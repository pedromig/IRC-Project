#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sodium.h>
#include "client.h"
#include "../Functions/functions.h"

int fd_udp, fd_tcp;
client_settings_t settings;

unsigned char key[crypto_secretbox_KEYBYTES] = "uB,$-k6??J_g/)^CwrBbD+kR_BH,z[Dk";

int main(int argc, char *argv[]) {

    struct sockaddr_in server_address, proxy_address;
    signal(SIGINT, SIG_IGN);

    if (argc != 5) {
        printf("Usage: ./client <PROXY ADDRESS> <SERVER ADDRESS> <PORT> <PROTOCOL>\n");
        exit(0);
    }
    settings = get_settings(argv);

    if ((fd_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error creating the tcp client socket\n");
        exit(0);
    }

    if ((fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Error creating the udp client socket\n");
        exit(0);
    }

    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = settings.network_address_proxy;
    proxy_address.sin_port = settings.port;

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = settings.network_address_server;
    server_address.sin_port = settings.port;

    signal(SIGINT, sig_handler);
    // Executa o cliente no protocolo definido
    if (settings.protocol == IPPROTO_TCP) {
        client(proxy_address, server_address);
    } else {
        printf("NOT IMPLEMENTED: \n");
        printf("Nota: como protocolo a indicar ao cliente poderá "
               "considerar apenas o TCP (a utilizar na ligação inicial\n"
               "entre o cliente e o proxy, bem como entre o proxy e o servidor).");
    }

    return 0;
}

client_settings_t get_settings(char *argv[]) {

    if (inet_pton(AF_INET, argv[1], &settings.network_address_proxy) == 0) {
        printf("Invalid ip address for the server\n");
        exit(0);
    }
    strcpy(settings.ip_address_proxy, argv[1]);

    if (inet_pton(AF_INET, argv[2], &settings.network_address_server) == 0) {
        printf("Invalid ip address for the server");
        exit(0);
    }
    strcpy(settings.ip_address_server, argv[2]);

    if (!isnumber(argv[3])) {
        printf("Invalid port\n");
        exit(0);
    }
    settings.port = htons(atoi(argv[3]));

    if (!strcmp(argv[4], "TCP")) {
        settings.protocol = IPPROTO_TCP;
    } else if (!strcmp(argv[4], "UDP")) {
        settings.protocol = IPPROTO_UDP;
    } else {
        printf("Invalid protocol!\n");
        exit(0);
    }
    return settings;
}

void client(struct sockaddr_in proxy_address, struct sockaddr_in server_address) {
    int nread = 0, done = 0, encryption;
    char command[BUFFER];
    char buffer[BUFFER], copy[BUFFER];
    char file_path[BUFFER];
    char **params;
    socklen_t proxy_address_size = sizeof(proxy_address);
    stats_t stats;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];

    if (connect(fd_tcp, (struct sockaddr *) &proxy_address, proxy_address_size) < 0) {
        printf("Error connecting to the server\n");
        exit(0);
    }
    write(fd_tcp, "TCP", BUFFER);
    write(fd_tcp, &server_address, sizeof(server_address));

    nread = read(fd_tcp, buffer, BUFFER);
    buffer[nread] = '\0';

    if (!strcmp(buffer, "REFUSED")) {
        printf("Server Rejected Connection\n"
               "-> Client Number Exceeded!\n");
        done = 1;
        close(fd_tcp);
    }

    while (!done) {
        signal(SIGINT, sig_handler);

        printf(">>> ");
        fgets(command, BUFFER, stdin);
        command[strcspn(command, "\n")] = 0;
        strcpy(copy, command);
        fflush(stdin);

        signal(SIGINT, SIG_IGN);
        if (!strcmp(command, "LIST")) {
            write(fd_tcp, "LIST", BUFFER);
            get_list(fd_tcp);
        } else if ((params = parseCommand(command))) {
            write(fd_tcp, copy, BUFFER);

            nread = read(fd_tcp, buffer, BUFFER);
            buffer[nread] = '\0';

            if (!strcmp(buffer, "FOUND")) {
                sprintf(file_path, "%s/%s", DOWNLOADS, params[2]);
                if (!strcmp(params[0], "TCP")) {

                    if (!strcmp(params[1], "NOR")) {
                        stats = get_file(file_path, params[2], fd_tcp);
                        print_stats(stats);
                    } else if (!strcmp(params[1], "ENC")) {
                        read(fd_tcp, nonce, sizeof(nonce));
                        stats = get_file(file_path, params[2], fd_tcp);
                        decrypt(file_path, nonce);
                        print_stats(stats);
                    }

                } else if (!strcmp(params[0], "UDP")) {
                    stats = get_file(file_path, params[2], fd_udp);
                    print_stats(stats);
                } else {
                    printf("Invalid Protocol!!\n");
                }
                free_double_ptr(params, 3);
            } else if (!strcmp(buffer, "NOT FOUND")) {
                printf("File not found !\n");
            }

        } else if (!strcmp(command, "QUIT")) {
            write(fd_tcp, "QUIT", BUFFER);
            done = 1;
            close(fd_tcp);
        } else {
            printf("Invalid Command!!\n");
        }
    }

}

void decrypt(char *path, unsigned char *nonce) {
    int nread, done = 0;
    unsigned char decrypt[EBUFFER], ciphertext[EBUFFER];
    FILE *encrypted_fp;
    FILE *original_fp;
    char temp[256];

    sprintf(temp, "%s/%ld", DOWNLOADS, time(NULL));
    encrypted_fp = fopen(path, "r");
    original_fp = fopen(temp, "w");

    if (!encrypted_fp || !original_fp) {
        printf("Failed to open files!\n");
        exit(0);
    }

    while ((nread = fread(ciphertext, 1, EBUFFER, encrypted_fp)) && !done) {
        if (!crypto_secretbox_open_easy(decrypt, ciphertext, nread, nonce, key)) {
            fwrite(decrypt, 1, nread - crypto_secretbox_MACBYTES, original_fp);
        } else {
            printf("Message forged!\nStoping decryption...");
            done = 1;
        }
    }
    fclose(encrypted_fp);
    remove(path);
    rename(temp, path);
    fclose(original_fp);
}

void get_list(int server_fd) {
    int nread = 0;
    int done = 0;
    char read_buff[KiB(2)];

    nread = read(server_fd, read_buff, KiB(2));
    read_buff[nread] = '\0';

    while (!done) {
        nread = read(server_fd, read_buff, KiB(2));
        read_buff[nread] = '\0';
        if (!strcmp(read_buff, CHAR)) {
            done = 1;
        } else {
            printf("%s", read_buff);
        }
    }
}


stats_t get_file(char *file_path, char *file_name, int server_fd) {
    int received = 0, nread = 0;
    char buffer[BUFFER];
    FILE *dst_fp = fopen(file_path, "wb");
    struct timeval start, end;
    stats_t stats;
    off_t size;

    printf("Waiting for file\n");
    gettimeofday(&start, NULL);

    read(server_fd, &size, sizeof(size));

    while (received < size) {
        nread = read(server_fd, buffer, BUFFER);
        fwrite(buffer, 1, nread, dst_fp);
        received += nread;
    }

    gettimeofday(&end, NULL);
    fclose(dst_fp);

    strcpy(stats.name, file_name);
    if (settings.protocol == IPPROTO_TCP) {
        strcpy(stats.protocol, "TCP");
    } else {
        strcpy(stats.protocol, "UDP");
    }

    stats.nbytes = received;
    stats.time_sec = end.tv_sec - start.tv_sec;
    stats.time_micro = end.tv_usec - start.tv_usec;

    return stats;
}

void print_stats(stats_t statistics) {
    printf("<-------- DOWNLOAD COMPLETE!!---------->\n");
    printf("-> FILE NAME: %s\n", statistics.name);
    printf("-> BYTES RECEIVED: %ld\n", statistics.nbytes);
    printf("-> PROTOCOL: %s\n", statistics.protocol);
    printf("-> ELAPSED : %1.3lf (seconds)\n", statistics.time_sec);
    printf("    TIME   : %1.3lf (microseconds)\n", statistics.time_micro);
    printf("<-------------------------------------->\n");
}

void sig_handler(int signo) {
    signal(SIGINT, SIG_IGN);
    printf("\nI Was killed by SIGINT\n");
    write(fd_tcp, "QUIT", BUFFER);
    close(fd_tcp);
    close(fd_udp);
    exit(0);
}