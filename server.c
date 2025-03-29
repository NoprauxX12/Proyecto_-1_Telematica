#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <asm-generic/socket.h>

#define PORT 8080
#define MAX_PLAYERS 2
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 10

// ========================= Structures =========================
typedef struct {
    char name[50];
    int socket;
    bool ready;
} Player;

typedef struct {
    Player players[MAX_PLAYERS];
    bool active;
    pthread_mutex_t lock;
} GameSession;

typedef struct {
    GameSession *sessions;
    int max_sessions;
    pthread_mutex_t session_lock;
} SessionManager;

// ========================= Functions =========================
void send_message(int socket, const char *type, const char *data) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s|%s", type, data);
    send(socket, message, strlen(message), 0);
}

SessionManager* session_manager_create(int max_sessions) {
    SessionManager *manager = (SessionManager *)malloc(sizeof(SessionManager));
    manager->max_sessions = max_sessions;
    manager->sessions = (GameSession *)malloc(sizeof(GameSession) * max_sessions);
    
    for (int i = 0; i < max_sessions; i++) {
        manager->sessions[i].active = false;
        pthread_mutex_init(&manager->sessions[i].lock, NULL);
    }
    
    pthread_mutex_init(&manager->session_lock, NULL);
    return manager;
}

int accept_players(SessionManager *manager, int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    // Buscar sesión disponible
    int session_id = -1;
    pthread_mutex_lock(&manager->session_lock);
    
    for (int i = 0; i < manager->max_sessions; i++) {
        if (!manager->sessions[i].active) {
            session_id = i;
            manager->sessions[i].active = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->session_lock);
    
    if (session_id == -1) {
        printf("No hay sesiones disponibles\n");
        return -1;
    }
    
    GameSession *session = &manager->sessions[session_id];
    
    // Aceptar jugadores para esta sesión
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        session->players[i].socket = new_socket;
        session->players[i].ready = false;
        
        char buffer[BUFFER_SIZE] = {0};
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) <= 0) {
            close(new_socket);
            i--;
            continue;
        }
        
        if (strncmp(buffer, "LOGIN|", 6) == 0) {
            strncpy(session->players[i].name, buffer + 6, sizeof(session->players[i].name) - 1);
            printf("Jugador %d conectado a sesión %d: %s\n", i+1, session_id, session->players[i].name);
            send_message(session->players[i].socket, "LOGIN", "OK");
        } else {
            send_message(session->players[i].socket, "ERROR", "INVALID_LOGIN");
            close(new_socket);
            i--;
            continue;
        }
    }
    
    // Notificar que ambos jugadores están conectados
    for (int i = 0; i < MAX_PLAYERS; i++) {
        send_message(session->players[i].socket, "WAITING", "OPPONENT_CONNECTED");
    }
    
    return session_id;
}

int setup_server() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    return server_fd;
}

// ========================= Main =========================
int main() {
    int server_fd = setup_server();
    SessionManager *manager = session_manager_create(MAX_SESSIONS);
    
    printf("Servidor iniciado en el puerto %d\n", PORT);
    printf("Esperando conexiones de jugadores...\n");
    
    while (1) {
        int session_id = accept_players(manager, server_fd);
        if (session_id >= 0) {
            printf("Sesión %d creada con 2 jugadores\n", session_id);
        } else {
            sleep(1); // Esperar si no hay sesiones disponibles
        }
    }
    
    close(server_fd);
    return 0;
}