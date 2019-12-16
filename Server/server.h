#ifndef IRC_PROJECT_SERVER_H
#define IRC_PROJECT_SERVER_H

#include <arpa/inet.h>

#define SERVER_IP_ADDRESS "127.0.0.2"
#define SERVER_FILES "./Server/Server_files"

typedef struct ServerSettings {
    int port;
    int num_clients;
    char ip_address[INET_ADDRSTRLEN];
    unsigned long network_address;
} server_settings_t;

/**
 * This function loads the server settings struct with the
 * ones that were passed by parameter in the command line
 * @param argv The arguments passed to the program in the command line
 * @return settings structure loaded with the info
 *         that was passed to the server via command
 *         line
 */
server_settings_t get_settings(char *argv[]);

void server_info();

void *new_client(void *arg);

void sig_handler(int signo);

int list_dir(int client_fd, char *directory);

int isInDirectory(char *name, char *directory);

int send_file_tcp(int dst_fd, char *path);

int send_file_udp(int fd,struct sockaddr_in client,char *path);

char *encrypt(char *path, unsigned char *nonce);

void udp_transfer(char *path);

#endif //IRC_PROJECT_SERVER_H
