/**
 * parser del POP3
 */

#include <string.h> 
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "pop3_parser.h"
#include "buffer.h"


#define CMD_SIZE    (capa + 1)

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


// comparacion case insensitive
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

const struct pop3_request_cmd * get_command(const char *cmd) {

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
    r->args     = args;

    return r;
}



// Parser

extern void response_parser_init (struct response_parser* p) {
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



extern bool response_is_done(const enum response_state st, bool *errored) {
    if(st >= response_error && errored != 0) {
        *errored = true;
    }
    return st >= response_done;
}


extern enum response_state response_parser_feed (struct response_parser* p, const uint8_t c) {
    enum response_state next;

    switch(p->state) {
        case response_status_indicator:
            next = status(c, p);
            break;
        case response_description:
            next = description(c, p);
            break;
        case response_newline:
            next = newline(c, p);
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

extern enum response_state response_consume(buffer *rb, buffer *Wb, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;
    if (p->state == response_done)
        return st;
    while(buffer_can_read(rb)) {
        const uint8_t c = buffer_read(rb);
        st = response_parser_feed(p, c);
        buffer_write(wb, c);
        if(response_is_done(st, errored) || p->first_line_done) {   // si se termino la respuesta o se termino de leer la primera linea
            break;
        }
    }
    return st;
}

extern enum response_state response_consume_first_line(buffer *rb, buffer *Wb, struct response_parser *p, bool *errored) {
    enum response_state st = response_consume(rb, Wb, p, errored);
    p->.first_line_done = false;
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


extern void response_parser_close(struct response_parser *p) {
 
}