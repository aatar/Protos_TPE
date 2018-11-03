#ifndef ADMIN_H
#define ADMIN_H

#include "psmp_packager.h"

typedef struct authentication authentication;
typedef struct auth_request auth_request;

void
start_interpreter();

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

#endif