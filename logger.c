
#include "logger.h"
#include <time.h>

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
