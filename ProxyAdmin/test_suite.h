#ifndef TEST_SUITE_H
#define TEST_SUITE_H

//#include "psmp_packager.h"


/*
void
test_connect_to_proxy_pop3();

void
run_psmp_packager_tests();
*/

typedef struct package package;

void
fake_proxy_server();

package *
handle_package(package *pckg);

int
set_transformation(uint8_t op_code);

package *
create_package(uint8_t op_code, uint8_t status, uint16_t length, 
    uint8_t *content);

void
send_package(int from_socket_fd, package *data);

void
unpack(package *from_data, uint8_t *to_buffer, int header_size);

package *
recv_package(int at_socket_fd, int max_content_size);

int
check_auth(uint8_t auth_type, char *auth_key);

int
stop_reading_from_admin();

int
close_connection(int socket_fd);

#endif