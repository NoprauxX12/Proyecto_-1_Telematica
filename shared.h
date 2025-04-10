
#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <pthread.h>

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

typedef struct {
    char name[20];
    int size;
} ShipType;

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

extern ShipType SHIPS[MAX_SHIPS];

#endif
