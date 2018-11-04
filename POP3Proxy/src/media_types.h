#ifndef PROTOS_TPE_MEDIA_TYPES_H
#define PROTOS_TPE_MEDIA_TYPES_H

#include <unistd.h>
#include <stdbool.h>

void
create_media_types_list();

bool
check_media_type(struct media_types * mt, char * type, char * subtype);

int
add_media_type(char *type, char *subtype);

char * 
get_types_list(char separator);

bool 
is_mime(char *str, char **type, char **subtype);

bool 
delete_media_type(char * type, char * subtype);

#endif
