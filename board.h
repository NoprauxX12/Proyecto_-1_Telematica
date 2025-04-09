
#ifndef BOARD_H
#define BOARD_H

#include "shared.h"

void init_board(Board *board);
bool place_ship(Board *board, const char *ship_name, int size, int row, int col, char orientation);

#endif
