#ifndef PROTOS_TPE_POP3_H
#define PROTOS_TPE_POP3_H

#include <netdb.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"

// /** usado por HELLO */
// struct hello_st {
//     /** buffer utilizado para I/O */
//     buffer * wb;
// };

// /** usado por REQUEST */
// struct request_st {
//     /** buffer utilizado para I/O */
//     buffer                     *rb, *wb;

//     * parser 
//     struct pop3_request         request;
//     struct request_parser       request_parser;

// };

// /** usado por RESPONSE */
// struct response_st {
//     buffer                      *rb, *wb;

//     struct pop3_request         *request;
//     struct response_parser      response_parser;
// };

// /** usado por EXTERNAL_TRANSFORMATION */
// enum et_status {
//     et_status_ok,
//     et_status_err,
//     et_status_done,
// };

// struct external_transformation {
//     enum et_status              status;

//     buffer                      *rb, *wb;
//     buffer                      *ext_rb, *ext_wb;

//     int                         *client_fd, *origin_fd;
//     int                         *ext_read_fd, *ext_write_fd;

//     struct parser               *parser_read;
//     struct parser               *parser_write;

//     bool                        finish_wr;
//     bool                        finish_rd;

//     bool                        error_wr;
//     bool                        error_rd;

//     bool                        did_write;
//     bool                        write_error;

//     size_t                      send_bytes_write;
//     size_t                      send_bytes_read;
// };

// estados de la sesion
enum pop3_session_state {
    AUTH,
    TRANSACTION,
    UPDATE,
    DONE,
};

// sesion de pop3
struct pop3_session {
    // long maxima 40 bytes
    char *user;
    char *password;

    enum pop3_session_state state;
    unsigned concurrent_invalid_commands;

    bool pipelining;

    struct queue * request_queue;
};


typedef struct pop3_data {

    // información del cliente
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    // resolución de la dirección del origin server
    struct addrinfo              *origin_resolution;

    // intento actual de la dirección del origin server
    struct addrinfo              *origin_resolution_current;

    // información del origin server 
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;

    int                           extern_read_fd;
    int                           extern_write_fd;

    struct pop3_session           session;

    /** maquinas de estados */
    struct state_machine          stm;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct pop3 *next;
}pop3_data;


void pop3_session_init(struct pop3_session *s, bool pipelining);


void pop3_passive_accept(struct selector_key *key);

void pop3_pool_destroy(void);

void pop3_destroy(pop3_data *s);


#endif
