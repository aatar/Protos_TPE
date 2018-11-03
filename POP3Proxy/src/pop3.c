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
#include "input_data.h"
#include "selector.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


#define BUFFER_SIZE 2048

// estados de pop3 
enum pop3_state {
    /**
     *  Resuelve el nombre del origin server
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


//  pool de pop3_data
static const unsigned  max_pool  = 50; // tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static pop3_data      * pool     = NULL;


static pop3_data * pop3_init(int client_fd) {
    pop3_data * pop3;

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

    // buffer_init(&pop3->read_buffer,  N(pop3->raw_buff_a), pop3->raw_buff_a);
    // buffer_init(&pop3->write_buffer, N(pop3->raw_buff_b), pop3->raw_buff_b);

    // buffer_init(&pop3->super_buffer,  N(pop3->raw_super_buffer), pop3->raw_super_buffer);
    // buffer_init(&pop3->extern_read_buffer,  N(pop3->raw_extern_read_buffer), pop3->raw_extern_read_buffer);

    // pop3_session_init(&pop3->session, false);

    pop3->references = 1;

finally:
    return pop3;
}

/////////////////////////////////////////////////////////////////////////////
// RESOLVE_ORIGIN
/////////////////////////////////////////////////////////////////////////////


unsigned resolve_origin_init(struct selector_key *key){
    unsigned ret;
    pthread_t tid;
    struct selector_key* k = malloc(sizeof(*key));

    if(k == NULL) {
        ret = ERROR;
    } else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0, origin_resolv_blocking, k)) {
            ret = ERROR;
        } else{
            ret = RESOLVE_ORIGIN;
            selector_set_interest_key(key, OP_NOOP);
        }
    }

    return RET;
}


/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void *
origin_resolv_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    pop3_data       *s   = ATTACHMENT(key);

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
                         &s->origin_resolution))/ {
        fprintf(stderr, "Domain name error\n");
    }

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

/** procesa el resultado de la resolución de nombres */
static unsigned origin_resolv_done(struct selector_key *key) {
    pop3_data *s      =  ATTACHMENT(key);

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

    int fd   = socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, IPPROTO_TCP);


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
                goto error;
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
            close(*fd);
            *fd = -1;
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
    pop3_data *d = ATTACHMENT(key);

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


// /** inicializa las variables del estado HELLO */
// static void
// hello_init(const unsigned state, struct selector_key *key) {
//     struct hello_st *d = &ATTACHMENT(key)->orig.hello;

//     d->wb = &(ATTACHMENT(key)->write_buffer);
// }

// /** Lee todos los bytes del mensaje de tipo `hello' de server_fd */
// static unsigned
// hello_read(struct selector_key *key) {
//     struct hello_st *d      = &ATTACHMENT(key)->orig.hello;
//     enum pop3_state  ret    = HELLO;
//     uint8_t *ptr;
//     size_t  count;
//     ssize_t  n;

//     ///////////////////////////////////////////////////////
//     //Proxy welcome message
//     ptr = buffer_write_ptr(d->wb, &count);
//     const char * msg = "+OK Proxy server POP3 ready.\r\n";
//     n = strlen(msg);
//     strcpy((char *) ptr, msg);
//     // memccpy(ptr, msg, 0, count);
//     buffer_write_adv(d->wb, n);
//     //////////////////////////////////////////////////////

//     ptr = buffer_write_ptr(d->wb, &count);
//     n = recv(key->fd, ptr, count, 0);

//     if(n > 0) {
//         buffer_write_adv(d->wb, 0);

//         selector_status ss = SELECTOR_SUCCESS;
//         ss |= selector_set_interest_key(key, OP_NOOP);
//         ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
//         if (ss != SELECTOR_SUCCESS) {
//             ret = ERROR;
//         }
//     } else {
//         ret = ERROR;
//     }

//     return ret;
// }

// /** Escribe todos los bytes del mensaje `hello' en client_fd */
// static unsigned
// hello_write(struct selector_key *key) {
//     struct hello_st *d = &ATTACHMENT(key)->orig.hello;

//     unsigned  ret      = HELLO;
//     uint8_t *ptr;
//     size_t  count;
//     ssize_t  n;

//     ptr = buffer_read_ptr(d->wb, &count);
//     n = send(key->fd, ptr, count, MSG_NOSIGNAL);

//     if(n == -1) {
//         ret = ERROR;
//     } else {
//         buffer_read_adv(d->wb, n);
//         if(!buffer_can_read(d->wb)) {
//             selector_status ss = SELECTOR_SUCCESS;
//             ss |= selector_set_interest_key(key, OP_NOOP);
//             ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
//             ret = SELECTOR_SUCCESS == ss ? CAPA : ERROR;

//             if (ret == CAPA) {
//                 char * msg = "CAPA\r\n";
//                 send(ATTACHMENT(key)->origin_fd, msg, strlen(msg), 0);
//             }
//         }
//     }

//     return ret;
// }

// static void
// hello_close(const unsigned state, struct selector_key *key) {
//     //nada por hacer
// }






/** obtiene el struct (pop3 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (pop3_data *)(key)->data)

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
    pop3_data                             *state = NULL;

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


void pop3_destroy(pop3_data *s) {
    if(s != NULL && s->references == 1) {
            if(pool_size < max_pool) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                if(s->origin_resolution != NULL) {
                    freeaddrinfo(s->origin_resolution);
                    s->origin_resolution = NULL;
                }
                free(s);
            }
    } else {
        s->references -= 1;
    }
}


void pop3_pool_destroy(void) {
    pop3_data *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
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
        }
        // ,{
        //         .state            = HELLO,
        //         .on_arrival       = hello_init,
        //         .on_read_ready    = hello_read,
        //         .on_write_ready   = hello_write,
        //         .on_departure     = hello_close,
        // }, {
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

void pop3_session_init(struct pop3_session *s, bool pipelining) {
    memset(s, 0, sizeof(*s));

    s->pipelining = pipelining;
    s->state = POP3_AUTHORIZATION;

    s->request_queue = queue_new();
}

void pop3_session_close(struct pop3_session *s) {
    queue_destroy(s->request_queue);
    s->state = DONE;
}