#ifndef TPE_PROTOS_POP3_PARSER_C_H
#define TPE_PROTOS_POP3_PARSER_C_H

#include <stdint.h>
#include <stdbool.h>

enum pop3_cmd_id {

    error = -1,

    user, pass, apop,

    stat, list, retr, dele, noop, rset, top, uidl,

    quit, capa,
};

struct pop3_request_cmd {
    const enum pop3_cmd_id  id;
    const char              *name;
};

struct pop3_request {
    const struct pop3_request_cmd   *cmd;
    char                            *args;

    const struct pop3_response      *response;
};


const struct pop3_request_cmd * get_cmd(const char *cmd);

struct pop3_request * new_request(const struct pop3_request_cmd * cmd, char * args);

void destroy_request(struct pop3_request *r);

#endif
