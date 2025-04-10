#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "shared.h"
#include "session.h"

FILE *global_log_file = NULL; // definido en session.c

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ip> <puerto> </ruta/log.log>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *log_path = argv[3];

    global_log_file = fopen(log_path, "a");
    if (!global_log_file) {
        perror("No se pudo abrir el archivo de log");
        return EXIT_FAILURE;
    }

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error en setsockopt");
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        fprintf(stderr, "Dirección IP inválida: %s\n", ip);
        return EXIT_FAILURE;
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Error en listen");
        return EXIT_FAILURE;
    }

    printf("Servidor iniciado en %s:%d\n", ip, port);
    fprintf(global_log_file, "[INFO] Servidor iniciado en %s:%d\n", ip, port);
    fflush(global_log_file);

    SessionManager *manager = session_manager_create(MAX_SESSIONS);
    if (!manager) {
        fprintf(stderr, "Error al crear el SessionManager\n");
        return EXIT_FAILURE;
    }

    printf("Esperando jugadores...\n");

    while (1) {
        int session_id = accept_players(manager, server_fd);
        if (session_id >= 0) {
            printf("Sesión %d creada con 2 jugadores\n", session_id);
            fprintf(global_log_file, "[INFO] Sesión %d iniciada\n", session_id);
            fflush(global_log_file);
        } else {
            sleep(1);
        }
    }

    close(server_fd);
    fclose(global_log_file);
    return 0;
}
