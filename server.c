// === BATTLESHIP SERVER COMPLETO ===

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define PORT 8080
#define MAX_PLAYERS 2
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 10
#define BOARD_SIZE 10
#define MAX_SHIPS 9

#define WATER '~'
#define HIT 'X'
#define SUNK '#'
#define SHIP 'O'

#define LOG_FILE "game_log.txt"
#define LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S"

// === ESTRUCTURAS ===
typedef struct {
    char name[20];
    int size;
} ShipType;

ShipType SHIPS[MAX_SHIPS] = {
    {"Portaavion", 5}, {"Buque de Guerra", 4}, {"Crucero1", 3},
    {"Crucero2", 3}, {"Destructor1", 2}, {"Destructor2", 2},
    {"Submarino1", 1}, {"Submarino2", 1}, {"Submarino3", 1}
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

// === FUNCIONES ===
void send_message(int socket, const char *type, const char *data) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s|%s", type, data);
    send(socket, message, strlen(message), 0);
}

void init_board(Board *board) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board->board[i][j] = WATER;
    board->ship_count = 0;
}

bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation) {
    if ((orientation != 'H' && orientation != 'V') ||
        (orientation == 'H' && col + size > BOARD_SIZE) ||
        (orientation == 'V' && row + size > BOARD_SIZE))
        return false;

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

void get_current_datetime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, LOG_DATE_FORMAT, tm_info);
}

void log_board_state(FILE *log_file, Board *board, const char *player_name) {
    fprintf(log_file, "\nTablero de %s:\n", player_name);
    fprintf(log_file, "  ");
    for (int i = 0; i < BOARD_SIZE; i++) fprintf(log_file, "%d ", i);
    fprintf(log_file, "\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(log_file, "%d ", i);
        for (int j = 0; j < BOARD_SIZE; j++)
            fprintf(log_file, "%c ", board->board[i][j]);
        fprintf(log_file, "\n");
    }
}

void log_game_start(FILE *log_file, const char *p1, const char *p2) {
    char dt[32];
    get_current_datetime(dt, sizeof(dt));
    fprintf(log_file, "\n=== NUEVA PARTIDA [%s] ===\nJugador 1: %s\nJugador 2: %s\n===========================\n", dt, p1, p2);
}

void log_game_end(FILE *log_file, const char *ganador, const char *perdedor) {
    char dt[32];
    get_current_datetime(dt, sizeof(dt));
    fprintf(log_file, "\n=== FIN PARTIDA [%s] ===\nGanador: %s\nPerdedor: %s\n========================\n", dt, ganador, perdedor);
}

void advance_turn(GameSession *session) {
    session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
}

void* handle_game_session(void *arg);

int accept_players(SessionManager *manager, int server_fd);

SessionManager* session_manager_create(int max_sessions) {
    SessionManager *manager = malloc(sizeof(SessionManager));
    if (!manager) return NULL;
    manager->max_sessions = max_sessions;
    manager->sessions = malloc(sizeof(GameSession) * max_sessions);
    if (!manager->sessions) {
        free(manager);
        return NULL;
    }
    for (int i = 0; i < max_sessions; i++) {
        manager->sessions[i].active = false;
        pthread_mutex_init(&manager->sessions[i].lock, NULL);
    }
    pthread_mutex_init(&manager->session_lock, NULL);
    return manager;
}

void* handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) return NULL;

    log_game_start(log_file, session->players[0].name, session->players[1].name);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        init_board(&session->players[i].board);
        session->players[i].ready = true;
        session->players[i].ships_placed = false;

        for (int s = 0; s < MAX_SHIPS; s++) {
            char placement_msg[BUFFER_SIZE];
            snprintf(placement_msg, sizeof(placement_msg), "%s|%d", SHIPS[s].name, SHIPS[s].size);
            send_message(session->players[i].socket, "PLACE_SHIP", placement_msg);

            char buffer[BUFFER_SIZE] = {0};
            int bytes = recv(session->players[i].socket, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0 || strncmp(buffer, "SHIP_POS|", 9) != 0) {
                send_message(session->players[i].socket, "ERROR", "INVALID_SHIP_POSITION");
                fclose(log_file);
                return NULL;
            }

            int row, col;
            char orientation;
            sscanf(buffer + 9, "%d,%d,%c", &row, &col, &orientation);
            if (!place_ship(&session->players[i].board, SHIPS[s].name, SHIPS[s].size, row, col, orientation)) {
                send_message(session->players[i].socket, "ERROR", "INVALID_SHIP_POSITION");
                fclose(log_file);
                return NULL;
            }
        }

        session->players[i].ships_placed = true;
        log_board_state(log_file, &session->players[i].board, session->players[i].name);
    }

    session->current_turn = 0;
    session->game_over = false;

    while (!session->game_over) {
        Player *current = &session->players[session->current_turn];
        Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];

        send_message(current->socket, "YOUR_TURN", "Dispara con ROW,COL o R para rendirte");
        send_message(opponent->socket, "WAIT_TURN", "Espera al oponente");

        char buffer[BUFFER_SIZE] = {0};
        int bytes = recv(current->socket, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0 || strncmp(buffer, "QUIT|", 5) == 0) {
            send_message(current->socket, "GAME_OVER", "YOU_QUIT");
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
            log_game_end(log_file, opponent->name, current->name);
            break;
        }

        int row, col;
        if (sscanf(buffer, "%d,%d", &row, &col) != 2 || row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
            send_message(current->socket, "ERROR", "POSICION_INVALIDA");
            continue;
        }

        char cell = opponent->board.board[row][col];
        if (cell == SHIP) {
            opponent->board.board[row][col] = HIT;
            send_message(current->socket, "RESULT", "HIT");
            char info[32];
            snprintf(info, sizeof(info), "%d,%d", row, col);
            send_message(opponent->socket, "ENEMY_HIT", info);

            for (int s = 0; s < opponent->board.ship_count; s++) {
                Ship *ship = &opponent->board.ships[s];
                for (int i = 0; i < ship->size; i++) {
                    if (ship->positions[i][0] == row && ship->positions[i][1] == col) {
                        ship->hits++;
                        break;
                    }
                }
            }

            bool all_sunk = true;
            for (int s = 0; s < opponent->board.ship_count; s++) {
                if (opponent->board.ships[s].hits < opponent->board.ships[s].size) {
                    all_sunk = false;
                    break;
                }
            }

            if (all_sunk) {
                send_message(current->socket, "GAME_OVER", "YOU_WIN");
                send_message(opponent->socket, "GAME_OVER", "YOU_LOSE");
                log_game_end(log_file, current->name, opponent->name);
                session->game_over = true;
                break;
            }
        } else {
            opponent->board.board[row][col] = WATER;
            send_message(current->socket, "RESULT", "MISS");
        }

        advance_turn(session);
    }

    for (int i = 0; i < MAX_PLAYERS; i++) close(session->players[i].socket);
    session->active = false;
    fclose(log_file);
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
        if (new_socket < 0) return -1;

        session->players[i].socket = new_socket;
        session->players[i].ready = false;

        char buffer[BUFFER_SIZE] = {0};
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) <= 0) return -1;

        if (strncmp(buffer, "LOGIN|", 6) == 0) {
            strncpy(session->players[i].name, buffer + 6, sizeof(session->players[i].name) - 1);
            send_message(new_socket, "LOGIN", "OK");
        } else {
            send_message(new_socket, "ERROR", "INVALID_LOGIN");
            return -1;
        }
    }

    pthread_t tid;
    pthread_create(&tid, NULL, handle_game_session, session);
    pthread_detach(tid);
    return session_id;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(EXIT_FAILURE);
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) exit(EXIT_FAILURE);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, 10) < 0) exit(EXIT_FAILURE);

    printf("Servidor iniciado en el puerto %d\n", PORT);
    SessionManager *manager = session_manager_create(MAX_SESSIONS);
    printf("Esperando jugadores...\n");

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