/**
 * pop3.c  - fucniones para el control de un proxy POP3 con sockets no bloqueantes
 */
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>  
#include <assert.h>  
#include <errno.h>
#include <unistd.h>  

#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <memory.h>

#include "buffer.h"
#include "stm.h"
#include "pop3.h"
#include "pop3_parser.h"
#include "input_data.h"
#include "selector.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


#define BUFFER_SIZE 2048

#define MAX_INVALID_CMDS 3

// estados de pop3 
enum pop3_state {
    /**
     *  Espera la resolución DNS del origin server 
     *
     *  Transiciones:
     *      - CONNECTING    una vez que se resuelve el nombre
     *      - ERROR         si fallo la resolucion
     *
     */
            RESOLVE_ORIGIN,

    /**
     *  Espera que se establezca la conexion con el origin server
     *
     *  Transiciones:
     *      - HELLO    una vez que se establecio la conexion
     *      - ERROR      si no se pudo conectar
     */
            REQUEST_CONNECTING,

    /**
     *  Lee el mensaje de bienvenida del origin server
     *
     *  Transiciones:
     *      - HELLO         mientras el mensaje no este completo
     *      - CAPA          cuando está completo
     *      - ERROR         ante cualquier error (IO/parseo)
     *
     */
            HELLO,

    /**
     *  Le pregunta las capacidades al origin server, nos interesa
     *  saber si el server soporta pipelining o no.
     *
     *      - REQUEST_CAPA          mientras la respuesta no este completa
     *      - REQUEST       cuando está completa
     *      - ERROR         ante cualquier error (IO/parseo)
     */
            REQUEST_CAPA,

    /**
     *  Lee requests del cliente y las manda al origin server
     *
     *  Transiciones:
     *      - REQUEST       mientras la request no este completa
     *      - RESPONSE      cuando la request esta completa
     *      - ERROR         ante cualquier error (IO/parseo)
     */
            REQUEST,
    /**
     *  Lee la respuesta del origin server y se la envia al cliente
     *
     *  Transiciones:
     *      - RESPONSE                  mientras la respuesta no este completa
     *      - EXTERNAL_TRANSFORMATION   si la request requiere realizar una transformacion externa
     *      - REQUEST                   cuando la respuesta esta completa
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            RESPONSE,

     /**
     *  Ejecuta una transformacion externa sobre un mail
     *      - EXTERNAL_TRANSFORMATION   mientras la transformacion externa no esta completa
     *      - REQUEST                   cuando esta completa
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            EXTERNAL_TRANSFORMATION,

    // estados terminales
            DONE,
            ERROR,
};

// estados de la sesion de pop3
enum pop3_session_state {
    SESSION_AUTH,
    SESSION_TRANSACTION,
    SESSION_UPDATE,
    SESSION_DONE,
};

// sesion de pop3
struct pop3_session {
    // long maxima 40 bytes
    char *user;
    char *password;

    enum pop3_session_state state;
    unsigned invalid_cmds;

    bool pipelining;

    struct queue * request_queue;
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado



/** usado por HELLO_READ, HELLO_WRITE */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer               *rb, *wb;
    // struct hello_parser   parser;

};


/** usado por REQUEST */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                     *rb, *wb;

    /** parser */
    struct pop3_request         request;
    struct request_parser       request_parser;
    struct response_parser      response_parser;

};

/** usado por RESPONSE */
struct response_st {
    buffer                      *rb, *wb;

    struct pop3_request         *request;
    struct response_parser      response_parser;
};


struct pop3_data {

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

    /** estados para el client_fd */
    union {
        struct request_st         request;
    } client;
    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
        struct request_st         response;
    } orig;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct pop3 *next;
};


//  pool de pop3_data
static const unsigned    max_pool  = 50;  // tamaño máximo
static unsigned          pool_size = 0;   // tamaño actual
static struct pop3_data * pool     = NULL;

static const struct state_definition * pop3_describe_states(void);

void pop3_session_init(struct pop3_session *s, bool pipeline);

static struct pop3_data * pop3_init(int client_fd) {
    struct pop3_data * pop3;

    if(pool == NULL) {
        pop3 = malloc(sizeof(*pop3));

        if (pop3 == NULL) {
            goto finally;
        }
    } else {
        pop3       = pool;
        pool       = pool->next;
        pop3->next = 0;
    }

    memset(pop3, 0x00, sizeof(*pop3));

    pop3->origin_fd       = -1;

    // configura la direccion del cliente (MUA)
    pop3->client_fd       = client_fd;
    pop3->client_addr_len = sizeof(pop3->client_addr);

    // configura el estado inicial de la maquina de estados
    pop3->stm    .initial   = RESOLVE_ORIGIN;
    pop3->stm    .max_state = ERROR;
    pop3->stm    .states    = pop3_describe_states();
    
    stm_init(&pop3->stm);

    buffer_init(&pop3->read_buffer,  N(pop3->raw_buff_a), pop3->raw_buff_a);
    buffer_init(&pop3->write_buffer, N(pop3->raw_buff_b), pop3->raw_buff_b);


    pop3_session_init(&pop3->session, false);

    pop3->references = 1;

finally:
    return pop3;
}


// void pop3_destroy(struct pop3_data *s);

static void pop3_destroy_(struct pop3_data *s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s);
}

/**
 * destruye un  `struct pop3_data', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void pop3_destroy(struct pop3_data *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < max_pool) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                pop3_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void pop3_pool_destroy(void) {
    struct pop3_data *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}



/** obtiene el struct (pop3 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3_data *)(key)->data)

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void pop3_read(struct selector_key *key);
static void pop3_write(struct selector_key *key);
static void pop3_block(struct selector_key *key);
static void pop3_close(struct selector_key *key);
static const struct fd_handler pop3_handler = {
        .handle_read   = pop3_read,
        .handle_write  = pop3_write,
        .handle_close  = pop3_close,
        .handle_block  = pop3_block,
};

void pop3_passive_accept(struct selector_key *key) {

    struct sockaddr_storage  mua_client_addr;
    socklen_t                mua_client_addr_len = sizeof(mua_client_addr);
    struct pop3_data                             *state = NULL;

    // en esta instancia llego por el select asique el accept va a retornar sin bloquear
    const int mua_client = accept(key->fd, (struct sockaddr*) &mua_client_addr,
                              &mua_client_addr_len);

    // met->concurrent_connections++;
    // met->historical_access++;

    if(mua_client == -1) {
        goto fail;
    } 

    // configura el socket para que sea no bloqueante
    if(selector_fd_set_nio(mua_client) == -1) {
        goto fail;
    }

    state = pop3_init(mua_client);

    if(state == NULL) {
        goto fail;
    }

    memcpy(&state->client_addr, &mua_client_addr, mua_client_addr_len);

    state->client_addr_len = mua_client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, mua_client, &pop3_handler,
                                             OP_WRITE, state)) {
        goto fail;
    }
    return ;

fail:
    if(mua_client != -1) {
        close(mua_client);
    }
    pop3_destroy(state);
}




/////////////////////////////////////////////////////////////////////////////
// RESOLVE_ORIGIN
/////////////////////////////////////////////////////////////////////////////

// static unsigned origin_resolv_blocking(struct selector_key *key);


static void * resolve_origin_blocking(void *data);

static unsigned origin_connect(struct selector_key *key);


unsigned resolve_origin_init(struct selector_key *key){
    unsigned ret;
    pthread_t tid;
    struct selector_key* k = malloc(sizeof(*key));

    if(k == NULL) {
        ret = ERROR;
    } else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0, resolve_origin_blocking, k)) {
            ret = ERROR;
        } else{
            ret = RESOLVE_ORIGIN;
            selector_set_interest_key(key, OP_NOOP);
        }
    }

    return ret;
}


/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void * resolve_origin_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    struct pop3_data       *s   = ATTACHMENT(key);

    pthread_detach(pthread_self());
    s->origin_resolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
        .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
        .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
        .ai_protocol  = 0,            /* Any protocol */
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", params->origin_port);

    if (0 != getaddrinfo(params->origin_server, buff, &hints,
                         &s->origin_resolution)){
        fprintf(stderr, "name resolution error\n");
    }

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

/** procesa el resultado de la resolución de nombres */
static unsigned resolve_origin_done(struct selector_key *key) {
    struct pop3_data *s      =  ATTACHMENT(key);

    if(s->origin_resolution == 0) {
        // char * msg = "-ERR Invalid domain.\r\n";
        // send(ATTACHMENT(key)->client_fd, msg, strlen(msg), 0);
        return ERROR;
    } else {
        s->origin_domain   = s->origin_resolution->ai_family;
        s->origin_addr_len = s->origin_resolution->ai_addrlen;
        memcpy(&s->origin_addr,
               s->origin_resolution->ai_addr,
               s->origin_resolution->ai_addrlen);
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }

    if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
        return ERROR;
    }

    return origin_connect(key);
}

/** intenta establecer una conexión con el origin server */
static unsigned origin_connect(struct selector_key *key) {
    bool error = false;

    int fd     = socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, IPPROTO_TCP);


    if (fd == -1) {
        error = true;
        goto finally;
    }
    if (selector_fd_set_nio(fd) == -1) {
        goto finally;
    }
    // Establece la coneccion con el servidor origin
    if (-1 == connect(fd, (const struct sockaddr *)&ATTACHMENT(key)->origin_addr,
                      ATTACHMENT(key)->origin_addr_len)) {
        if(errno == EINPROGRESS) {
            // es esperable,  tenemos que esperar a la conexión

            // dejamos de pollear el socket del cliente
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st) {
                goto finally;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, fd, &pop3_handler,
                                   OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            error = true;
            goto finally;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        // saltaríamos directamente a COPY
        abort();
    }

    return REQUEST_CONNECTING;


finally:
    if (error) {
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
    return ERROR;
}

/////////////////////////////////////////////////////////////////////////////
// CONNECTING
/////////////////////////////////////////////////////////////////////////////


void send_msg(int fd, const char * msg) {
    send(fd, msg, strlen(msg), 0);
}

unsigned connecting(struct selector_key *key) {
    int error;
    socklen_t len = sizeof(error);
    struct pop3_data *d = ATTACHMENT(key);

    d->origin_fd = key->fd;

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {

        fprintf(stderr, "Origin server connection failed\n");
        send_msg(d->client_fd, "-ERR Connection refused.\r\n");
        selector_set_interest_key(key, OP_NOOP);
        return ERROR;

    } else {
        if(error == 0) {
            // Todo bien
            d->origin_fd = key->fd;
        } else {
            send_msg(d->client_fd, "-ERR Connection refused.\r\n");
            fprintf(stderr, "Connection to origin server failed\n");
            selector_set_interest_key(key, OP_NOOP);
            return ERROR;
        }
    }

    // inicializa la sesion pop3 sin pipelining
    //chequear para mas legilibilidad
    pop3_session_init(&ATTACHMENT(key)->session, false);

    selector_status s = 0;
    s |= selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
    s |= selector_set_interest_key(key,                                OP_READ);

    return SELECTOR_SUCCESS == s ? HELLO : ERROR;
}

/////////////////////////////////////////////////////////////////////////////
// HELLO
/////////////////////////////////////////////////////////////////////////////


/** inicializa las variables del estado HELLO */
static void hello_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
}

/** Lee todos los bytes del mensaje de tipo `hello' del servidor */
static unsigned hello_read(struct selector_key *key) {
    struct hello_st *d      = &ATTACHMENT(key)->orig.hello;
    enum pop3_state  ret    = HELLO;
    bool  error             = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;
    size_t len;

    ptr = buffer_write_ptr(d->wb, &count);
    const char * messsage = "+OK Proxy server POP3.\r\n";
    len = strlen(messsage);
    strcpy((char *) ptr, messsage);
    buffer_write_adv(d->wb, len);

    ptr = buffer_write_ptr(d->wb, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {

        selector_status s = SELECTOR_SUCCESS;
        s |= selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        s |= selector_set_interest_key(key, OP_NOOP);
        if (s != SELECTOR_SUCCESS) {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escribe todos los bytes del mensaje `hello' al cliente */
static unsigned hello_write(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    unsigned  ret      = HELLO;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    // le mando el mensaje que recibi desde el origin
    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        if(!buffer_can_read(d->wb)) {
            selector_status s = SELECTOR_SUCCESS;
            s |= selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_READ);
            s |= selector_set_interest_key(key, OP_NOOP);

            ret = SELECTOR_SUCCESS == s ? REQUEST_CAPA : ERROR;

            if (ret == REQUEST_CAPA) {
                char * msg = "CAPA\r\n";
                send(ATTACHMENT(key)->origin_fd, msg, strlen(msg), 0);
            }
        }
    }

    return ret;
}

static void
hello_close(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    buffer_reset(d->rb);
    buffer_reset(d->wb);
}


////////////////////////////////////////////////////////////////////////////////
// CAPA
////////////////////////////////////////////////////////////////////////////////

void capa_init(const unsigned state, struct selector_key *key) {
    struct response_st * d      = &ATTACHMENT(key)->orig.response;


    d->rb                       = &(ATTACHMENT(key)->read_buffer);
    d->wb                       = &(ATTACHMENT(key)->write_buffer);

    struct pop3_request *r      = new_request(get_cmd("capa"), NULL);

    d->request                  = r;
    d->response_parser.request  = d->request;
    response_parser_init(&d->response_parser);

}

// Lee respuesta del comando capa
static unsigned capa_read(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    enum pop3_state ret  = REQUEST_CAPA;
    bool  error        = false;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(d->rb, n);
        enum response_state st = response_consume_first_line(d->rb, d->wb, &d->response_parser, &error);
        if (response_is_done(st, 0)) {

            // set_pipelining(key, d);

            selector_status s = SELECTOR_SUCCESS;
            s |= selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_READ);
            s |= selector_set_interest_key(key, OP_NOOP);
            ret = SELECTOR_SUCCESS == s ? REQUEST : ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}


////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

enum pop3_state request_process(struct selector_key *key, struct request_st * d);

// inicializacion de variables
static void request_init(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    d->rb              = &(ATTACHMENT(key)->read_buffer);
    d->wb              = &(ATTACHMENT(key)->write_buffer);

    buffer_reset(d->rb);
    buffer_reset(d->wb);

    d->request_parser.request  = &d->request;
    request_parser_init(&d->request_parser);
}

// Lee el request del mua 
static unsigned request_read(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;
   
    buffer *b            = d->rb;    
    enum pop3_state ret  = REQUEST;
    bool  error          = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);
    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        int st = request_consume(b, &d->request_parser, &error);
        if (request_is_done(st, 0)) {
            ret = request_process(key, d);
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

// procesa una request ya parseada
enum pop3_state request_process(struct selector_key *key, struct request_st * d) {
    enum pop3_state ret = REQUEST;

    if (d->request_parser.state >= req_error) {
        char * message = NULL;

        //se manda un mensaje de error al cliente y se vuelve a leer de client_fd
        switch (d->request_parser.state) {
            case req_error_long_param:
                message = "-ERR Parameter is too long.\r\n";
                break;
            case req_error_long_cmd:
                message = "-ERR Command is too long.\r\n";
                break;
            case req_error:
                message = "-ERR Unknown command.\r\n";
                break;
            default:
                break;
        }

        send(key->fd, message, strlen(message), 0);

        ATTACHMENT(key)->session.invalid_cmds++;
        unsigned int invalid_cmds = ATTACHMENT(key)->session.invalid_cmds;
        if (invalid_cmds >= MAX_INVALID_CMDS) {
            message = "-ERR Too many invalid commands.\n";
            send(key->fd, message, strlen(message), 0);
            ret = DONE;
            goto finally;
        }

        //reseteamos el parser
        request_parser_init(&d->request_parser);
        ret = REQUEST;
        goto finally;
    }

    ATTACHMENT(key)->session.invalid_cmds = 0;

    struct pop3_request *req = new_request(d->request.cmd, d->request.args);
    if (req == NULL) {
        ret = ERROR;
        goto finally;
    }

    queue_add(ATTACHMENT(key)->session.request_queue, req);
    request_parser_init(&d->request_parser);


    if (!buffer_can_read(d->rb)) {
        selector_status s = SELECTOR_SUCCESS;

        s |= selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
        s |= selector_set_interest_key(key, OP_NOOP);

        ret = SELECTOR_SUCCESS == s ? ret : ERROR;
    }

finally:
    return ret;

}


/** Escrible un request en el server */
static unsigned request_write(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    unsigned  ret      = REQUEST;
    buffer *b          = d->wb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    struct pop3_request *req;
    struct queue *q = ATTACHMENT(key)->session.request_queue;

    //si el server no soporta pipelining se manda la primer request
    if (ATTACHMENT(key)->session.pipelining == false) {
        req = queue_peek(q);
        if (req == NULL) {
            ret = ERROR;
            goto finally;
        }
        // copio la request en el buffer
        if (-1 == request_marshall(req, b)) {
            ret = ERROR;
        }
    } else {
        // si el server soporta pipelining copio el resto de las requests y las mando todas juntas
        while ((req = queue_get_next(q)) != NULL) {
            if (-1 == request_marshall(req, b)) {
                fprintf(stderr, "Buffer error");
                ret = ERROR;
                goto finally;
            }
        }
    }

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n < 0) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if(!buffer_can_read(b)) {

            selector_status s = SELECTOR_SUCCESS;

            s = selector_set_interest_key(key, OP_READ);

            ret = SELECTOR_SUCCESS == s ? RESPONSE : ERROR;
        }
    }

finally:
    return ret;
}

static void
request_close(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    d->rb              = &(ATTACHMENT(key)->read_buffer);
    d->wb              = &(ATTACHMENT(key)->write_buffer);

    buffer_reset(d->rb);
    buffer_reset(d->wb);

    request_parser_close(&d->request_parser);
}


////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

// enum pop3_state response_process(struct selector_key *key, struct response_st * d);

// void set_request(struct response_st *d, struct pop3_request *request) {
//     if (request == NULL) {
//         fprintf(stderr, "Request is NULL");
//         abort();
//     }
//     d->request                  = request;
//     d->response_parser.request  = request;
// }

void response_init(const unsigned state, struct selector_key *key) {
    struct response_st * d = &ATTACHMENT(key)->orig.response;

    d->rb                       = &(ATTACHMENT(key)->read_buffer);
    d->wb                       = &(ATTACHMENT(key)->write_buffer);

    response_parser_init(&d->response_parser);
    set_request(d, queue_remove(ATTACHMENT(key)->session.request_queue));
}



// Lee la respuesta del origin server. posible paso el estado transformacion externa
static unsigned response_read(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;

    buffer  *b         = d->rb;
    unsigned  ret      = RESPONSE;
    bool  error        = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        int st = response_consume(d->rb, d->wb, &d->response_parser, &error);

        // se termino de leer la primera linea
        if (d->response_parser.first_line_done) {

            d->response_parser.first_line_done = false;

            // si el comando era retr pasa a la transformacion externa
            if (st == response_mail && d->request->cmd->id == retr && d->request->response->status == response_status_ok) {
                if (params->external_transf_activated && params->filter_command != NULL) {   // revisar input_data

                    selector_status s = SELECTOR_SUCCESS;
                    s |= selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
                    s |= selector_set_interest_key(key, OP_NOOP);
                    

                    while (buffer_can_read(d->wb)) {
                        buffer_read(d->wb);
                    }

                    ret = s == SELECTOR_SUCCESS ? EXTERNAL_TRANSFORMATION : ERROR;
                    goto finally;
                }
            }

            //consumimos el resto de la respuesta
            st = response_consume(d->rb, d->wb, &d->response_parser, &error);
        }

        selector_status s = SELECTOR_SUCCESS;
        s |= selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        s |= selector_set_interest_key(key, OP_NOOP);
        ret = s == SELECTOR_SUCCESS ? RESPONSE : ERROR;

        if (response_is_done(st, 0) && ret == RESPONSE) {
            if (d->request->cmd->id == capa) {
                response_process_capa(d);
            }
        }
    } else if (n < 0){
        ret = ERROR;
    }

    if (error) {
        ret = ERROR;
    }

finally:
    return ret;
}


/** Escribe respuesta en el mua */
static unsigned response_write(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;

    buffer *b = d->wb;
    enum pop3_state  ret = RESPONSE;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n < 0) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b)) {

            if (d->response_parser.state != response_done) {
                if (d->request->cmd->id == retr) {
                        // metricas->transferred_bytes += n;
                }
                selector_status s = SELECTOR_SUCCESS;
                s |= selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_READ);
                s |= selector_set_interest_key(key, OP_NOOP);
                ret = s == SELECTOR_SUCCESS ? RESPONSE : ERROR;

            } else {
                if (d->request->cmd->id == retr)
                ret = response_process(key, d);
                // metricas->retrieved_messages++;
            }
        }
    }

    return ret;
}


// enum pop3_state response_process_capa(struct response_st *d) {
//     // busco pipelinig
//     to_upper(d->response_parser.capa_response);
//     char * capa = d->response_parser.capa_response;
//     // siempre pasar la needle en upper case
//     char * needle = "PIPELINING";

//     if (strstr(capa, needle) != NULL) {
//         return RESPONSE;
//     }

//     // else
//     size_t capa_length = strlen(capa);
//     size_t needle_length = strlen(needle);

//     char * eom = "\r\n.\r\n";
//     size_t eom_length = strlen(eom);

//     char * new_capa = calloc(capa_length - 3 + needle_length + eom_length + 1, sizeof(char));
//     if (new_capa == NULL) {
//         return ERROR;
//     }
//     // copio to-do menos los ultimos 3 caracteres
//     memcpy(new_capa, capa, capa_length - 3);
//     // agrego la needle
//     memcpy(new_capa + capa_length - 3, needle, needle_length);
//     // agrego eom
//     memcpy(new_capa + capa_length - 3 + needle_length, eom, eom_length);

//     //printf("--%s--", new_capa);

//     free(capa);

//     d->response_parser.capa_response = new_capa;

//     //leer el buffer y copiar la nueva respuesta
//     while (buffer_can_read(d->wb)) {
//         buffer_read(d->wb);
//     }

//     uint8_t *ptr1;
//     size_t   count1;

//     ptr1 = buffer_write_ptr(d->wb, &count1);
//     strcpy((char *)ptr1, new_capa);
//     buffer_write_adv(d->wb, strlen(new_capa));

//     return RESPONSE;
// }


// // enum pop3_state response_process(struct selector_key *key, struct response_st * d) {
// //     enum pop3_state ret;

// //     switch (d->request->cmd->id) {
// //         case quit:
// //             selector_set_interest_key(key, OP_NOOP);
// //             ATTACHMENT(key)->session.state = POP3_UPDATE;
// //             return DONE;
// //         case user:
// //             ATTACHMENT(key)->session.user = d->request->args;
// //             break;
// //         case pass:
// //             if (d->request->response->status == response_status_ok)
// //                 ATTACHMENT(key)->session.state = POP3_TRANSACTION;
// //             break;
// //         case capa:
// //             break;
// //         default:
// //             break;
// //     }

// //     // si quedan mas requests/responses por procesar
// //     struct queue *q = ATTACHMENT(key)->session.request_queue;
// //     if (!queue_is_empty(q)) {
// //         // vuelvo a response_read porque el server soporta pipelining entonces ya le mande to-do y espero respuestas
// //         if (ATTACHMENT(key)->session.pipelining) {
// //             set_request(d, queue_remove(q));
// //             response_parser_init(&d->response_parser);

// //             selector_status ss = SELECTOR_SUCCESS;
// //             ss |= selector_set_interest_key(key, OP_NOOP);
// //             ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
// //             ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
// //         } else {
// //             //vuelvo a request write, hay request encoladas que todavia no se mandaron
// //             selector_status ss = SELECTOR_SUCCESS;
// //             ss |= selector_set_interest_key(key, OP_NOOP);
// //             ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
// //             ret = ss == SELECTOR_SUCCESS ? REQUEST : ERROR;
// //         }

// //     } else {
// //         // voy a request read
// //         selector_status ss = SELECTOR_SUCCESS;
// //         ss |= selector_set_interest_key(key, OP_READ);
// //         ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
// //         ret = ss == SELECTOR_SUCCESS ? REQUEST : ERROR;
// //     }

// //     return ret;
// // }

static void
response_close(const unsigned state, struct selector_key *key) {
    struct response_st * d = &ATTACHMENT(key)->orig.response;

    d->rb              = &(ATTACHMENT(key)->read_buffer);
    d->wb              = &(ATTACHMENT(key)->write_buffer);

    buffer_reset(d->rb);
    buffer_reset(d->wb);

    response_parser_close(&d->response_parser);
}

// handlers para cada estado 
static const struct state_definition client_statbl[] = {
        {
                .state            = RESOLVE_ORIGIN,
                .on_write_ready   = resolve_origin_init,
                .on_block_ready   = resolve_origin_done,
        },{
                .state            = REQUEST_CONNECTING,
                // .on_arrival       = connecting_init,
                .on_write_ready   = connecting,
        },{
                .state            = HELLO,
                .on_arrival       = hello_init,
                .on_read_ready    = hello_read,
                .on_write_ready   = hello_write,
                .on_departure     = hello_close,
        }, {
                .state            = REQUEST_CAPA,
                .on_arrival       = capa_init,
                .on_read_ready    = capa_read,
        },{

                .state            = RESPONSE,
                .on_arrival       = response_init,
                .on_read_ready    = response_read,
                .on_write_ready   = response_write,
                .on_departure     = response_close,
        },{
        //         .state            = EXTERNAL_TRANSFORMATION,
        //         .on_arrival       = external_transformation_init,
        //         .on_read_ready    = external_transformation_read,
        //         .on_write_ready   = external_transformation_write,
        //         .on_departure     = external_transformation_close,
        // },{
                .state            = DONE,

        },{
                .state            = ERROR,
        }
};

static const struct state_definition * pop3_describe_states(void) {
    return client_statbl;
}


///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void
pop3_done(struct selector_key *key);

static void
pop3_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st    = (enum pop3_state)stm_handler_read(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st    = (enum pop3_state)stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st    = (enum pop3_state)stm_handler_block(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_close(struct selector_key *key) {
    pop3_destroy(ATTACHMENT(key));
}

static void
pop3_done(struct selector_key *key) {
    const int fds[] = {
            ATTACHMENT(key)->client_fd,
            ATTACHMENT(key)->origin_fd,
    };

    if (ATTACHMENT(key)->origin_fd != -1) {
        // met->concurrent_connections--;
    }

    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }

}

void pop3_session_init(struct pop3_session *s, bool pipeline) {
    memset(s, 0, sizeof(*s));

    s->pipelining = pipeline;
    s->state = SESSION_AUTH;

    // s->request_queue = queue_new();
}

void pop3_session_close(struct pop3_session *s) {
    // queue_destroy(s->request_queue);
    s->state = DONE;
}