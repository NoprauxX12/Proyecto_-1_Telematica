/**
 * Servidor de Juego de Batalla Naval
 * 
 * Este servidor implementa un juego de batalla naval multijugador con las siguientes características:
 * - Soporte para múltiples sesiones de juego simultáneas
 * - Sistema de turnos
 * - Validación de movimientos
 * - Registro detallado de acciones
 * - Manejo de desconexiones
 * - Persistencia del estado del juego
 * 
 * Protocolo de Comunicación:
 * - LOGIN|nombre: Inicio de sesión del jugador
 * - PLACE_SHIP|nombre|fila,columna,tamaño,orientación: Colocación de barco
 * - SHOT|fila,columna: Realizar un disparo
 * - END_TURN|: Finalizar turno
 * - QUIT|: Abandonar juego
 * 
 * Estados del Tablero:
 * - ~ : Agua (sin disparar)
 * - O : Barco
 * - X : Impacto
 * - # : Barco hundido
 * 
 * @author Equipo de Desarrollo
 * @version 1.0
 */

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
/**
 * Configuración del Servidor:
 * - PORT: Puerto de escucha (8080)
 * - MAX_PLAYERS: Número de jugadores por sesión (2)
 * - BUFFER_SIZE: Tamaño máximo de mensajes
 * - MAX_SESSIONS: Número máximo de sesiones simultáneas
 * - BOARD_SIZE: Tamaño del tablero (10x10)
 * - MAX_SHIPS: Número máximo de barcos por jugador
 */
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

// ======================= Game Status Constants =======================
/**
 * Límites para el seguimiento del estado del juego:
 * - MAX_LOG_ENTRIES: Máximo número de entradas en el log
 * - MAX_SHOTS: Máximo número de disparos por jugador
 */
#define MAX_LOG_ENTRIES 100

// ======================= Logging Constants =======================
#define LOG_FILE "game_log.txt"
#define MAX_LOG_LINE 256
#define LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S"

// ========================= Structures =========================
/**
 * Estructuras de Datos Principales:
 * 
 * ShipType: Define tipos de barcos y sus tamaños
 * Ship: Representa un barco en el juego
 * Board: Tablero de juego con barcos y estado
 * Player: Información del jugador
 * GameSession: Sesión de juego completa
 * SessionManager: Gestor de múltiples sesiones
 * 
 * Estructuras de Estado:
 * - ShotLog: Registro de disparos
 * - ShipStatus: Estado de un barco
 * - PlayerGameStatus: Estado del juego para un jugador
 * - GameStatus: Estado general del juego
 */

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

// ======================= Game Status Structures =======================
typedef struct {
    int row;
    int col;
    char result;  // 'X' for hit, '~' for miss, '#' for sunk
    time_t timestamp;
} ShotLog;

typedef struct {
    char ship_name[20];
    int size;
    int positions[5][2];
    bool is_sunk;
} ShipStatus;

typedef struct {
    ShotLog shots[MAX_SHOTS];
    int shot_count;
    ShipStatus ships[MAX_SHIPS];
    int ship_count;
    time_t last_action;
    bool is_disconnected;
} PlayerGameStatus;

typedef struct {
    PlayerGameStatus players[MAX_PLAYERS];
    time_t game_start_time;
    int current_turn;
    bool is_active;
} GameStatus;

typedef struct {
    char name[20];
    int size;
    int hits;
    int positions[5][2]; 
} Ship;

typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];      // Tablero real con barcos
    char visible_board[BOARD_SIZE][BOARD_SIZE]; // Tablero visible para el oponente
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
    GameStatus game_status;
} GameSession;

typedef struct {
    GameSession *sessions;
    int max_sessions;
    pthread_mutex_t session_lock;
} SessionManager;

// ========================= Function Prototypes =========================
// Funciones principales
void* handle_game_session(void *arg);
void send_message(int socket, const char *type, const char *data);
SessionManager* session_manager_create(int max_sessions);
int accept_players(SessionManager *manager, int server_fd);
int setup_server();

// Funciones de gestión del tablero
void init_board(Board *board);
void update_visible_board(Board *board, int row, int col, char result);
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation);
char* board_to_string(char (*board)[BOARD_SIZE]);

// Funciones de validación
bool is_valid_shot(Board *board, int row, int col);
bool is_valid_ship_placement(Board *board, int row, int col, int size, char orientation);
bool is_players_turn(GameSession *session, int player_index);
bool has_shot_before(GameSession *session, int player_index, int row, int col);

// Funciones de gestión de estado
void init_game_status(GameSession *session);
void log_shot(GameSession *session, int player_index, int row, int col, char result);
void update_ship_status(GameSession *session, int player_index, const char *ship_name, bool is_sunk);
void handle_player_disconnect(GameSession *session, int player_index);
void handle_player_reconnect(GameSession *session, int player_index);
void save_game_state(GameSession *session, const char *filename);
void load_game_state(GameSession *session, const char *filename);

// Funciones de logging
void get_current_datetime(char *buffer, size_t size);
void log_game_action(int session_id, const char *player_name, const char *action, const char *details);
void log_player_connection(int session_id, const char *player_name, int socket);
void log_shot_action(int session_id, const char *player_name, int row, int col, char result);
void log_ship_sunk(int session_id, const char *player_name, const char *ship_name);
void log_ship_placement(int session_id, const char *player_name, const char *ship_name, int row, int col, char orientation);
void log_player_disconnection(int session_id, const char *player_name);
void log_player_reconnection(int session_id, const char *player_name);

// ========================= Function Implementations =========================
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
            log_player_connection(session_id, session->players[i].name, new_socket);
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
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
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

void *handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    int session_id = -1; // Necesitamos obtener el ID de la sesión

    // Initialize game status
    init_game_status(session);

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
        
        // Enviar tablero propio al jugador actual
        char board_msg[BUFFER_SIZE];
        snprintf(board_msg, sizeof(board_msg), "YOUR_BOARD|%s", 
                board_to_string(current->board.board));
        send_message(current->socket, "GAME_STATUS", board_msg);

        // Enviar tablero visible del oponente
        snprintf(board_msg, sizeof(board_msg), "OPPONENT_BOARD|%s", 
                board_to_string(opponent->board.visible_board));
        send_message(current->socket, "GAME_STATUS", board_msg);

        char formatted_message[BUFFER_SIZE];
        snprintf(formatted_message, sizeof(formatted_message), "Es tu turno, %s", current->name);
        send_message(current->socket, "YOUR_TURN", formatted_message);
        send_message(opponent->socket, "WAIT_TURN", "OPPONENT_TURN");

        // Wait answer from current player
        char buffer[BUFFER_SIZE] = {0};
        int bytes_received = recv(current->socket, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received <= 0) {
            // Player disconnected
            handle_player_disconnect(session, session->current_turn);
            log_player_disconnection(session_id, current->name);
            char disconnect_msg[BUFFER_SIZE];
            snprintf(disconnect_msg, sizeof(disconnect_msg), "OPPONENT_DISCONNECTED|%s", current->name);
            send_message(opponent->socket, "GAME_STATUS", disconnect_msg);
            
            // Save game state
            char save_file[50];
            snprintf(save_file, sizeof(save_file), "game_%d.dat", session->current_turn);
            save_game_state(session, save_file);
            
            // Wait for reconnection or timeout
            time_t start_wait = time(NULL);
            while (time(NULL) - start_wait < 60) { // 60 second timeout
                if (recv(current->socket, buffer, BUFFER_SIZE, 0) > 0) {
                    handle_player_reconnect(session, session->current_turn);
                    log_player_reconnection(session_id, current->name);
                    break;
                }
                sleep(1);
            }
            
            if (session->game_status.players[session->current_turn].is_disconnected) {
                send_message(opponent->socket, "GAME_OVER", "OPPONENT_TIMEOUT");
                session->game_over = true;
                continue;
            }
        }

        if(strncmp(buffer, "END_TURN|", 9) == 0){
            session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
            printf("Turno cambiado a %s\n", session->players[session->current_turn].name);
        } else if(strncmp(buffer, "QUIT|", 5) == 0) {
            send_message(current->socket, "GAME_OVER", "YOU_QUIT");
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
            session->game_over = true;
        } else if(strncmp(buffer, "SHOT|", 5) == 0) {
            int row, col;
            sscanf(buffer + 5, "%d,%d", &row, &col);
            
            if (!is_valid_shot(&opponent->board, row, col)) {
                send_message(current->socket, "ERROR", "INVALID_SHOT");
                continue;
            }
            
            if (has_shot_before(session, session->current_turn, row, col)) {
                send_message(current->socket, "ERROR", "REPEATED_SHOT");
                continue;
            }
            
            char result = opponent->board.board[row][col];
            log_shot(session, session->current_turn, row, col, result);
            log_shot_action(session_id, current->name, row, col, result);
            
            // Actualizar tablero visible
            update_visible_board(&opponent->board, row, col, result);
            
            // Si el barco fue hundido, marcar todas sus posiciones como hundidas
            if (result == SHIP) {
                for (int i = 0; i < opponent->board.ship_count; i++) {
                    Ship *ship = &opponent->board.ships[i];
                    for (int j = 0; j < ship->size; j++) {
                        if (ship->positions[j][0] == row && ship->positions[j][1] == col) {
                            ship->hits++;
                            if (ship->hits >= ship->size) {
                                log_ship_sunk(session_id, opponent->name, ship->name);
                                // Marcar todas las posiciones del barco como hundidas
                                for (int k = 0; k < ship->size; k++) {
                                    update_visible_board(&opponent->board, 
                                        ship->positions[k][0], 
                                        ship->positions[k][1], 
                                        SUNK);
                                }
                                update_ship_status(session, 
                                    (session->current_turn + 1) % MAX_PLAYERS,
                                    ship->name, true);
                            }
                            break;
                        }
                    }
                }
            }
            
            // Enviar resultado del disparo a ambos jugadores
            char shot_result[BUFFER_SIZE];
            snprintf(shot_result, sizeof(shot_result), "SHOT_RESULT|%d,%d,%c", row, col, result);
            send_message(current->socket, "GAME_STATUS", shot_result);
            send_message(opponent->socket, "GAME_STATUS", shot_result);
        } else if(strncmp(buffer, "PLACE_SHIP|", 11) == 0) {
            char ship_name[20];
            int row, col, size;
            char orientation;
            sscanf(buffer + 11, "%[^|]|%d,%d,%d,%c", ship_name, &row, &col, &size, &orientation);
            
            if (!is_valid_ship_placement(&current->board, row, col, size, orientation)) {
                send_message(current->socket, "ERROR", "INVALID_SHIP_PLACEMENT");
                continue;
            }
            
            log_ship_placement(session_id, current->name, ship_name, row, col, orientation);
            // Process valid ship placement
            // ... ship placement code ...
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

// Implementation of game status management functions
void init_game_status(GameSession *session) {
    memset(&session->game_status, 0, sizeof(GameStatus));
    session->game_status.game_start_time = time(NULL);
    session->game_status.is_active = true;
    session->game_status.current_turn = 0;
    
    // Initialize both players' status
    for (int i = 0; i < MAX_PLAYERS; i++) {
        session->game_status.players[i].shot_count = 0;
        session->game_status.players[i].ship_count = 0;
        session->game_status.players[i].last_action = time(NULL);
        session->game_status.players[i].is_disconnected = false;
    }
}

void log_shot(GameSession *session, int player_index, int row, int col, char result) {
    if (player_index >= 0 && player_index < MAX_PLAYERS && 
        session->game_status.players[player_index].shot_count < MAX_SHOTS) {
        
        ShotLog *shot = &session->game_status.players[player_index].shots[session->game_status.players[player_index].shot_count];
        shot->row = row;
        shot->col = col;
        shot->result = result;
        shot->timestamp = time(NULL);
        
        session->game_status.players[player_index].shot_count++;
        session->game_status.players[player_index].last_action = shot->timestamp;
    }
}

void update_ship_status(GameSession *session, int player_index, const char *ship_name, bool is_sunk) {
    if (player_index >= 0 && player_index < MAX_PLAYERS) {
        for (int i = 0; i < session->game_status.players[player_index].ship_count; i++) {
            if (strcmp(session->game_status.players[player_index].ships[i].ship_name, ship_name) == 0) {
                session->game_status.players[player_index].ships[i].is_sunk = is_sunk;
                break;
            }
        }
    }
}

void handle_player_disconnect(GameSession *session, int player_index) {
    if (player_index >= 0 && player_index < MAX_PLAYERS) {
        session->game_status.players[player_index].is_disconnected = true;
        session->game_status.players[player_index].last_action = time(NULL);
    }
}

void handle_player_reconnect(GameSession *session, int player_index) {
    if (player_index >= 0 && player_index < MAX_PLAYERS) {
        session->game_status.players[player_index].is_disconnected = false;
        session->game_status.players[player_index].last_action = time(NULL);
    }
}

void save_game_state(GameSession *session, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file) {
        fwrite(&session->game_status, sizeof(GameStatus), 1, file);
        fclose(file);
    }
}

void load_game_state(GameSession *session, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file) {
        fread(&session->game_status, sizeof(GameStatus), 1, file);
        fclose(file);
    }
}

// ========================= Validation Functions Implementation =========================

/**
 * Validates if a shot position is within board boundaries and hasn't been shot before
 * @param board The game board
 * @param row Row position (0-9)
 * @param col Column position (0-9)
 * @return true if the shot is valid, false otherwise
 */
bool is_valid_shot(Board *board, int row, int col) {
    // Check boundaries
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return false;
    }
    
    // Check if position was already shot (marked as HIT, MISS, or SUNK)
    char position = board->board[row][col];
    if (position == HIT || position == SUNK || position == WATER) {
        return false;
    }
    
    return true;
}

/**
 * Checks if a player has already shot at a specific position
 * @param session Current game session
 * @param player_index Index of the player
 * @param row Row position
 * @param col Column position
 * @return true if the position was shot before, false otherwise
 */
bool has_shot_before(GameSession *session, int player_index, int row, int col) {
    PlayerGameStatus *player = &session->game_status.players[player_index];
    
    for (int i = 0; i < player->shot_count; i++) {
        if (player->shots[i].row == row && player->shots[i].col == col) {
            return true;
        }
    }
    return false;
}

/**
 * Validates if a ship can be placed at the specified position
 * @param board The game board
 * @param row Starting row position
 * @param col Starting column position
 * @param size Ship size
 * @param orientation 'H' for horizontal, 'V' for vertical
 * @return true if placement is valid, false otherwise
 */
bool is_valid_ship_placement(Board *board, int row, int col, int size, char orientation) {
    // Check boundaries
    if (row < 0 || col < 0) return false;
    
    if (orientation == 'H') {
        if (col + size > BOARD_SIZE) return false;
        // Check for overlapping ships
        for (int i = 0; i < size; i++) {
            if (board->board[row][col + i] != WATER) {
                return false;
            }
        }
    } else if (orientation == 'V') {
        if (row + size > BOARD_SIZE) return false;
        // Check for overlapping ships
        for (int i = 0; i < size; i++) {
            if (board->board[row + i][col] != WATER) {
                return false;
            }
        }
    } else {
        return false; // Invalid orientation
    }
    
    return true;
}

/**
 * Validates if it's the player's turn
 * @param session Current game session
 * @param player_index Index of the player attempting an action
 * @return true if it's the player's turn, false otherwise
 */
bool is_players_turn(GameSession *session, int player_index) {
    return session->current_turn == player_index && !session->game_over;
}

// ========================= Board Functions =========================
/**
 * Inicializa un tablero con agua en todas las posiciones
 * @param board Puntero al tablero a inicializar
 */
void init_board(Board *board) {
    memset(board->board, WATER, sizeof(board->board));
    memset(board->visible_board, WATER, sizeof(board->visible_board));
    board->ship_count = 0;
}

/**
 * Actualiza el tablero visible después de un disparo
 * @param board Puntero al tablero
 * @param row Fila del disparo
 * @param col Columna del disparo
 * @param result Resultado del disparo (HIT, WATER, SUNK)
 */
void update_visible_board(Board *board, int row, int col, char result) {
    board->visible_board[row][col] = result;
}

/**
 * Coloca un barco en el tablero
 * @param board Puntero al tablero
 * @param ship_name Nombre del barco
 * @param size Tamaño del barco
 * @param row Fila inicial
 * @param col Columna inicial
 * @param orientation Orientación (H/V)
 * @return true si se pudo colocar el barco, false en caso contrario
 */
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation) {
    if (!is_valid_ship_placement(board, row, col, size, orientation)) {
        return false;
    }
    
    Ship *ship = &board->ships[board->ship_count];
    strncpy(ship->name, ship_name, sizeof(ship->name) - 1);
    ship->size = size;
    ship->hits = 0;
    
    if (orientation == 'H') {
        for (int i = 0; i < size; i++) {
            board->board[row][col + i] = SHIP;
            ship->positions[i][0] = row;
            ship->positions[i][1] = col + i;
        }
    } else {
        for (int i = 0; i < size; i++) {
            board->board[row + i][col] = SHIP;
            ship->positions[i][0] = row + i;
            ship->positions[i][1] = col;
        }
    }
    
    board->ship_count++;
    return true;
}

/**
 * Convierte un tablero a string para enviarlo al cliente
 * @param board Puntero al tablero a convertir
 * @return String representando el tablero
 */
char* board_to_string(char (*board)[BOARD_SIZE]) {
    static char result[BOARD_SIZE * BOARD_SIZE + BOARD_SIZE + 1];
    int pos = 0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            result[pos++] = board[i][j];
        }
        result[pos++] = '\n';
    }
    result[pos] = '\0';
    
    return result;
}

// ========================= Logging Functions =========================
/**
 * Sistema de Logging:
 * - get_current_datetime: Obtiene fecha/hora actual
 * - log_game_action: Función base de logging
 * - log_player_connection: Registra conexiones
 * - log_shot_action: Registra disparos
 * - log_ship_sunk: Registra hundimientos
 * - log_ship_placement: Registra colocación de barcos
 * - log_player_disconnection: Registra desconexiones
 * - log_player_reconnection: Registra reconexiones
 */

/**
 * Obtiene la fecha y hora actual formateada
 * @param buffer Buffer donde se guardará la fecha/hora
 * @param size Tamaño del buffer
 */
void get_current_datetime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, LOG_DATE_FORMAT, tm_info);
}

/**
 * Escribe un mensaje en el archivo de log
 * @param session_id ID de la sesión de juego
 * @param player_name Nombre del jugador
 * @param action Tipo de acción realizada
 * @param details Detalles adicionales de la acción
 */
void log_game_action(int session_id, const char *player_name, const char *action, const char *details) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        char datetime[32];
        get_current_datetime(datetime, sizeof(datetime));
        fprintf(log_file, "[%s] Session %d - Player %s - %s - %s\n", 
                datetime, session_id, player_name, action, details);
        fclose(log_file);
    }
}

/**
 * Registra la conexión de un jugador
 * @param session_id ID de la sesión
 * @param player_name Nombre del jugador
 * @param socket Socket del jugador
 */
void log_player_connection(int session_id, const char *player_name, int socket) {
    char details[64];
    snprintf(details, sizeof(details), "Socket: %d", socket);
    log_game_action(session_id, player_name, "CONNECTION", details);
}

/**
 * Registra el hundimiento de un barco
 * @param session_id ID de la sesión
 * @param player_name Nombre del jugador
 * @param ship_name Nombre del barco hundido
 */
void log_ship_sunk(int session_id, const char *player_name, const char *ship_name) {
    char details[64];
    snprintf(details, sizeof(details), "Ship: %s", ship_name);
    log_game_action(session_id, player_name, "SHIP_SUNK", details);
}

/**
 * Registra la colocación de un barco
 * @param session_id ID de la sesión
 * @param player_name Nombre del jugador
 * @param ship_name Nombre del barco
 * @param row Fila inicial
 * @param col Columna inicial
 * @param orientation Orientación
 */
void log_ship_placement(int session_id, const char *player_name, const char *ship_name, 
                       int row, int col, char orientation) {
    char details[128];
    snprintf(details, sizeof(details), "Ship: %s - Position: (%d,%d) - Orientation: %c",
             ship_name, row, col, orientation);
    log_game_action(session_id, player_name, "SHIP_PLACEMENT", details);
}

/**
 * Registra la desconexión de un jugador
 * @param session_id ID de la sesión
 * @param player_name Nombre del jugador
 */
void log_player_disconnection(int session_id, const char *player_name) {
    log_game_action(session_id, player_name, "DISCONNECTION", "Player disconnected");
}

/**
 * Registra la reconexión de un jugador
 * @param session_id ID de la sesión
 * @param player_name Nombre del jugador
 */
void log_player_reconnection(int session_id, const char *player_name) {
    log_game_action(session_id, player_name, "RECONNECTION", "Player reconnected");
}

// Nueva función para logging de disparos
void log_shot_action(int session_id, const char *player_name, int row, int col, char result) {
    char details[64];
    snprintf(details, sizeof(details), "Position: (%d,%d) - Result: %c", row, col, result);
    log_game_action(session_id, player_name, "SHOT", details);
}

// ========================= Main =========================
/**
 * Función Principal:
 * 1. Inicializa el servidor
 * 2. Crea el gestor de sesiones
 * 3. Entra en bucle principal aceptando conexiones
 * 4. Maneja múltiples sesiones de juego
 */
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