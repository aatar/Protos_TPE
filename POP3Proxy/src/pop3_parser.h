#ifndef TPE_PROTOS_POP3_PARSER_C_H
#define TPE_PROTOS_POP3_PARSER_C_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "request.h"


///////////////////////////////////////////////////////////////////////////
// PARSE
///////////////////////////////////////////////////////////////////////////

struct parser_event {
    /** tipo de evento */
    unsigned type;
    /** caracteres asociados al evento */
    uint8_t  data[3];
    /** cantidad de datos en el buffer `data' */
    uint8_t  n;

    /** lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct parser_event *next;
};

/** describe una transición entre estados  */
struct parser_state_transition {
    /* condición: un caracter o una clase de caracter. Por ej: '\r' */
    int       when;
    /** descriptor del estado destino cuando se cumple la condición */
    unsigned  dest;
    /** acción 1 que se ejecuta cuando la condición es verdadera. requerida. */
    void    (*act1)(struct parser_event *ret, const uint8_t c);
    /** otra acción opcional */
    void    (*act2)(struct parser_event *ret, const uint8_t c);
};

/** predicado para utilizar en `when' que retorna siempre true */
#define ANY (1 << 9)

/** declaración completa de una máquina de estados */
struct parser_definition {
    /** cantidad de estados */
    const unsigned                         states_count;
    /** por cada estado, sus transiciones */
    const struct parser_state_transition **states;
    /** cantidad de estados por transición */
    const size_t                          *states_n;

    /** estado inicial */
    const unsigned                         start_state;
};

struct parser * parser_init    (const unsigned *classes, const struct parser_definition *def);

void parser_destroy  (struct parser *p);

void parser_reset    (struct parser *p);

const struct parser_event * parser_feed     (struct parser *p, const uint8_t c);


///////////////////////////////////////////////////////////////////////////
// POP3_PIPELINE
///////////////////////////////////////////////////////////////////////////


/**
 * pop3_multi.c - parsea respuesta multilinea POP3.
 */
enum pop3_multi_type {
    /** N bytes nuevos*/
    POP3_MULTI_BYTE,
    /** hay que esperar, no podemos decidir */
    POP3_MULTI_WAIT,
    /** la respuesta está completa */
    POP3_MULTI_FIN,
};

/** la definición del parser */
const struct parser_definition * pop3_multi_parser(void);

const char *
pop3_multi_event(enum pop3_multi_type type);



///////////////////////////////////////////////////////////////////////////
// RESPONSE
///////////////////////////////////////////////////////////////////////////


enum pop3_response_status {
    response_status_invalid = -1,

    response_status_ok,
    response_status_err,
};

struct pop3_response {
    const enum pop3_response_status status;
    const char                      *name;
};

const struct pop3_response *
get_response(const char *response);


///////////////////////////////////////////////////////////////////////////
// REQUEST
///////////////////////////////////////////////////////////////////////////


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


///////////////////////////////////////////////////////////////////////////
// REQUEST PARS
///////////////////////////////////////////////////////////////////////////

enum request_state {
    request_cmd,
    request_param,
    request_newline,
    request_done,
    req_error,
    req_error_long_cmd,
    req_error_long_param,
};

#define CMD_SIZE    4
#define PARAM_SIZE  (40 * 2)       // cada argumento puede tener 40 bytes y la maxima cantidad de argumentos de un comando es 2

struct request_parser {
    struct pop3_request *request;
    enum request_state  state;

    uint8_t             i, j;

    char                cmd_buffer[CMD_SIZE];
    char                param_buffer[PARAM_SIZE];
};

void request_parser_init (struct request_parser *p);

enum request_state request_parser_feed (struct request_parser *p, const uint8_t c);

enum request_state request_consume(buffer *b, struct request_parser *p, bool *errored);

bool  request_is_done(enum request_state st, bool *errored);

void request_parser_close(struct request_parser *p);

extern int request_marshall(struct pop3_request *r, buffer *b);


///////////////////////////////////////////////////////////////////////////
// REQUEST PARS
///////////////////////////////////////////////////////////////////////////

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

/** inicializa el parser */
void
response_parser_init (struct response_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum response_state response_parser_feed (struct response_parser *p, uint8_t c);

enum response_state response_consume(buffer *b, buffer *wb, struct response_parser *p, bool *errored);

bool response_is_done(enum response_state st, bool *errored);

void response_parser_close(struct response_parser *p);


#endif
