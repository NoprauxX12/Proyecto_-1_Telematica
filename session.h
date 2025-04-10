
#ifndef SESSION_H
#define SESSION_H

#include "shared.h"
#include <stdio.h>

extern FILE *global_log_file;
SessionManager* session_manager_create(int max_sessions);
int accept_players(SessionManager *manager, int server_fd);
void* handle_game_session(void *arg);

#endif
