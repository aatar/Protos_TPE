#ifndef MEDIA_TYPES_STORAGE_H
#define MEDIA_TYPES_STORAGE_H

#include <unistd.h>
#include <stdbool.h>

#define MEDIA_BLOCK_SIZE 20

bool
create_media_types_list();

bool
check_media_type(const char *type, const char *subtype);

bool
add_media_type(const char *type, const char *subtype);

char * 
get_media_types_list();

//bool 
//is_mime(char *str, char **type, char **subtype);

bool 
delete_media_type(char *type, char *subtype);

#endif