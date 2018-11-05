#ifndef TPE_PROTOS_POP3_PARSER_C_H
#define TPE_PROTOS_POP3_PARSER_C_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"

#define STATUS_SIZE         4
#define MAX_RESPONSE_SIZE   512
#define DESCRIPTION_SIZE    (MAX_RESPONSE_SIZE - STATUS_SIZE - 2)

enum response_state {
    response_status_indicator,
    response_description,
    response_newline,
    response_mail,
    response_list,
    response_capa,
    response_multiline,
    response_done,
    response_error,
};

struct response_parser {
    struct pop3_request  *request;
    enum response_state   state;

    uint8_t               i, j, count;

    char                  status_buffer[STATUS_SIZE];
    char                  description_buffer[DESCRIPTION_SIZE];     // unused

    bool                  first_line_done;
    struct parser         *pop3_multi_parser;

    char                  *capa_response;
    size_t                capa_size;
};


enum request_state {
    request_cmd,
    request_param,
    request_newline,
    request_done,
    request_error,
    request_error_cmd_too_long,
    request_error_param_too_long,
};

#define CMD_SIZE    4
#define PARAM_SIZE  (40 * 2)       // cada argumento puede tener 40 bytes y como mucho puede haber 2 argumentos por comando

struct request_parser {
    struct pop3_request *request;
    enum request_state  state;

    uint8_t             i, j;

    char                cmd_buffer[CMD_SIZE];
    char                param_buffer[PARAM_SIZE];
};


enum pop3_cmd_id {

    error = -1,

    /* valid in the AUTHORIZATION state */
    user, pass,
    apop,

    /* valid in the TRANSACTION state */
    stat, list, retr, dele, noop, rset,
    top, uidl,

    /* other */
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

// para el parser de un response

void response_parser_init(struct response_parser *p);

enum response_state response_parser_feed(struct response_parser *p, uint8_t c);

enum response_state response_consume(buffer *b, buffer *wb, struct response_parser *p, bool *errored);

bool response_is_done(enum response_state st, bool *errored);

void response_parser_close(struct response_parser *p);


// para el parser de un request

void request_parser_init(struct request_parser *p);

enum request_state request_parser_feed(struct request_parser *p, const uint8_t c);

enum request_state request_consume(buffer *b, struct request_parser *p, bool *errored);

bool request_is_done(enum request_state st, bool *errored);

void request_parser_close(struct request_parser *p);

void set_pipelining(struct selector_key *key, struct response_st *d);

#endif
