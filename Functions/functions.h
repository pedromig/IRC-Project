#ifndef IRC_PROJECT_FUNCTIONS_H
#define IRC_PROJECT_FUNCTIONS_H

#include <sodium.h>

#define KiB(x) ((size_t) (x) << 10) // 1024 bytes
#define BUFFER 512
#define MEDIUM_BUFFER KiB(6)
#define LARGE_BUFFER KiB(12)
#define LISTEN_LIMIT 5
#define STATE_FREE 0
#define CHAR "$"
#define TIMEOUT 5

#define EBUFFER BUFFER + crypto_secretbox_MACBYTES

typedef struct ClientThread {
    int thread_index;
    int client_fd;
} client_thread_t;

/**
 * This function checks if the string inserted
 * represents a number.
 * @param string The string receive in the command line
 * @return 0 if the string represents a number
 *         1 if the string does not represent a number
 */
int isnumber(char *string);

char **parseCommand(char *string);

void free_double_ptr(char **d_ptr, int size);

#endif //IRC_PROJECT_FUNCTIONS_H
