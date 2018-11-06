#ifndef ADMIN_H
#define ADMIN_H

#include "psmp_packager.h"
#include "../POP3Proxy/src/selector.h"
#include "../POP3Proxy/src/buffer.h"

typedef struct authentication authentication;
typedef struct auth_request auth_request;

void
start_interpreter();

void
psmp_accept_connection(struct selector_key *key);

static struct admin_data *
admin_init(int client_fd);

#endif