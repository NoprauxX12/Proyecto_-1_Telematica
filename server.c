// ======================= Includes =======================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
 
// ======================= Constants =========================
#define PORT 8080
#define MAX_PLAYERS 2
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 10
#define BOARD_SIZE 10
#define MAX_SHIPS 5
#define MAX_SHOTS 100
#define WATER '~'
#define HIT 'X'
#define SUNK '#'
#define SHIP 'O'
#define MAX_LOG_ENTRIES 100
#define LOG_FILE "game_log.txt"
#define MAX_LOG_LINE 256
#define LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S"
 
// ======================= Structures =======================
typedef struct {
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
    bool ships_placed;
} Player;
 
typedef struct {
    Player players[MAX_PLAYERS];
    bool active;
    pthread_mutex_t lock;
    int current_turn;
    bool game_over;
} GameSession;
 
typedef struct {
    GameSession *sessions;
    int max_sessions;
    pthread_mutex_t session_lock;
} SessionManager;
 
// ======================= Function Prototypes =======================
void send_message(int socket, const char *type, const char *data);
void init_board(Board *board);
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation);
void* handle_game_session(void *arg);
SessionManager* session_manager_create(int max_sessions);
int accept_players(SessionManager *manager, int server_fd);
 
// ======================= Function Implementations =======================
void send_message(int socket, const char *type, const char *data) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s|%s", type, data);
    send(socket, message, strlen(message), 0);
}
 
void init_board(Board *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->board[i][j] = WATER;
        }
    }
    board->ship_count = 0;
}
 
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation) {
    if (orientation != 'H' && orientation != 'V') return false;
    if ((orientation == 'H' && col + size > BOARD_SIZE) ||
        (orientation == 'V' && row + size > BOARD_SIZE)) return false;
 
    for (int i = 0; i < size; i++) {
        int r = row + (orientation == 'V' ? i : 0);
        int c = col + (orientation == 'H' ? i : 0);
        if (board->board[r][c] != WATER) return false;
    }
 
    Ship *ship = &board->ships[board->ship_count];
    strncpy(ship->name, ship_name, sizeof(ship->name));
    ship->size = size;
    ship->hits = 0;
    for (int i = 0; i < size; i++) {
        int r = row + (orientation == 'V' ? i : 0);
        int c = col + (orientation == 'H' ? i : 0);
        board->board[r][c] = SHIP;
        ship->positions[i][0] = r;
        ship->positions[i][1] = c;
    }
    board->ship_count++;
    return true;
}
 
SessionManager* session_manager_create(int max_sessions) {
    SessionManager *manager = malloc(sizeof(SessionManager));
    manager->max_sessions = max_sessions;
    manager->sessions = malloc(sizeof(GameSession) * max_sessions);
    for (int i = 0; i < max_sessions; i++) {
        manager->sessions[i].active = false;
        pthread_mutex_init(&manager->sessions[i].lock, NULL);
    }
    pthread_mutex_init(&manager->session_lock, NULL);
    return manager;
}
 
void* handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        init_board(&session->players[i].board);
        session->players[i].ready = true;
        session->players[i].ships_placed = false;
        for (int s = 0; s < MAX_SHIPS; s++) {
            char placement_msg[BUFFER_SIZE];
            snprintf(placement_msg, sizeof(placement_msg), "PLACE_SHIP|%s|%d", SHIPS[s].name, SHIPS[s].size);
            send_message(session->players[i].socket, "PLACE_SHIP", placement_msg);
            char buffer[BUFFER_SIZE] = {0};
            int bytes = recv(session->players[i].socket, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0 || strncmp(buffer, "SHIP_POS|", 9) != 0) {
                send_message(session->players[i].socket, "ERROR", "INVALID_SHIP_POSITION");
                return NULL;
            }
            int row, col;
            char orientation;
            sscanf(buffer + 9, "%d,%d,%c", &row, &col, &orientation);
            if (!place_ship(&session->players[i].board, SHIPS[s].name, SHIPS[s].size, row, col, orientation)) {
                send_message(session->players[i].socket, "ERROR", "INVALID_SHIP_POSITION");
                return NULL;
            }
        }
        session->players[i].ships_placed = true;
    }
 
    session->current_turn = 0;
    session->game_over = false;
    while (!session->game_over) {
        Player *current = &session->players[session->current_turn];
        Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];
        char buffer[BUFFER_SIZE] = {0};
        send_message(current->socket, "YOUR_TURN", "Dispara con formato ROW,COL o QUIT|");
        send_message(opponent->socket, "WAIT_TURN", "Espera al oponente");
        int bytes_received = recv(current->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_DISCONNECTED");
            session->game_over = true;
            break;
        }
        if (strncmp(buffer, "QUIT|", 5) == 0) {
            send_message(current->socket, "GAME_OVER", "YOU_QUIT");
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
            session->game_over = true;
            break;
        } else {
            int row, col;
            sscanf(buffer, "%d,%d", &row, &col);
            if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
                send_message(current->socket, "ERROR", "POSICION_INVALIDA");
                continue;
            }
            char result = opponent->board.board[row][col];
            if (result == SHIP) {
                opponent->board.board[row][col] = HIT;
            
                // Verificar si se hundió un barco
                bool sunk = false;
                for (int s = 0; s < opponent->board.ship_count; s++) {
                    Ship *ship = &opponent->board.ships[s];
                    bool all_hit = true;
                    for (int i = 0; i < ship->size; i++) {
                        int sr = ship->positions[i][0];
                        int sc = ship->positions[i][1];
                        if (opponent->board.board[sr][sc] != HIT) {
                            all_hit = false;
                            break;
                        }
                    }
                    if (all_hit) {
                        sunk = true;
                        break;
                    }
                }
            
                if (sunk) {
                    send_message(current->socket, "RESULT", "SUNK");
                } else {
                    send_message(current->socket, "RESULT", "HIT");
                }
            } else {
                opponent->board.board[row][col] = WATER;
                send_message(current->socket, "RESULT", "MISS");
            }
            session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
        }
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(session->players[i].socket);
    }
    session->active = false;
    return NULL;
}
 
int accept_players(SessionManager *manager, int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
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
    if (session_id == -1) return -1;
    GameSession *session = &manager->sessions[session_id];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            if (i > 0) close(session->players[0].socket);
            session->active = false;
            return -1;
        }
        session->players[i].socket = new_socket;
        session->players[i].ready = false;
        char buffer[BUFFER_SIZE] = {0};
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) <= 0) {
            close(new_socket);
            for (int j = 0; j < i; j++) close(session->players[j].socket);
            session->active = false;
            return -1;
        }
        if (strncmp(buffer, "LOGIN|", 6) == 0) {
            strncpy(session->players[i].name, buffer + 6, sizeof(session->players[i].name) - 1);
            send_message(session->players[i].socket, "LOGIN", "OK");
        } else {
            send_message(session->players[i].socket, "ERROR", "INVALID_LOGIN");
            close(new_socket);
            for (int j = 0; j < i; j++) close(session->players[j].socket);
            session->active = false;
            return -1;
        }
    }
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_game_session, session) != 0) {
        perror("pthread_create");
        for (int i = 0; i < MAX_PLAYERS; i++) close(session->players[i].socket);
        session->active = false;
        return -1;
    }
    pthread_detach(thread_id);
    return session_id;
}
 
int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
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
    printf("Servidor iniciado en el puerto %d\n", PORT);
    SessionManager *manager = session_manager_create(MAX_SESSIONS);
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