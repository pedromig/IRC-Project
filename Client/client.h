#ifndef IRC_PROJECT_CLIENT_H
#define IRC_PROJECT_CLIENT_H

#include <arpa/inet.h>

#define DOWNLOADS "./Client/Downloads"
typedef struct ClientSettings {
    int port;
    int protocol;
    char ip_address_server[INET_ADDRSTRLEN];
    char ip_address_proxy[INET_ADDRSTRLEN];
    unsigned long network_address_server;
    unsigned long network_address_proxy;
} client_settings_t;

typedef struct Statistics {
    char name[256];
    char protocol[10];
    double time_sec;
    double time_micro;
    off_t nbytes;
} stats_t;

/**
 * This function loads the client settings struct with the
 * ones that were passed by parameter in the command line
 * @param argv The arguments passed to the program in the command line
 * @return settings structure loaded with the info
 *         that was passed to the client via command
 *         line
 */
client_settings_t get_settings(char *argv[]);

void client(struct sockaddr_in proxy_address, struct sockaddr_in server_address);

void get_list(int server_fd);

void sig_handler(int signo);

stats_t get_file(char *file_path, char *file_name, int server_fd);

void print_stats(stats_t statistics);

#endif //IRC_PROJECT_CLIENT_H
