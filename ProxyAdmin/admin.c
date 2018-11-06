#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "admin.h"

#define TRUE 1
#define FALSE 0

#define OK 1
#define ERR 0

#define MENU_VIEW_METRICS 1
#define MENU_TRANSFORMATION_ON_OFF 2
#define MENU_CHG_TRANSFORMATION 3
#define MENU_ADD_REMOVE_MIME 4
#define MENU_LIST_MIME 5
#define MENU_EXIT 6

#define MENU_TRANSF_ON 1
#define MENU_TRANSF_OFF 2

#define MENU_ADD_MIME 1
#define MENU_REMOVE_MIME 2

#define INVALID_INPUT -1

#define MAX_AUTH_ATTEMPTS 3
#define MAX_AUTH_STRING_SIZE 64
#define MAX_METRICS_SIZE 64
#define MAX_MIME_LIST_SIZE 2048



#define ATTACHMENT(key) ((struct admin_data *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))



authentication *
ask_for_username();

authentication *
ask_for_password();

authentication *
init_auth(auth_request *auth_req);

authentication *
ask_for_auth(authentication *auth);

char *
read_auth(authentication *auth);

void
reattempt_to_auth(authentication *auth);

char *
read_user_input(int max_size);

int
is_valid_input(const char *user_input);

void
check_auth(authentication *auth);

void
print_line(const char* s);

void
show_menu();

int
read_menu_option();

int
menu_option_to_int(const char *user_input);

int
is_digit(char c);

int
is_valid_menu_option(int menu_option);

int
invalid_menu_input();

void
execute_menu_option(int menu_option);

void
get_metrics();

int
is_okay(package *pckg);

void
print_metrics(uint8_t *metrics);

void
set_transformation_on_off();

int
ask_for_transformation_on_off();

void
set_transformation(op_code transformation);

void
change_transformation();

char *
ask_for_transformation_program();

void
add_or_remove_mime();

int
ask_for_add_or_remove_mime();

void
send_mime_action(op_code action);

void
list_mime_types();

void
show_mime_list(char *mime_list);

void
exit_proxy_management();

package *
handle_package(package *pckg);

int
proxy_set_transformation(uint8_t op_code);

int
proxy_check_auth(uint8_t auth_type, char *auth_key);

int
mime_do(uint8_t op_code);

int
proxy_change_transf(char *program_path);




typedef struct authentication {
	char *name;
	char *buffer;
	int max_size;
	int attempt;
	int valid;
	uint8_t op_code;
} authentication;

typedef struct auth_request {
	char *name;
	uint8_t op_code;
} auth_request;

static int admin_socket_fd;


int main(int argc, char const *argv[])
{
	admin_socket_fd = connect_to_proxy_pop3();
	printf("CONNECTED WITH FD: %d\n", admin_socket_fd);
	start_interpreter();
	close(admin_socket_fd);

	return 0;
}

void
start_interpreter() {
	int menu_option = 0;
	authentication *username = NULL, *password = NULL;

	username = ask_for_username();
	if (!username->valid)
		return;

	password = ask_for_password();
	if (!password->valid)
		printf("BAD PASSWORD\n");
	if (!password->valid)
		return;

	do {
		show_menu();
		menu_option = read_menu_option();
		execute_menu_option(menu_option);
	} while (menu_option != MENU_EXIT);
}

authentication *
ask_for_username() {
	auth_request auth_req = {
		.name = "username",
		.op_code = USER
	};

	authentication *auth = init_auth(&auth_req);

	return ask_for_auth(auth);
}

authentication *
ask_for_password() {
	auth_request auth_req = {
		.name = "password",
		.op_code = PASS
	};

	authentication *auth = init_auth(&auth_req);

	return ask_for_auth(auth);
}

authentication *
init_auth(auth_request *auth_req) {
	int max_size = MAX_AUTH_STRING_SIZE;
	authentication *auth = malloc(sizeof(authentication));

	auth->name     = auth_req->name;
	auth->buffer   = calloc(max_size, sizeof(char));
	auth->max_size = max_size;
	auth->attempt  = 0;
	auth->valid    = FALSE;
	auth->op_code  = auth_req->op_code;

	return auth;
}

authentication *
ask_for_auth(authentication *auth) {
	char *user_input = read_auth(auth);

	if (!is_valid_input(user_input)) {
		reattempt_to_auth(auth);
	} else {
		memcpy(auth->buffer, user_input, strlen(user_input) + sizeof('\0'));
		check_auth(auth);
	}

	return auth;
}

char *
read_auth(authentication *auth) {
	char *user_input = NULL;
	memset(auth->buffer, 0, auth->max_size);

	printf("> Type your %s\n", auth->name);
	user_input = read_user_input(auth->max_size);
	auth->attempt = auth->attempt + 1;

	return user_input;
}

void
reattempt_to_auth(authentication *auth) {
	if (auth->attempt < MAX_AUTH_ATTEMPTS) {
		printf("> [%d/%d] Invalid %s, try again\n",
			auth->attempt, MAX_AUTH_ATTEMPTS, auth->name);
		ask_for_auth(auth);
	} else {
		printf("> [%d/%d] Invalid %s, closing connection\n",
			auth->attempt, MAX_AUTH_ATTEMPTS, auth->name);
	}
}

char *
read_user_input(int max_size) {
	char *user_input = calloc(max_size, sizeof(char));

	user_input = fgets(user_input, max_size, stdin);
	user_input = strtok(user_input, "\n");

	return user_input;
}

int
is_valid_input(const char *user_input) {
	return user_input != NULL;
}

void
check_auth(authentication *auth) {
	uint16_t auth_length = strlen(auth->buffer) + sizeof('\0');
	package *pckg = create_package(auth->op_code, 0, auth_length, 
		(uint8_t *) auth->buffer);
	// TODO: change sent status

	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, auth_length);

	if (is_okay(pckg))
		auth->valid = TRUE;

	else
		auth->valid = FALSE;		
}

int
is_okay(package *pckg) {
	return pckg->status == OK;
}


void
print_line(const char* s) {
	putchar('>');
	putchar('\b');
	printf("%s", s);
}

void
show_menu() {
	print_line("Welcome, as an admin of our proxy you can choose to do "
		"one of the following actions:\n");
	printf("\t1. View the proxy's metrics\n"
		   "\t2. Set the message transformation ON or OFF\n"
	   	   "\t3. Pick a program of your choosing to transform the messages\n"
	   	   "\t4. Add or remove a media type to be transformed by our default "
		   		"message transformation program\n"
		   "\t5. View the current list of media types that are being "
		   		"transformed\n"
		   "\t6. Exit\n");
	printf("\b\bType the number of the action that you want to execute and "
		"press enter\n");
}

int
read_menu_option() {
	int menu_option = 0, menu_option_digits = 1;
	int max_size = menu_option_digits + sizeof('\n') + sizeof('\0');
	char *user_input = read_user_input(max_size);

	menu_option = menu_option_to_int(user_input);
	if (!is_valid_menu_option(menu_option))
		return invalid_menu_input();

	return menu_option;
}

int
menu_option_to_int(const char *user_input) {
	int menu_option = 0, i;

	for (i = 0; is_digit(user_input[i]); i++) {
		menu_option += user_input[i] - '0';

		if (user_input[i + 1] != '\0')
			menu_option *= 10;
	}

	if (user_input[i] != '\0')
		return INVALID_INPUT;

	return menu_option;
}

int
is_digit(char c) {
	return c >= '0' && c <= '9';
}

int
is_valid_menu_option(int menu_option) {
	int min_menu_option = 1, max_menu_option = 6;

	return menu_option >= min_menu_option || menu_option <= max_menu_option;
}

int
invalid_menu_input() {
	print_line("Invalid input, remember to type only the number of the menu "
		"option that you want to execute, and to press enter afterwards\n");

	return INVALID_INPUT;
}

void
execute_menu_option(int menu_option) {
	switch (menu_option) {
		case MENU_VIEW_METRICS:
			get_metrics();
			break;

		case MENU_TRANSFORMATION_ON_OFF:
			set_transformation_on_off();
			break;

		case MENU_CHG_TRANSFORMATION:
			change_transformation();
			break;

		case MENU_ADD_REMOVE_MIME:
			add_or_remove_mime();
			break;

		case MENU_LIST_MIME:
			list_mime_types();
			break;

		case MENU_EXIT:
			exit_proxy_management();
			break;

		default:
			print_line("Invalid menu option\n");
			break;
	}
}

void
get_metrics() {
	package *pckg = create_package(METRICS, 0, 0, NULL);
	// TODO: change sent status

	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, MAX_METRICS_SIZE);

	if (is_okay(pckg))
		print_metrics(pckg->content);
	else
		print_line("There was an error and the metrics could not be printed\n");
}

void
print_metrics(uint8_t *metrics) {

}

void
set_transformation_on_off() {
	int transf_option = ask_for_transformation_on_off();

	switch (transf_option) {
		case MENU_TRANSF_ON:
			set_transformation(TRANSF_ON);
			break;

		case MENU_TRANSF_OFF:
			set_transformation(TRANSF_OFF);
			break;

		default:
			invalid_menu_input();
			break;
	}
}

int
ask_for_transformation_on_off() {
	print_line("Would you like to set the message transformation on or off?\n");
	printf("\t1. ON\n"
		   "\t2. OFF\n");
	printf("\b\bType the number of the action that you want to execute and "
		"press enter\n");

	return read_menu_option();
}

void
set_transformation(op_code transformation) {
	package *pckg = create_package(transformation, 0, 0, NULL);
	// TODO: change sent status

	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, 0);

	if (is_okay(pckg))
		printf("> The message transformation was successfully set %s\n",
			transformation == MENU_TRANSF_ON ? "on" : "off");
	else
		print_line("There was an error trying to set the message "
			"transformation\n");
}

void
change_transformation() {
	char *program = ask_for_transformation_program();
	uint16_t program_length = strlen(program) + sizeof('\0');
	package *pckg = create_package(CHG_TRANSF, 0, program_length, 
		(uint8_t *)program);
	// TODO: change sent status

	if (!is_valid_input(program)) {
		print_line("The program path specified is too long\n");
		return;
	}

	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, 0);

	if (is_okay(pckg))
		print_line("The message transformation program was successfully set\n");
	else
		print_line("There was an error setting the transformation program\n");
}

char *
ask_for_transformation_program() {
	int max_size = (2^16) - 1 - sizeof('\0');

	print_line("Type the program's path to be used to transform the messages "
		"below and press enter\n");

	return read_user_input(max_size);
}

void
add_or_remove_mime() {
	int add_or_remove_option = ask_for_add_or_remove_mime();

	switch (add_or_remove_option) {
		case MENU_ADD_MIME:
			send_mime_action(ADD_MIME);
			break;

		case MENU_REMOVE_MIME:
			send_mime_action(REMOVE_MIME);
			break;

		default:
			invalid_menu_input();
			break;
	}
}

int
ask_for_add_or_remove_mime() {
	print_line("Would you like to add or remove a MIME type?\n");
	printf("\t1. ADD\n"
		   "\t2. REMOVE\n");
	printf("\b\bType the number of the action that you want to execute and "
		"press enter\n");

	return read_menu_option();
}

void
send_mime_action(op_code action) {
	package *pckg = create_package(action, 0, 0, NULL); 
	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, 0);

	if (is_okay(pckg))
		printf("> The specified MIME type was successfully %s\n",
			action == MENU_ADD_MIME ? "added" : "removed");
	else
		printf("> There was an error trying to %s the specified MIME type\n",
			action == MENU_ADD_MIME ? "add" : "remove");
}

void
list_mime_types() {
	package *pckg = create_package(LIST_MIMES, 0, 0, NULL); 
	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, MAX_MIME_LIST_SIZE);

	if (is_okay(pckg))
		show_mime_list((char *)pckg->content);
	else
		print_line("There was an error with the message transformation\n");
}

void
show_mime_list(char *mime_list) {
	int max_size    = 128;
	char *mime_type = calloc(max_size, sizeof(char));
	char delimiter  = ',';

	print_line("Here are the MIME types that are being transformed\n");

	for (int i = 0; mime_list[i] != '\0'; i++) {
		if (mime_list[i] != delimiter) {
			mime_type[i] = mime_list[i];
		} else {
			printf("%s\n", mime_type);
			memset(mime_type, 0, max_size);
		}		
	}
}

void
exit_proxy_management() {
	package *pckg = create_package(EXIT, 0, 0, NULL); 
	send_package(admin_socket_fd, pckg);
	pckg = recv_package(admin_socket_fd, 0);

	if (is_okay(pckg) && close_connection(admin_socket_fd))
    	print_line("You are now disconnected from the proxy server\n");
	else
		print_line("There was an error while disconnecting from the proxy "
			"server\n");
}


package *
handle_package(package *pckg) {
    int request_fullfilled = 0;

    switch (pckg->op_code) {
        case USER:
            request_fullfilled = proxy_check_auth(USER, (char *) pckg->content);
            break;

        case PASS:
            request_fullfilled = proxy_check_auth(PASS, (char *) pckg->content);
            break;

        case METRICS:
            //request_fullfilled = get_metrics();
            break;

        case TRANSF_ON:
            request_fullfilled = proxy_set_transformation(TRANSF_ON);
            break;

        case TRANSF_OFF:
            request_fullfilled = proxy_set_transformation(TRANSF_OFF);
            break;

        case CHG_TRANSF:
            request_fullfilled = proxy_change_transf((char *) pckg->content);
            break;

        case ADD_MIME:
            request_fullfilled = mime_do(ADD_MIME);
            break;

        case REMOVE_MIME:
            request_fullfilled = mime_do(REMOVE_MIME);
            break;

        case LIST_MIMES:
            request_fullfilled = true;
            list_mime_types();
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
proxy_set_transformation(uint8_t op_code) {
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
proxy_check_auth(uint8_t auth_type, char *auth_key) {
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
    switch (op_code) {
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
proxy_change_transf(char *program_path) {
    return 1;
}

/*
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
*/


/***************************************
****************************************
*******   FOR PROXY USE ONLY   *********
****************************************
****************************************/


typedef enum {
	ON_HELLO,
	ON_AUTH,
	ON_TRANSACTION,
	ON_EXIT
} admin_session_state;

struct admin_data {

    // Info about the admin client
    struct sockaddr_storage       admin_addr;
    socklen_t                     admin_addr_len;
    int                           admin_fd;

    admin_session_state          session_state;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;
    package *temp_pckg;

    /** siguiente en el pool */
    struct pop3 *next;
};

//  pool de admin_data
static const unsigned    max_pool  = 50;  // tamaño máximo
static unsigned          pool_size = 0;   // tamaño actual
static struct admin_data *pool     = NULL;


/* declaración forward de los handlers de selección de una conexión
** establecida entre un cliente y el proxy.
*/
static void admin_read(struct selector_key *key);
static void admin_write(struct selector_key *key);
static void admin_close(struct selector_key *key);
static const struct fd_handler admin_handler = {
        .handle_read   = admin_read,
        .handle_write  = admin_write,
        .handle_close  = admin_close,
};



void
psmp_accept_connection(struct selector_key *key) {
    struct sockaddr_storage  admin_client_addr;
    socklen_t                admin_client_addr_len = sizeof(admin_client_addr);
    struct admin_data        *admin_info           = NULL;

    const int admin_client = accept(key->fd, (struct sockaddr*) &admin_client_addr,
                             &admin_client_addr_len);

    if (admin_client == -1) {
        goto fail;
    }

    if (selector_fd_set_nio(admin_client) == -1) {
        goto fail;
    }

    admin_info = admin_init(admin_client);

    if (admin_info == NULL) {
        goto fail;
    }

    memcpy(&admin_info->admin_addr, &admin_client_addr, admin_client_addr_len);

    admin_info->admin_addr_len = admin_client_addr_len;

    if (SELECTOR_SUCCESS != selector_register(key->s, admin_client, &admin_handler,
                                             OP_WRITE, state)) {
        goto fail;
    }

    return;

fail:
    if (admin_client != -1)
        close(admin_client);

    admin_destroy(state);
}


static struct admin_data *
admin_init(int client_fd) {
    struct admin_data *admin;

    if (pool == NULL) {
        admin = malloc(sizeof(*admin));

        if (admin == NULL) {
            goto finally;
        }
    } else {
        admin       = pool;
        pool        = pool->next;
        admin->next = 0;
    }

    memset(admin, 0x00, sizeof(*admin));

    // configura la direccion del cliente (MUA)
    admin->admin_fd       = client_fd;
    admin->admin_addr_len = sizeof(admin->client_addr);
    admin->temp_pckg 	  = NULL;

    // configura el estado inicial de la maquina de estados
    admin->session_state = ON_HELLO;

    buffer_init(&admin->read_buffer,  N(admin->raw_buff_a), admin->raw_buff_a);
    buffer_init(&admin->write_buffer, N(admin->raw_buff_b), admin->raw_buff_b);

    admin->references = 1;

finally:
    return admin;
}


static void 
admin_destroy(struct admin_data *data) {
    if (data != NULL) {
        if (pool_size < max_pool) {
            data->next = pool;
            pool       = data;
            pool_size++;
        } else {
		    free(data);
        }
    }
}


void 
admin_read(struct selector_key *key) {
	struct admin_data *data  = ATTACHMENT(key);
	size_t max_pckg_size = 512;
	package *pckg = recv_package(key->fd, max_pckg_size);
	int header_size = OP_CODE_SIZE + STATUS_SIZE + LENGTH_SIZE;
	unpack(pckg, data->raw_buff_a, header_size);

    if (selector_set_interest(key, OP_WRITE) != SELECTOR_SUCCESS)
        pop3_done(key);
}

void 
admin_write(struct selector_key *key) {
	struct admin_data *data  = ATTACHMENT(key);
	package *pckg = handle_package(data->temp_pckg);
	data->temp_pckg = pckg;
	send_package(key->fd, pckg);

	if (selector_set_interest(key, OP_READ) != SELECTOR_SUCCESS)
        pop3_done(key);
}

void 
admin_close(struct selector_key *key) {
	free(ATTACHMENT(key)->temp_pckg);
	free(ATTACHMENT(key));
}