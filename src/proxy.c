#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#define FIBERTEL_IP "201.212.7.96"
#define USERNAME_FIBERTEL "lucas.sg.07@fibertel.com.ar"
#define PASSWORD_FIBERTEL "f1del1n4"
#define PORT 110

#define ALIVE 0
#define CLOSED -1
#define INPUT_ERROR -2

void
got_an_error(char * msg) {
	perror(msg);
	exit(1);
}

void
setup_proxy_socket(int *proxy_socket_fd) {
	struct sockaddr_in server_addr;
	int server_addr_length;

	*proxy_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (*proxy_socket_fd == -1)
		got_an_error("Proxy socket creation");

	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(FIBERTEL_IP);
	server_addr.sin_port        = htons(PORT);
    server_addr_length          = sizeof(server_addr);

	if (connect(*proxy_socket_fd, (struct sockaddr *) &server_addr,
		server_addr_length) == -1)
		got_an_error("Proxy socket attempting to connect");
}

int
read_origin_server(int proxy_socket_fd) {
	char *buffer   = calloc(1024, sizeof(char));
	int bytes_read = 0;

	bytes_read = read(proxy_socket_fd, buffer, 256);
	printf("%s", buffer);

	return bytes_read;
}

void
read_user_input(char *c, int *index, int max_size, char *user_input) {
	*index = 0;
	memset(user_input, 0, max_size);

	while (((*c = getchar()) != '\n') && (*index != (max_size - 2)) &&
		(*c != EOF)) {
		user_input[(*index)++] = *c;
	}
}

int
communicate_with_server(const int proxy_socket_fd, char *user_input,
	const int index) {
	write(proxy_socket_fd, user_input, index + 1);

	if (read_origin_server(proxy_socket_fd) == 0) {
		printf("The POP3 origin server disconnected\n");
		return CLOSED;
	}

	return ALIVE;
}

int
process_user_input(const char c, char *user_input, const int index,
	const int max_size, const int proxy_socket_fd) {
	int connection_state = ALIVE;

	if (c == '\n') {
		user_input[index] = '\n';

		if (index == (max_size - 2))
			printf("This protocol does not handle commands larger than %d "
				"characters\n", max_size - 2);
		else
			connection_state = communicate_with_server(proxy_socket_fd, user_input,
				index);
	} else {
		connection_state = INPUT_ERROR;
		printf("EOF\n");
	}

	return connection_state;
}

int main(int argc, char const *argv[]) {
	int proxy_socket_fd = -1;
	int index = 0, max_size = 128, connection_state = ALIVE;
	char user_input[max_size];
	char c = -1;

	setup_proxy_socket(&proxy_socket_fd);
	read_origin_server(proxy_socket_fd);

	while (connection_state == ALIVE) {
		read_user_input(&c, &index, max_size, user_input);
		connection_state = process_user_input(c, user_input, index, max_size,
			proxy_socket_fd);
	}

	return 0;
}
