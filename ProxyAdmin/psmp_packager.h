#ifndef PSMP_PACKAGER_H
#define PSMP_PACKAGER_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
#define USER 0
#define PASS 1
#define METRICS 2
#define TRANSF_ON 3
#define TRANSF_OFF 4
#define CHG_TRANSF 5
#define ADD_MIME 6
#define REMOVE_MIME 7
#define LIST_MIMES 8
#define EXIT
*/

#define OP_CODE_SIZE 1
#define STATUS_SIZE 1
#define LENGTH_SIZE 2

#define OP_CODE 0
#define STATUS OP_CODE_SIZE
#define LENGTH (OP_CODE_SIZE + STATUS_SIZE)


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

void
got_an_error(char * msg);

package *
create_package(uint8_t op_code, uint8_t status, uint16_t length, 
    uint8_t *content);

void
send_package(int from_socket_fd, package *data);

package *
recv_package(int at_socket_fd, int max_content_size);

void
unpack(package *from_data, uint8_t *to_buffer, int header_size);

void
setup_proxy_server_addr(struct sockaddr_in *proxy_server_addr);

int
connect_to_proxy_pop3();

int
close_connection(int socket_fd);

#endif
