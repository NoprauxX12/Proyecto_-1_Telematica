// ======================= Includes =======================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <asm-generic/socket.h>
#include <time.h>

// ======================= Constants =========================
#define PORT 8080
#define MAX_PLAYERS 2
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 10
#define BOARD_SIZE 10
#define MAX_SHIPS 5
#define WATER '~'
#define HIT 'X'
#define SUNK '#'
#define SHIP 'O'

// ========================= Structures =========================

typedef struct{
    char name[20];
    int size;
} ShipType;

ShipType SHIPS[MAX_SHIPS] = {
    {"Portaaviones", 5},
    {"Buque", 4},
    {"Crucero", 3},
    {"Destructor", 2},
    {"Submarino", 1}
};

typedef struct {
    char name[20];
    int size;
    int hits;
    int positions[5][2]; 
} Ship;

typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    Ship ships[MAX_SHIPS];
    int ship_count;
} Board;

typedef struct {
    char name[50];
    int socket;
    bool ready;
    Board board;
} Player;

typedef struct {
    Player players[MAX_PLAYERS];
    bool active;
    pthread_mutex_t lock;
    int current_turn;
    time_t turn_start_time;
    int time_limit;
    bool game_over;
} GameSession;

typedef struct {
    GameSession *sessions;
    int max_sessions;
    pthread_mutex_t session_lock;
} SessionManager;

// ========================= Function Prototypes =========================
void* handle_game_session(void *arg);
void send_message(int socket, const char *type, const char *data);
SessionManager* session_manager_create(int max_sessions);
int accept_players(SessionManager *manager, int server_fd);
int setup_server();
void init_board(Board *board);

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
    
    // 1. Buscar sesión disponible
    int session_id = -1;
    pthread_mutex_lock(&manager->session_lock);
    
    for (int i = 0; i < manager->max_sessions; i++) {
        if (!manager->sessions[i].active) {
            session_id = i;
            manager->sessions[i].active = true;
            pthread_mutex_init(&manager->sessions[i].lock, NULL);
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->session_lock);
    
    if (session_id == -1) {
        printf("No hay sesiones disponibles\n");
        return -1;
    }
    
    GameSession *session = &manager->sessions[session_id];
    
    // 2. Aceptar jugadores para esta sesión
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            if (i > 0) close(session->players[0].socket);
            manager->sessions[session_id].active = false;
            continue;
        }
        
        session->players[i].socket = new_socket;
        session->players[i].ready = false;
        
        char buffer[BUFFER_SIZE] = {0};
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) <= 0) {
            close(new_socket);
            for (int j = 0; j < i; j++) {
                close(session->players[j].socket);
            }
            manager->sessions[session_id].active = false;
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
            for (int j = 0; j < i; j++) {
                close(session->players[j].socket);
            }
            manager->sessions[session_id].active = false;
            i--;
            continue;
        }
    }
    
    // 3. Ambos jugadores conectados - crear hilo del juego
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_game_session, session)) {
        perror("Error al crear hilo del juego");
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            close(session->players[i].socket);
        }
        manager->sessions[session_id].active = false;
        return -1;
    }
    
    pthread_detach(thread_id);
    
    printf("Sesión %d iniciada\n", session_id);
    
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

void init_board(Board *board){
    memset(board->board, WATER, sizeof(board->board));
    board->ship_count = 0;
}

void *handle_game_session(void *arg){
    GameSession *session = (GameSession *)arg;

    // Inicializar tableros
    for(int i = 0; i < MAX_PLAYERS; i++){
        init_board(&session->players[i].board);
        session->players[i].ready = true;
    }

    session->current_turn = 0;
    session->game_over = false;

    for(int i = 0; i < MAX_PLAYERS; i++){
        send_message(session->players[i].socket, "GAME_START", "READY");
    }

    while (!session->game_over) {
        Player *current = &session->players[session->current_turn];
        Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];
        
        char formatted_message[BUFFER_SIZE];
        snprintf(formatted_message, sizeof(formatted_message), "Es tu turno, %s", current->name);
        send_message(current->socket, "YOUR_TURN", formatted_message);
        send_message(opponent->socket, "WAIT_TURN", "OPPONENT_TURN");

        // Wait answer from current player
        char buffer[BUFFER_SIZE] = {0};
        int bytes_received = recv(current->socket, buffer, BUFFER_SIZE, 0);
        
        //Space for disconnection from player
      

        if(strncmp(buffer, "END_TURN|", 9) == 0){
            session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
            printf("Turno cambiado a %s\n", session->players[session->current_turn].name);
        } else if(strncmp(buffer, "QUIT|", 5) == 0) {
            send_message(current->socket, "GAME_OVER", "YOU_QUIT");
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
            session->game_over = true;
        } else {
            send_message(current->socket, "ERROR", "INVALID_COMMAND");
        }
    }

    // Close Sockets
    for (int i = 0; i < MAX_PLAYERS; i++){
        close(session->players[i].socket);
    }
    session->active = false;
    
    return NULL;
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
            sleep(1);
        }
    }
    
    close(server_fd);
    return 0;
}