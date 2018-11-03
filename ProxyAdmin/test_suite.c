#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "test_suite.h"

#define OK 1
#define ERR 0

#define OP_CODE_SIZE 1
#define STATUS_SIZE 1
#define LENGTH_SIZE 2

#define OP_CODE 0
#define STATUS OP_CODE_SIZE
#define LENGTH (OP_CODE_SIZE + STATUS_SIZE)

#define SCTP_PORT 9090

typedef enum {
    USER = 0,
    PASS,
    METRICS,
    TRANSF_ON,
    TRANSF_OFF,
    CHG_TRANSF,
    ADD_MIME,
    REMOVE_MIME,
    LIST_MIMES,
    EXIT
} op_code;

typedef struct package {
    uint8_t op_code;
    uint8_t status;
    uint16_t length;
    uint8_t *content;
} package;

int fake_proxy_socket;


int main(int argc, char const *argv[])
{
    fake_proxy_server();
    
    return 0;
}

void
got_an_error(char * msg) {
    perror(msg);
    exit(1);
}

/*
void
test_connect_to_proxy_pop3() {
    int socket;

    if (connect_to_proxy_pop3(&socket))
        printf("[SUCCEEDED] ");
    else
        printf("[FAILED] ");

    printf("Connection to proxy test\n");
}

void 
run_psmp_packager_tests() {
    printf("Begin testing:\n\n");

    test_connect_to_proxy_pop3();
}
*/

void
fake_proxy_server() {
    int listen_socket;
    int reuse = 1;
    int accepting_requests = 0, connection_closed = 0;
    struct sockaddr_in serv_addr;
    uint16_t max_packet_size = 128;
    package *pckg = NULL;

    struct sctp_initmsg initmsg = {
        .sinit_num_ostreams = 5,
        .sinit_max_instreams = 5,
        .sinit_max_attempts = 4,
    };
    
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    setsockopt(listen_socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, 
        sizeof(initmsg));
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (listen_socket == -1)
        got_an_error("Attempting to create a socket;");

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port        = htons(SCTP_PORT);

    if (bind(listen_socket, (struct sockaddr *) &serv_addr, 
        sizeof(serv_addr)) == -1)
        got_an_error("Attempting to bind the socket\n");

    if (listen(listen_socket, initmsg.sinit_max_instreams) == -1)
        got_an_error("Attempting to listen for connections\n");

    fake_proxy_socket = accept(listen_socket, (struct sockaddr *) NULL, NULL);
    accepting_requests = fake_proxy_socket != -1;

    while (!connection_closed && accepting_requests) {
        pckg = recv_package(fake_proxy_socket, max_packet_size);
        pckg = handle_package(pckg);
        send_package(fake_proxy_socket, pckg);

        if (pckg->op_code == EXIT && pckg->status == OK)
            connection_closed = close_connection(fake_proxy_socket);
    }
}


package *
handle_package(package *pckg) {
    int request_fullfilled = 0;

    switch (pckg->op_code) {
        case USER:
            request_fullfilled = check_auth(USER, (char *) pckg->content);
            break;

        case PASS:
            request_fullfilled = check_auth(PASS, (char *) pckg->content);
            break;

        case METRICS:
            //request_fullfilled = get_metrics();
            break;

        case TRANSF_ON:
            request_fullfilled = set_transformation(TRANSF_ON);
            break;

        case TRANSF_OFF:
            request_fullfilled = set_transformation(TRANSF_OFF);
            break;

        case CHG_TRANSF:
            request_fullfilled = change_transformation((char *) pckg->content);
            break;

        case ADD_MIME:
            request_fullfilled = mime_do(ADD_MIME);
            break;

        case REMOVE_MIME:
            request_fullfilled = mime_do(REMOVE_MIME);
            break;

        case LIST_MIMES:
            request_fullfilled = list_mime_types();
            break;

        case EXIT:
            request_fullfilled = stop_reading_from_admin();
            break;
    }

    if (request_fullfilled)
        pckg->status = OK;
    else
        pckg->status = ERR;

    return pckg;
}

int
set_transformation(uint8_t op_code) {
    switch (op_code) {
        case TRANSF_ON:
            printf("Transformation is set on\n");
            return 1;

        case TRANSF_OFF:
            printf("Transformation is set off\n");
            return 1;
    }

    return 0;
}

int
check_auth(uint8_t auth_type, char *auth_key) {
    switch (auth_type) {
        case USER:
            return strcmp("admin", auth_key) == 0;

        case PASS:
            return strcmp("admin", auth_key) == 0;
    }

    return 0;
}

int
mime_do(uint8_t op_code) {
    switch (auth_type) {
        case ADD_MIME:
            printf("MIME type added\n");
            return 1;

        case REMOVE_MIME:
            printf("Transformation is set on\n");
            return 1;
    }

    return 0;
}

int
change_transformation(char *program_path) {
    return 1;
}

int
stop_reading_from_admin() {
    return shutdown(fake_proxy_socket, SHUT_RD) != -1;
}

int
close_connection(int socket_fd) {
    int shut_down = shutdown(socket_fd, SHUT_RDWR);
    int closed    = close(socket_fd);

    return shut_down != -1 && closed != -1;
}






package *
create_package(uint8_t op_code, uint8_t status, uint16_t length, 
    uint8_t *content) {
    package *pckg = malloc(sizeof(package));

    pckg->op_code = op_code;
    pckg->status = status;
    pckg->length = length;

    if (length > 0) {
        pckg->content = calloc(length, sizeof(uint8_t));
        pckg->content = memcpy(pckg->content, content, length);
    } else {
        pckg->content = NULL;
    }

    return pckg;
}

void
send_package(int from_socket_fd, package *data) {
    int header_size = OP_CODE_SIZE + STATUS_SIZE + LENGTH_SIZE;
    int buffer_size = header_size + data->length;
    uint8_t *buffer = calloc(buffer_size, sizeof(uint8_t));

    unpack(data, buffer, header_size);

    int bytes_sent = sctp_sendmsg(from_socket_fd, (void *) buffer,
        (size_t) buffer_size, (struct sockaddr *) NULL, 0, 0, 0, 0, 0, 0);

    if (bytes_sent == -1)
        got_an_error("Attempting to send a message over SCTP");
}

void
unpack(package *from_data, uint8_t *to_buffer, int header_size) {
    to_buffer[OP_CODE] = from_data->op_code;
    to_buffer[STATUS]  = from_data->status;
    to_buffer[LENGTH]  = from_data->length;

    if (from_data->length > 0)
        to_buffer[header_size] = *(uint8_t *) memcpy(to_buffer + header_size, 
            from_data->content, from_data->length);
}

package *
recv_package(int at_socket_fd, int max_content_size) {
    int header_size = OP_CODE_SIZE + STATUS_SIZE + LENGTH_SIZE;
    int buffer_size = header_size + max_content_size;
    uint8_t *buffer = calloc(buffer_size, sizeof(uint8_t));
    package *pckg = NULL;

    int bytes_rcvd = sctp_recvmsg(at_socket_fd, (void *) buffer, 
        (size_t) buffer_size, (struct sockaddr *) NULL, NULL, NULL, NULL);

    if (bytes_rcvd == -1)
        got_an_error("Attempting to receive a message over SCTP");

    pckg = create_package(buffer[OP_CODE], buffer[STATUS], buffer[LENGTH], 
        buffer + header_size);

    return pckg;
}