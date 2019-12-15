#ifndef IRC_PROJECT_IRCPROXY_H
#define IRC_PROJECT_IRCPROXY_H

#define PROXY_IP_ADDRESS "127.0.0.4"
#define NUM_CLIENTS_MAX 100

#define PROXY "./Proxy/Saved_files"

#include <arpa/inet.h>
#include "../Functions/functions.h"

typedef struct ProxySettings {
    int port;
    char ip_address[INET_ADDRSTRLEN];
    unsigned long network_address;
} proxy_settings_t;

typedef struct ProxyClientThread {
    int thread_index;
    int client_fd;
} pclient_thread_t;

typedef struct ShowInfoStructure {
    struct sockaddr_in client_address;
    struct sockaddr_in server_address;
} show_t;

/**
 * This function loads the proxy settings struct with the
 * ones that were passed by parameter in the command line
 * @param argv The arguments passed to the program in the command line
 * @return settings structure loaded with the info
 *         that was passed to the proxy via command
 *         line
 */
proxy_settings_t get_settings(char *argv[]);

void cleanup(client_thread_t client);

void *new_client(void *arg);

void *proxy(void *arg);

void proxy_info(proxy_settings_t settings);

void sig_handler(int signo);

void transmit_dir(int server_fd, int client_fd);

int transmit_file(char *name, int server_fd, int client_fd);

void udp_transfer();

#endif //IRC_PROJECT_IRCPROXY_H
