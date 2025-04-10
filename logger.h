
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include "shared.h"

void log_game_start(FILE *log_file, const char *p1, const char *p2);
void log_game_end(FILE *log_file, const char *ganador, const char *perdedor);
void log_board_state(FILE *log_file, Board *board, const char *player_name);

#endif
