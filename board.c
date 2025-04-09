
#include "board.h"
#include <string.h>

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
