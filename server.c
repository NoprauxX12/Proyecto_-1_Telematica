/*
 * BATTLESHIP - SERVIDOR
 * Este es el servidor del juego de Batalla Naval.
 * Maneja la lógica del juego, la comunicación entre jugadores y el registro de partidas.
 */

// ======================= Includes =======================
// Librerías necesarias para el funcionamiento del servidor
#include <stdio.h>      // Para entrada/salida
#include <stdlib.h>     // Para funciones generales
#include <string.h>     // Para manipulación de strings
#include <unistd.h>     // Para funciones del sistema
#include <sys/socket.h> // Para sockets
#include <netinet/in.h> // Para estructuras de red
#include <pthread.h>    // Para manejo de hilos
#include <stdbool.h>    // Para variables booleanas
#include <time.h>       // Para manejo de tiempo

// ======================= Constantes =========================
// Configuración del servidor y del juego
#define PORT 8080           // Puerto en el que escuchará el servidor
#define MAX_PLAYERS 2       // Número máximo de jugadores por partida
#define BUFFER_SIZE 1024    // Tamaño del buffer para mensajes
#define MAX_SESSIONS 10     // Número máximo de partidas simultáneas
#define BOARD_SIZE 10       // Tamaño del tablero (10x10)
#define MAX_SHIPS 7         // Número de barcos por jugador (2 Cruceros + 2 Destructores + 3 Submarinos)
#define MAX_SHOTS 100       // Máximo número de disparos permitidos

// Símbolos para el tablero
#define WATER '~'   // Agua (sin disparar)
#define HIT 'X'     // Impacto en barco
#define SUNK '#'    // Barco hundido
#define SHIP 'O'    // Barco (solo visible en tablero propio)

// Configuración del sistema de logging
#define MAX_LOG_ENTRIES 100    // Máximo número de entradas en el log
#define LOG_FILE "game_log.txt" // Archivo donde se guarda el log
#define MAX_LOG_LINE 256       // Longitud máxima de una línea del log
#define LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S" // Formato de fecha/hora

// ======================= Estructuras =======================
// Define los tipos de barcos disponibles
typedef struct {
    char name[20];    // Nombre del barco
    int size;         // Tamaño del barco
} ShipType;

// Lista de barcos disponibles en el juego
ShipType SHIPS[MAX_SHIPS] = {
    {"Crucero1", 3},     // Primer Crucero (3 casillas)
    {"Crucero2", 3},     // Segundo Crucero (3 casillas)
    {"Destructor1", 2},  // Primer Destructor (2 casillas)
    {"Destructor2", 2},  // Segundo Destructor (2 casillas)
    {"Submarino1", 1},   // Primer Submarino (1 casilla)
    {"Submarino2", 1},   // Segundo Submarino (1 casilla)
    {"Submarino3", 1}    // Tercer Submarino (1 casilla)
};

// Estructura para representar un barco en el tablero
typedef struct {
    char name[20];        // Nombre del barco
    int size;             // Tamaño del barco
    int hits;             // Número de impactos recibidos
    int positions[5][2];  // Posiciones que ocupa en el tablero
} Ship;

// Estructura para el tablero de un jugador
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];  // Matriz del tablero
    Ship ships[MAX_SHIPS];               // Lista de barcos
    int ship_count;                      // Número de barcos colocados
} Board;

// Estructura para un jugador
typedef struct {
    char name[50];       // Nombre del jugador
    int socket;          // Socket para comunicación
    bool ready;          // Estado de preparación
    Board board;         // Tablero del jugador
    bool ships_placed;   // Indica si los barcos han sido colocados
} Player;

// Estructura para una sesión de juego
typedef struct {
    Player players[MAX_PLAYERS];  // Lista de jugadores
    bool active;                  // Estado de la sesión
    pthread_mutex_t lock;         // Mutex para sincronización
    int current_turn;             // Turno actual
    bool game_over;               // Estado del juego
} GameSession;

// Estructura para el administrador de sesiones
typedef struct {
    GameSession *sessions;        // Lista de sesiones activas
    int max_sessions;            // Máximo número de sesiones
    pthread_mutex_t session_lock; // Mutex para sincronización
} SessionManager;

// ======================= Funciones de Utilidad =======================
// Envía un mensaje a un cliente
void send_message(int socket, const char *type, const char *data) {
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%s|%s", type, data);
    send(socket, message, strlen(message), 0);
}

// Inicializa un tablero vacío
void init_board(Board *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->board[i][j] = WATER;
        }
    }
    board->ship_count = 0;
}

// Coloca un barco en el tablero
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation) {
    // Verifica que la orientación sea válida
    if (orientation != 'H' && orientation != 'V') return false;
    
    // Verifica que el barco quepa en el tablero
    if ((orientation == 'H' && col + size > BOARD_SIZE) ||
        (orientation == 'V' && row + size > BOARD_SIZE)) return false;
 
    // Verifica que no haya otros barcos en el camino
    for (int i = 0; i < size; i++) {
        int r = row + (orientation == 'V' ? i : 0);
        int c = col + (orientation == 'H' ? i : 0);
        if (board->board[r][c] != WATER) return false;
    }
 
    // Coloca el barco en el tablero
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

// ======================= Funciones de Logging =======================
// Obtiene la fecha y hora actual formateada
void get_current_datetime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, LOG_DATE_FORMAT, tm_info);
}

// Escribe el estado del tablero en el log
void log_board_state(FILE *log_file, Board *board, const char *player_name) {
    fprintf(log_file, "\nTablero de %s:\n", player_name);
    fprintf(log_file, "  ");
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(log_file, "%d ", i);
    }
    fprintf(log_file, "\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(log_file, "%d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            fprintf(log_file, "%c ", board->board[i][j]);
        }
        fprintf(log_file, "\n");
    }
    fprintf(log_file, "\n");
}

// Escribe un movimiento en el log
void log_move(FILE *log_file, const char *player_name, int row, int col, const char *result) {
    char datetime[32];
    get_current_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "[%s] %s disparó en (%d,%d) - Resultado: %s\n", 
            datetime, player_name, row, col, result);
}

// Escribe el inicio de una partida en el log
void log_game_start(FILE *log_file, const char *player1_name, const char *player2_name) {
    char datetime[32];
    get_current_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "\n=== NUEVA PARTIDA INICIADA [%s] ===\n", datetime);
    fprintf(log_file, "Jugador 1: %s\n", player1_name);
    fprintf(log_file, "Jugador 2: %s\n", player2_name);
    fprintf(log_file, "================================\n\n");
}

// Escribe el fin de una partida en el log
void log_game_end(FILE *log_file, const char *winner_name, const char *loser_name) {
    char datetime[32];
    get_current_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "\n=== FIN DE LA PARTIDA [%s] ===\n", datetime);
    fprintf(log_file, "Ganador: %s\n", winner_name);
    fprintf(log_file, "Perdedor: %s\n", loser_name);
    fprintf(log_file, "================================\n\n");
}

// ======================= Funciones Principales =======================
// Crea un nuevo administrador de sesiones
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

// Maneja una sesión de juego
void* handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Error abriendo archivo de log");
        return NULL;
    }

    // Registra el inicio de la partida
    log_game_start(log_file, session->players[0].name, session->players[1].name);

    // Fase de colocación de barcos
    for (int i = 0; i < MAX_PLAYERS; i++) {
        init_board(&session->players[i].board);
        session->players[i].ready = true;
        session->players[i].ships_placed = false;
        
        // Coloca cada barco
        for (int s = 0; s < MAX_SHIPS; s++) {
            char placement_msg[BUFFER_SIZE];
            snprintf(placement_msg, sizeof(placement_msg), "PLACE_SHIP|%s|%d", SHIPS[s].name, SHIPS[s].size);
            send_message(session->players[i].socket, "PLACE_SHIP", placement_msg);
            
            // Recibe la posición del barco
            char buffer[BUFFER_SIZE] = {0};
            int bytes = recv(session->players[i].socket, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0 || strncmp(buffer, "SHIP_POS|", 9) != 0) {
                send_message(session->players[i].socket, "ERROR", "INVALID_SHIP_POSITION");
                fclose(log_file);
                return NULL;
            }
            
            // Procesa la posición del barco
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
 
    // Inicia el juego
    session->current_turn = 0;
    session->game_over = false;
    
    // Bucle principal del juego
    while (!session->game_over) {
        Player *current = &session->players[session->current_turn];
        Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];
        
        // Maneja el turno actual
        char buffer[BUFFER_SIZE] = {0};
        send_message(current->socket, "YOUR_TURN", "Dispara con formato ROW,COL o QUIT|");
        send_message(opponent->socket, "WAIT_TURN", "Espera al oponente");
        
        // Recibe el disparo
        int bytes_received = recv(current->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_DISCONNECTED");
            log_game_end(log_file, opponent->name, current->name);
            session->game_over = true;
            break;
        }
        
        // Verifica si el jugador se rinde
        if (strncmp(buffer, "QUIT|", 5) == 0) {
            send_message(current->socket, "GAME_OVER", "YOU_QUIT");
            send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
            log_game_end(log_file, opponent->name, current->name);
            session->game_over = true;
            break;
        }
        
        // Procesa el disparo
        int row, col;
        sscanf(buffer, "%d,%d", &row, &col);
        if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
            send_message(current->socket, "ERROR", "POSICION_INVALIDA");
            continue;
        }
        
        // Verifica el resultado del disparo
        char result = opponent->board.board[row][col];
        if (result == SHIP) {
            opponent->board.board[row][col] = HIT;
            
            // Verifica si se hundió un barco
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
                // Marca todas las posiciones del barco hundido
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
                        for (int i = 0; i < ship->size; i++) {
                            int sr = ship->positions[i][0];
                            int sc = ship->positions[i][1];
                            opponent->board.board[sr][sc] = SUNK;
                        }
                        break;
                    }
                }
                
                send_message(current->socket, "RESULT", "SUNK");
                log_move(log_file, current->name, row, col, "SUNK");
                
                // Verifica si el juego ha terminado
                bool all_ships_sunk = true;
                for (int s = 0; s < opponent->board.ship_count; s++) {
                    Ship *ship = &opponent->board.ships[s];
                    bool ship_sunk = true;
                    for (int i = 0; i < ship->size; i++) {
                        int sr = ship->positions[i][0];
                        int sc = ship->positions[i][1];
                        if (opponent->board.board[sr][sc] != HIT) {
                            ship_sunk = false;
                            break;
                        }
                    }
                    if (!ship_sunk) {
                        all_ships_sunk = false;
                        break;
                    }
                }
                
                if (all_ships_sunk) {
                    send_message(current->socket, "GAME_OVER", "YOU_WIN");
                    send_message(opponent->socket, "GAME_OVER", "YOU_LOSE");
                    log_game_end(log_file, current->name, opponent->name);
                    session->game_over = true;
                    break;
                }
            } else {
                send_message(current->socket, "RESULT", "HIT");
                log_move(log_file, current->name, row, col, "HIT");
            }
            // Mantiene el turno si fue un HIT
        } else {
            opponent->board.board[row][col] = WATER;
            send_message(current->socket, "RESULT", "MISS");
            log_move(log_file, current->name, row, col, "MISS");
            // Cambia el turno si fue un MISS
            session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
        }
        
        // Registra el estado del tablero después del movimiento
        log_board_state(log_file, &opponent->board, opponent->name);
    }
    
    // Limpia los recursos
    fclose(log_file);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(session->players[i].socket);
    }
    session->active = false;
    return NULL;
}

// Acepta nuevos jugadores y crea sesiones de juego
int accept_players(SessionManager *manager, int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int session_id = -1;
    
    // Busca una sesión disponible
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
    
    // Configura la nueva sesión
    GameSession *session = &manager->sessions[session_id];
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            if (i > 0) close(session->players[0].socket);
            session->active = false;
            return -1;
        }
        
        // Configura el jugador
        session->players[i].socket = new_socket;
        session->players[i].ready = false;
        
        // Maneja el login del jugador
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
    
    // Inicia el hilo de la sesión
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

// Función principal del servidor
int main() {
    // Configura el socket del servidor
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
    
    // Bucle principal del servidor
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