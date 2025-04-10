// session.c

#include "session.h"
#include "board.h"
#include "logger.h"
#include "messaging.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>



ShipType SHIPS[MAX_SHIPS] = {
    {"Portaavion", 5}, {"Buque de Guerra", 4}, {"Crucero1", 3},
    {"Crucero2", 3}, {"Destructor1", 2}, {"Destructor2", 2},
    {"Submarino1", 1}, {"Submarino2", 1}, {"Submarino3", 1}
};

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

void advance_turn(GameSession *session) {
    session->current_turn = (session->current_turn + 1) % MAX_PLAYERS;
}

void handle_turn_change(GameSession *session) {
    advance_turn(session);
    Player *next = &session->players[session->current_turn];
    Player *waiting = &session->players[(session->current_turn + 1) % MAX_PLAYERS];
    send_message(next->socket, "YOUR_TURN", "Tu turno ha comenzado");
    send_message(waiting->socket, "WAIT_TURN", "Espera tu turno");
}

void* handle_game_session(void *arg) {
    GameSession *session = (GameSession *)arg;
    FILE *log_file = global_log_file ? global_log_file : fopen(LOG_FILE, "a");
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

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(session->players[i].socket, &read_fds);
            struct timeval timeout = { .tv_sec = 30, .tv_usec = 0 };

            int activity = select(session->players[i].socket + 1, &read_fds, NULL, NULL, &timeout);
            if (activity <= 0 || recv(session->players[i].socket, placement_msg, BUFFER_SIZE, MSG_PEEK) <= 0) {
                int other = (i == 0) ? 1 : 0;
                send_message(session->players[other].socket, "GAME_OVER", "OPPONENT_DISCONNECTED");
                send_message(session->players[other].socket, "VICTORY", "WINNING_PLAYER");
                log_game_end(log_file, session->players[other].name, session->players[i].name);
                session->game_over = true;
                return NULL;
            }

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
        log_board_state(log_file, &session->players[i].board, session->players[i].name);
    }

    session->current_turn = 0;
    session->game_over = false;
    Player *current = &session->players[session->current_turn];
    Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];
    send_message(current->socket, "YOUR_TURN", "Tienes 30 segundos para disparar");
    send_message(opponent->socket, "WAIT_TURN", "Espera tu turno");

    while (!session->game_over) {
        Player *current = &session->players[session->current_turn];
        Player *opponent = &session->players[(session->current_turn + 1) % MAX_PLAYERS];

        time_t start_time = time(NULL);
        fd_set read_fds;
        struct timeval timeout;

        while (!session->game_over && difftime(time(NULL), start_time) < 30) {
            FD_ZERO(&read_fds);
            FD_SET(current->socket, &read_fds);

            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int activity = select(current->socket + 1, &read_fds, NULL, NULL, &timeout);
            if (activity > 0 && FD_ISSET(current->socket, &read_fds)) {
                char buffer[BUFFER_SIZE] = {0};
                int bytes = recv(current->socket, buffer, BUFFER_SIZE, 0);

                if (bytes <= 0) {
                    send_message(opponent->socket, "GAME_OVER", "OPPONENT_DISCONNECTED");
                    send_message(opponent->socket, "VICTORY", "WINNING_PLAYER");
                    log_game_end(log_file, opponent->name, current->name);
                    session->game_over = true;
                    break;
                }

                if (strncmp(buffer, "QUIT|", 5) == 0) {
                    send_message(current->socket, "GAME_OVER", "YOU_QUIT");
                    send_message(opponent->socket, "GAME_OVER", "OPPONENT_QUIT");
                    log_game_end(log_file, opponent->name, current->name);
                    session->game_over = true;
                    break;
                }

                int row, col;
                if (sscanf(buffer, "%d,%d", &row, &col) != 2 || row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
                    send_message(current->socket, "ERROR", "POSICION_INVALIDA");
                    continue;
                }

                char cell = opponent->board.board[row][col];
                fprintf(log_file, "%s ataca (%d,%d): ", current->name, row, col);

                if (cell == HIT || cell == WATER || cell == SUNK) {
                    send_message(current->socket, "ERROR", "YA_DISPARASTE_AQUI");
                    fprintf(log_file, "fallo (ya atacado)\n");
                    continue;
                }

                if (cell == SHIP) {
                    opponent->board.board[row][col] = HIT;
                    send_message(opponent->socket, "ENEMY_HIT", buffer);
                    send_message(current->socket, "RESULT", "HIT");
                    fprintf(log_file, "¡acertó!\n");

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
                    } else {
                        handle_turn_change(session);
                        break;
                    }
                } else {
                    opponent->board.board[row][col] = WATER;
                    send_message(current->socket, "RESULT", "MISS");
                    fprintf(log_file, "fallo.\n");
                    handle_turn_change(session);
                    break;
                }
            }
        }

        if (!session->game_over && difftime(time(NULL), start_time) >= 30) {
            send_message(current->socket, "TURN_END", "TIME_OUT");
            handle_turn_change(session);
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(session->players[i].socket);
    }
    session->active = false;
    fclose(log_file);
    return NULL;
}
