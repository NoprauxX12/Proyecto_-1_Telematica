
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "messaging.h"

#define BUFFER_SIZE 1024

void send_message(int socket, const char *type, const char *data) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s|%s\n", type, data);
    send(socket, message, strlen(message), 0);
}
