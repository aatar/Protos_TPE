#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psmp_packager.h"



/*
**  DATA PACKAGE STRUCTURE:
**
**  +---------+--------+-------------+-------------------------+
**  | OP_CODE | STATUS |   LENGTH    |         CONTENT         |
**  +---------+--------+-------------+-------------------------+
**  | 1 byte  | 1 byte |   2 bytes   |     [length] bytes      |
**  +---------+--------+-------------+-------------------------+
*/

#define OK 1

#define OP_CODE_SIZE 1
#define STATUS_SIZE 1
#define LENGTH_SIZE 2

#define OP_CODE 0
#define STATUS OP_CODE_SIZE
#define LENGTH (OP_CODE_SIZE + STATUS_SIZE)

#define SCTP_PORT 9090

void
got_an_error(char * msg) {
	perror(msg);
	exit(1);
}

package *
create_package(uint8_t op_code, uint8_t status, uint16_t length, 
    uint8_t *content) {
    package *pckg = malloc(sizeof(package));

    pckg->op_code = op_code;
    pckg->status  = status;
    pckg->length  = length;

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
    package *pckg   = NULL;

    int bytes_rcvd = sctp_recvmsg(at_socket_fd, (void *) buffer, 
        (size_t) buffer_size, (struct sockaddr *) NULL, NULL, NULL, NULL);

    if (bytes_rcvd == -1)
        got_an_error("Attempting to receive a message over SCTP");

    pckg = create_package(buffer[OP_CODE], buffer[STATUS], buffer[LENGTH], 
        buffer + header_size);

    return pckg;
}

int
connect_to_proxy_pop3() {
    struct sockaddr_in proxy_server_addr;
    int addr_length = 0;

    int admin_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    if (admin_socket_fd == -1)
        got_an_error("Admin socket creation");

    setup_proxy_server_addr(&proxy_server_addr);
    addr_length = sizeof(proxy_server_addr);

    if (connect(admin_socket_fd, (struct sockaddr *) &proxy_server_addr,
        addr_length) == -1)
        return -1;
    else 
        return admin_socket_fd;
}

void
setup_proxy_server_addr(struct sockaddr_in *proxy_server_addr) {
    memset(proxy_server_addr, 0, sizeof(struct sockaddr_in));

    proxy_server_addr->sin_family      = AF_INET;
    proxy_server_addr->sin_addr.s_addr = inet_addr("127.0.0.1");
    proxy_server_addr->sin_port        = htons(SCTP_PORT);
}

int
close_connection(int socket_fd) {
    int shut_down = shutdown(socket_fd, SHUT_RDWR);
    int closed    = close(socket_fd);

    return shut_down != -1 && closed != -1;
}