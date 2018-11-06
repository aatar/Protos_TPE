/**
 * parser del POP3
 */

#include <string.h> 
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "pop3_parser.h"



///////////////////////////////////////////////////////////////////////////
// PARSE
///////////////////////////////////////////////////////////////////////////

struct parser {
    /** tipificación para cada caracter */
    const unsigned     *classes;
    /** definición de estados */
    const struct parser_definition *def;

    /* estado actual */
    unsigned            state;

    /* evento que se retorna */
    struct parser_event e1;
    /* evento que se retorna */
    struct parser_event e2;
};

void
parser_destroy(struct parser *p) {
    if(p != NULL) {
        free(p);
    }
}

struct parser *
parser_init(const unsigned *classes,
            const struct parser_definition *def) {
    struct parser *ret = malloc(sizeof(*ret));
    if(ret != NULL) {
        memset(ret, 0, sizeof(*ret));
        ret->classes = classes;
        ret->def     = def;
        ret->state   = def->start_state;
    }
    return ret;
}

void
parser_reset(struct parser *p) {
    p->state   = p->def->start_state;
}

const struct parser_event *
parser_feed(struct parser *p, const uint8_t c) {
    const unsigned type = p->classes[c];

    p->e1.next = p->e2.next = 0;

    const struct parser_state_transition *state = p->def->states[p->state];
    const size_t n                              = p->def->states_n[p->state];
    bool matched   = false;

    for(unsigned i = 0; i < n ; i++) {
        const int when = state[i].when;
        if (state[i].when <= 0xFF) {
            matched = (c == when);
        } else if(state[i].when == ANY) {
            matched = true;
        } else if(state[i].when > 0xFF) {
            matched = (type & when);
        } else {
            matched = false;
        }

        if(matched) {
            state[i].act1(&p->e1, c);
            if(state[i].act2 != NULL) {
                p->e1.next = &p->e2;
                state[i].act2(&p->e2, c);
            }
            p->state = state[i].dest;
            break;
        }
    }
    return &p->e1;
}


static const unsigned classes[0xFF] = {0x00};

const unsigned *
parser_no_classes(void) {
    return classes;
}


///////////////////////////////////////////////////////////////////////////
// POP3_PIPELINE
///////////////////////////////////////////////////////////////////////////



const char *
pop3_multi_event(enum pop3_multi_type type) {
    const char *ret = NULL;
    switch(type) {
        case POP3_MULTI_BYTE:
            ret = "byte(c)";
            break;
        case POP3_MULTI_WAIT:
            ret = "wait()";
            break;
        case POP3_MULTI_FIN:
            ret = "fin()";
            break;
    }
    return ret;
}

enum {
    NEWLINE,
    BYTE,
    CR,
    DOT,
    DOT_CR,
    FIN,
};

static void
byte(struct parser_event *ret, const uint8_t c) {
    ret->type    = POP3_MULTI_BYTE;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
byte_cr(struct parser_event *ret, const uint8_t c) {
    byte(ret, '\r');
}

static void
wait(struct parser_event *ret, const uint8_t c) {
    ret->type    = POP3_MULTI_WAIT;
    ret->n       = 0;
}

static void
fin(struct parser_event *ret, const uint8_t c) {
    ret->type    = POP3_MULTI_FIN;
    ret->n       = 0;
}

static const struct parser_state_transition ST_NEWLINE[] =  {
    {.when = '.',        .dest = DOT,         .act1 = wait,},
    {.when = '\r',       .dest = CR,          .act1 = wait,},
    {.when = ANY,        .dest = BYTE,        .act1 = byte,},
};

static const struct parser_state_transition ST_BYTE[] =  {
    {.when = '\r',       .dest = CR,          .act1 = wait,},
    {.when = ANY,        .dest = BYTE,        .act1 = byte,},
};

static const struct parser_state_transition ST_CR[] =  {
    {.when = '\n',       .dest = NEWLINE,     .act1 = byte_cr, .act2 = byte},
    {.when = ANY,        .dest = BYTE,        .act1 = byte_cr, .act2 = byte},
};

static const struct parser_state_transition ST_DOT[] =  {
    {.when = '\r',       .dest = DOT_CR,      .act1 = wait},
    {.when = ANY,        .dest = BYTE,        .act1 = byte},
};

static const struct parser_state_transition ST_DOT_CR[] =  {
    {.when = '\n',       .dest = FIN,         .act1 = fin},
    {.when = ANY,        .dest = BYTE,        .act1 = byte_cr, .act2 = byte},
};

static const struct parser_state_transition ST_FIN[] =  {
    {.when = ANY,        .dest = BYTE,        .act1 = byte_cr, .act2 = byte},
};

///////////////////////////////////////////////////////////////////////////////
// Declaración formal

static const struct parser_state_transition *states [] = {
    ST_NEWLINE,
    ST_BYTE,
    ST_CR,
    ST_DOT,
    ST_DOT_CR,
    ST_FIN,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t states_n [] = {
    N(ST_NEWLINE),
    N(ST_BYTE),
    N(ST_CR),
    N(ST_DOT),
    N(ST_DOT_CR),
    N(ST_FIN),
};

static struct parser_definition definition = {
    .states_count = N(states),
    .states       = states,
    .states_n     = states_n,
    .start_state  = NEWLINE,
};

const struct parser_definition *
pop3_multi_parser(void) {
    return &definition;
}



///////////////////////////////////////////////////////////////////////////
// RESPONSE
///////////////////////////////////////////////////////////////////////////

#define RESP_SIZE (response_status_err + 1)

static const struct pop3_response responses[RESP_SIZE] = {
        {
                .status = response_status_ok,
                .name   = "+OK",
        },
        {
                .status = response_status_err,
                .name   = "-ERR",
        },
};

static const struct pop3_response invalid_response = {
        .status = response_status_invalid,
        .name   = NULL,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

const struct pop3_response *
get_response(const char *response) {
    for (unsigned i = 0; i < N(responses); i++) {
        if (strcmp(response, responses[i].name) == 0) {
            return &responses[i];
        }
    }

    return &invalid_response;
}


///////////////////////////////////////////////////////////////////////////
// REQUEST
///////////////////////////////////////////////////////////////////////////


const struct pop3_request_cmd commands[CMD_SIZE] = {
        {
                .id     = user,
                .name   = "user",
        },
        {
                .id     = pass,
                .name   = "pass",
        },
        {
                .id     = apop,
                .name   = "apop",
        },
        {
                .id     = stat,
                .name   = "stat",
        },
        {
                .id     = list,
                .name   = "list",
        },
        {
                .id     = retr,
                .name   = "retr",
        },
        {
                .id     = dele,
                .name   = "dele",
        },
        {
                .id     = noop,
                .name   = "noop",
        },
        {
                .id     = rset,
                .name   = "rset",
        },
        {
                .id     = top,
                .name   = "top",
        },
        {
                .id     = uidl,
                .name   = "uidl",
        },
        {
                .id     = quit,
                .name   = "quit",
        },
        {
                .id     = capa,
                .name   = "capa",
        },
};

const struct pop3_request_cmd invalid_cmd = {
        .id     = error,
        .name   = NULL,
};


/**
 * Comparacion case insensitive de dos strings
 */
static bool strcmpi(const char * str1, const char * str2) {
    int c1, c2;
    while (*str1 && *str2) {
        c1 = tolower(*str1++);
        c2 = tolower(*str2++);
        if (c1 != c2) {
            return false;
        }
    }

    return *str1 == 0 && *str2 == 0 ? true : false;
}

#define N(x) (sizeof(x)/sizeof((x)[0]))

const struct pop3_request_cmd * get_cmd(const char *cmd) {

    for (unsigned i = 0; i < N(commands); i++) {
        if (strcmpi(cmd, commands[i].name)) {
            return &commands[i];
        }
    }

    return &invalid_cmd;
}

struct pop3_request * new_request(const struct pop3_request_cmd * cmd, char * args) {
    struct pop3_request *r = malloc(sizeof(*r));

    if (r == NULL) {
        return NULL;
    }

    r->cmd      = cmd;
    r->args     = args; // args ya fue alocado en el parser. se podria alocar aca tambien
    // la response no se aloca porque son genericas

    return r;
}

//TODO usar en algun lado!
void destroy_request(struct pop3_request *r) {
    free(r->args);
    free(r);
}


///////////////////////////////////////////////////////////////////////////
// REQUEST PARS
///////////////////////////////////////////////////////////////////////////

static enum request_state
cmd(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_cmd;
    struct pop3_request *r = p->request;

    if (c == ' ' || c == '\r' || c == '\n') {   // aceptamos comandos terminados en '\r\n' y '\n'
        r->cmd = get_cmd(p->cmd_buffer);
        if (r->cmd->id == error) {
            ret = req_error;
        } else if (c == ' ') {
            ret = request_param;
        } else if (c == '\r'){
            ret = request_newline;
        } else {
            ret = request_done;
        }
    } else if (p->i >= CMD_SIZE) {
        r->cmd = get_cmd(""); // seteo cmd en error
        ret = req_error_long_cmd;
    } else {
        p->cmd_buffer[p->i++] = c;
    }

    return ret;
}

static enum request_state
param(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_param;
    struct pop3_request *r = p->request;

    if (c == '\r' || c == '\n') {
        char * aux = p->param_buffer;
        int count = 0;
        while (*aux  != 0) {
            if (*aux == ' ' || *aux == '\t')
                count++;
            aux++;
        }

        if (count != p->j) {
            r->args = malloc(strlen(p->param_buffer) + 1);
            strcpy(r->args, p->param_buffer);
        }

        if (c == '\r') {
            ret = request_newline;
        } else {
            ret = request_done;
        }

    } else {
        p->param_buffer[p->j++] = c;
        if (p->j >= PARAM_SIZE) {
            ret = req_error_long_param;
        }
    }

    return ret;
}

static enum request_state
req_newline(const uint8_t c, struct request_parser* p) {

    if (c != '\n') {
        return req_error;
    }

    return request_done;
}

extern void
request_parser_init (struct request_parser* p) {
    memset(p->request, 0, sizeof(*(p->request)));
    memset(p->cmd_buffer, 0, CMD_SIZE);
    memset(p->param_buffer, 0, PARAM_SIZE);
    p->state = request_cmd;
    p->i = p->j = 0;
}

extern enum request_state 
request_parser_feed (struct request_parser* p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_cmd:
            next = cmd(c, p);
            break;
        case request_param:
            next = param(c, p);
            break;
        case request_newline:
            next = req_newline(c, p);
            break;
        case request_done:
        case req_error:
        case req_error_long_cmd:
        case req_error_long_param:
            next = p->state;
            break;
        default:
            next = req_error;
            break;
    }

    return p->state = next;
}

extern bool 
request_is_done(const enum request_state st, bool *errored) {
    if(st >= req_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;
    uint8_t c = 0;

    while(buffer_can_read(b)) {
        c = buffer_read(b);
        st = request_parser_feed(p, c);
        if(request_is_done(st, errored)) {
            break;
        }
    }

    //limpio el buffer luego de un comando invalido
    if (st >= req_error && c != '\n') {
        while(buffer_can_read(b) && c != '\n') {
            c = buffer_read(b);
        }

        if (c != '\n') {
            st = request_cmd;
        }
    }

    return st;
}

extern void
request_parser_close(struct request_parser *p) {
    // nada que hacer
}

extern int
request_marshall(struct pop3_request *r, buffer *b) {
    size_t  n;
    uint8_t *buff;

    const char * cmd = r->cmd->name;
    char * args = r->args;

    size_t i = strlen(cmd);
    size_t j = args == NULL ? 0 : strlen(args);
    size_t count = i + j + (j == 0 ? 2 : 3);

    buff = buffer_write_ptr(b, &n);

    if(n < count) {
        return -1;
    }

    memcpy(buff, cmd, i);
    buffer_write_adv(b, i);


    if (args != NULL) {
        buffer_write(b, ' ');
        buff = buffer_write_ptr(b, &n);
        memcpy(buff, args, j);
        buffer_write_adv(b, j);
    }

    buffer_write(b, '\r');
    buffer_write(b, '\n');

    return (int)count;
}



///////////////////////////////////////////////////////////////////////////
// RESPONSE PARS
///////////////////////////////////////////////////////////////////////////


enum response_state
status(const uint8_t c, struct response_parser* p) {
    enum response_state ret = response_status_indicator;
    if (c == ' ' || c == '\r') {
        p->request->response = get_response(p->status_buffer);
        if (c == ' ') {
            ret = response_description;
        } else {
            ret = response_newline;
        }
    } else if (p->i >= STATUS_SIZE) {
        ret = response_error;
    } else {
        p->status_buffer[p->i++] = c;
    }

    return ret;
}

//ignoramos la descripcion
enum response_state
description(const uint8_t c, struct response_parser* p) {
    enum response_state ret = response_description;

    if (c == '\r') {
        ret = response_newline;
    }

    return ret;
}

enum response_state
resp_newline(const uint8_t c, struct response_parser *p) {
    enum response_state ret = response_done;

    if (p->request->response->status != response_status_err) {
        switch (p->request->cmd->id) {
            case retr:
                ret = response_mail;
                break;
            case list:
                if (p->request->args == NULL) {
                    ret = response_list;
                }
                break;
            case capa:
                ret = response_capa;
                break;
            case uidl:
            case top:
                ret = response_multiline;
                break;
            default:
                break;
        }
    }

    if (c == '\n') {
        p->first_line_done = true;
    }

    return c != '\n' ? response_error : ret;
}

enum response_state
mail(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_mail;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

enum response_state
plist(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_list;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

#define BLOCK_SIZE  20

enum response_state
pcapa(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_capa;

    // save capabilities to struct
    if (p->j == p->capa_size) {
        void * tmp = realloc(p->capa_response, p->capa_size + BLOCK_SIZE);
        if (tmp == NULL)
            return response_error;
        p->capa_size += BLOCK_SIZE;
        p->capa_response = tmp;
    }

    p->capa_response[p->j++] = c;

    switch (e->type) {
        case POP3_MULTI_FIN:
            if (p->j == p->capa_size) {
                void * tmp = realloc(p->capa_response, p->capa_size + 1);
                if (tmp == NULL)
                    return response_error;
                p->capa_size++;
                p->capa_response = tmp;
            }
            p->capa_response[p->j] = 0;
            ret = response_done;
            break;
    }

    return ret;
}

enum response_state
multiline(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_multiline;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

extern void
response_parser_init (struct response_parser* p) {
    memset(p->status_buffer, 0, STATUS_SIZE);
    p->state = response_status_indicator;
    p->first_line_done = false;
    p->i = 0;

    if (p->pop3_multi_parser == NULL) {
        p->pop3_multi_parser = parser_init(parser_no_classes(), pop3_multi_parser());
    }

    parser_reset(p->pop3_multi_parser);

    if (p->capa_response != NULL) {
        free(p->capa_response);
        p->capa_response = NULL;
    }

    p->j = 0;
    p->capa_size = 0;
}

extern enum response_state
response_parser_feed (struct response_parser* p, const uint8_t c) {
    enum response_state next;

    switch(p->state) {
        case response_status_indicator:
            next = status(c, p);
            break;
        case response_description:
            next = description(c, p);
            break;
        case response_newline:
            next = resp_newline(c, p);
            break;
        case response_mail:
            next = mail(c, p);
            break;
        case response_list:
            next = plist(c, p);
            break;
        case response_capa:
            next = pcapa(c, p);
            break;
        case response_multiline:
            next = multiline(c, p);
            break;
        case response_done:
        case response_error:
            next = p->state;
            break;
        default:
            next = response_error;
            break;
    }

    return p->state = next;
}

extern bool
response_is_done(const enum response_state st, bool *errored) {
    if(st >= response_error && errored != 0) {
        *errored = true;
    }
    return st >= response_done;
}

extern enum response_state
response_consume(buffer *b, buffer *wb, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;
    if (p->state == response_done)
        return st;
    while(buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);
        st = response_parser_feed(p, c);
        buffer_write(wb, c);
        if(response_is_done(st, errored) || p->first_line_done) {   // si se termino la respuesta o se termino de leer la primera linea
            break;
        }
    }
    return st;
}

extern void
response_parser_close(struct response_parser *p) {
    // nada que hacer
}


extern enum response_state response_consume_first_line(buffer *rb, buffer *Wb, struct response_parser *p, bool *errored) {
    enum response_state st = response_consume(rb, Wb, p, errored);
    p->first_line_done = false;
    st = response_consume(rb, Wb, p, errored);
    return st;
}


// char * to_upper(char *str) {
//     char * aux = str;
//     while (*aux != 0) {
//         *aux = (char)toupper(*aux);
//         aux++;
//     }

//     return str;
// }

// void set_pipelining(struct selector_key *key, struct response_st *d) {
//     to_upper(d->response_parser.capa_response);
//     char * capabilities = d->response_parser.capa_response;
//     char * needle = "PIPELINING";

//     struct pop3_data *p = ATTACHMENT(key);

//     if (strstr(capabilities, needle) != NULL) {
//         p->session.pipelining = true;
//     } else {
//         p->session.pipelining = false;
//     }

//     while (buffer_can_read(d->wb)) {
//         buffer_read(d->wb);
//     }

// }
