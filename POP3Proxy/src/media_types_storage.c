#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "media_types_storage.h"

typedef struct media_types media_types;

typedef struct media_node media_node;

typedef struct subtype_node subtype_node;


typedef struct media_types {
    size_t      size;
    media_node *first;
    media_node *last;
} media_types;

typedef struct media_node {
    char         *type;
    media_node   *next;
    subtype_node *first_subtype;
    subtype_node *last_subtype;
} media_node;

typedef struct subtype_node {
    char         *name;
    subtype_node *next;
} subtype_node;

bool
check_for_type(const char *type, media_node **current_mt);

bool
check_for_subtype(const char *subtype, subtype_node **current_msubt);

bool
add_new_subtype(const char *subtype, media_node *mt);

void
check_if_empty_msubt_list(media_node *mt, subtype_node *new_subtype);

bool
add_new_media_type(const char *type, const char *subtype);

void
check_if_empty_mt_list(media_node *new_type);

char *
get_all_media_types_from_type(media_node *mt);

char *
attempt_to_insert_str_into_buffer(char *str, char *buffer, double *buffer_size, 
	size_t *list_size);

char *
get_possibly_enlarged_buffer(size_t list_size, size_t str_len, 
	double *buffer_size, char *buffer);

char *
create_media_type_str(char *type, size_t type_str_len, char *subtype);

void
insert_mt_into_buffer(char *buffer, size_t *list_size, char *str, 
	size_t str_len);

bool
find_type(char *type, media_node **type_node, media_node **prev_type);

bool
find_subtype(char *subtype, media_node *current_mt, subtype_node **subt_node,
	subtype_node **prev_subtype);

void
free_mt_resources(media_node *mt, media_node *prev_mt, subtype_node *msubt,
	subtype_node *prev_msubt);

void
free_subtype_resources(subtype_node *msubt, subtype_node *prev_msubt, 
	media_node *parent_type);

void
free_type_resources(media_node *mt, media_node *prev_mt);

bool
free_all_subtypes(char *type);

bool
free_all_types();




media_types *mt_list = NULL;

bool
create_media_types_list() {
    mt_list = malloc(sizeof(struct media_types));

    if (mt_list == NULL)
        return false;

    mt_list->first = NULL;
    mt_list->last   = NULL;
    mt_list->size  = 0;

    return true;
}

bool
check_media_type(const char *type, const char *subtype) {
	bool type_found = false, subtype_found = false;
	media_node *current_mt    = NULL;
	subtype_node *current_msubt = NULL;

	if (type == NULL || subtype == NULL)
		return false;		
	
	if (mt_list != NULL && mt_list->size != 0) {
		current_mt = mt_list->first;
		type_found = check_for_type(type, &current_mt);

		if (type_found) {
			current_msubt = current_mt->first_subtype;
			subtype_found = check_for_subtype(subtype, &current_msubt);
		}
	}

	return type_found && subtype_found;
}

bool
check_for_type(const char *type, media_node **current_mt) {
	bool type_found = false;

	if (current_mt == NULL)
		return false;

	do {
		if (strcmp((*current_mt)->type, type) == 0)
			type_found = true;
		else
			*current_mt = (*current_mt)->next;
	} while (*current_mt != NULL && !type_found);

	return type_found;
}

bool
check_for_subtype(const char *subtype, subtype_node **current_msubt) {
	bool subtype_found = false;

	do {
		if (strcmp((*current_msubt)->name, subtype) == 0)
			subtype_found = true;
		else
			*current_msubt = (*current_msubt)->next;
	} while (*current_msubt != NULL && !subtype_found);

	return subtype_found;
}

bool
add_media_type(const char *type, const char *subtype) {
	bool type_found = false, subtype_found = false;
	bool mt_added = false, mt_already_existed = false;
	media_node *current_mt    = mt_list->first;
	subtype_node *current_msubt = NULL;

	//TODO implement is_mime using an external file that will have all the MIMES
	//if (is_mime(type, subtype)) {
		type_found = check_for_type(type, &current_mt);
		
		if (type_found) {
			current_msubt = current_mt->first_subtype;
			subtype_found = check_for_subtype(subtype, &current_msubt);

			if (!subtype_found)
				mt_added = add_new_subtype(subtype, current_mt);
			else
				mt_already_existed = true;	
		} else {
			mt_added = add_new_media_type(type, subtype);
		}

		if (!mt_already_existed && mt_added)
			mt_list->size++;
	//}

	return !mt_already_existed && mt_added;
}

bool
add_new_subtype(const char *subtype, media_node *mt) {
	subtype_node *new_subtype = malloc(sizeof(subtype_node));
	char subtype_name[128] = {0};

	if (mt == NULL || new_subtype == NULL)
		return false;

	new_subtype->name = strcpy(subtype_name, subtype);
	new_subtype->next = NULL;
	check_if_empty_msubt_list(mt, new_subtype);

	return true;
}

void
check_if_empty_msubt_list(media_node *mt, subtype_node *new_subtype) {
	if (mt->first_subtype == NULL || mt->last_subtype == NULL) {
		mt->first_subtype = new_subtype;
		mt->last_subtype  = new_subtype;
	} else {
		mt->last_subtype->next = new_subtype;
	}
}

bool
add_new_media_type(const char *type, const char *subtype) {
	bool could_add_new_subtype = false;
	media_node *new_type = malloc(sizeof(media_node));
	char type_name[128] = {0};

	if (new_type == NULL)
		return false;

	new_type->type = strcpy(type_name, type);
	new_type->next = NULL;
	could_add_new_subtype = add_new_subtype(subtype, new_type);

	if (!could_add_new_subtype)
		return false;

	mt_list->last->next = new_type;
	check_if_empty_mt_list(new_type);
	
	return true;
}

void
check_if_empty_mt_list(media_node *new_type) {
	if (mt_list->size == 0) {
		mt_list->first = new_type;
		mt_list->last  = new_type;
	} else {
		mt_list->last->next = new_type;
	}
}

char *
get_media_types_list() {
	size_t max_media_type_str_size	= 128;
	char *types_list 	   			= NULL;
	char *mt_buffer 				= calloc(mt_list->size, 
									  max_media_type_str_size);
	double mt_buffer_size 			= mt_list->size * max_media_type_str_size;
	size_t mt_list_size 			= 0;
	media_node *current_mt 			= NULL;

	if (mt_list->size == 0) 
		return NULL;

	current_mt = mt_list->first;

	while (current_mt != NULL) {
		types_list = get_all_media_types_from_type(current_mt);

		if (types_list == NULL)
			return NULL;

		mt_buffer = attempt_to_insert_str_into_buffer(types_list, mt_buffer, 
					&mt_buffer_size, &mt_list_size);

		if (mt_buffer == NULL)
			return NULL;

		current_mt = current_mt->next;
	}

	if (mt_list_size != 0)
		types_list[mt_list_size - 1] = '\0';
		// replaces the ',' after the last media type inserted

	return mt_buffer;
}

char *
get_all_media_types_from_type(media_node *mt) {
	size_t max_subtypes_for_type  	= 256;
	size_t max_media_type_str_size	= 128;
	double subtypes_buffer_size		= max_subtypes_for_type * 
									  max_media_type_str_size * sizeof(',');
	char *subtypes_buffer 		  	= calloc(max_subtypes_for_type, 
									  max_media_type_str_size * sizeof(','));	
	subtype_node *current_msubt 	= mt->first_subtype;
	size_t type_str_len 			= strlen(mt->type);
	char *media_type_str 			= NULL;
	size_t sublist_size 			= 0;

	if (subtypes_buffer == NULL)
		return NULL;	

	while (current_msubt != NULL) {
		media_type_str  = create_media_type_str(mt->type, type_str_len, 
						  current_msubt->name);

		if (media_type_str == NULL)
			return NULL;
			
		subtypes_buffer = attempt_to_insert_str_into_buffer(media_type_str, 
						  subtypes_buffer, &subtypes_buffer_size, 
						  &sublist_size);

		if (subtypes_buffer == NULL)
			return NULL;

		current_msubt 	= current_msubt->next;
	}

	if (sublist_size != 0)
	subtypes_buffer[sublist_size - 1] = '\0';
		// replaces the ',' after the last media type inserted

	return subtypes_buffer;
}

char *
attempt_to_insert_str_into_buffer(char *str, char *buffer, double *buffer_size, 
	size_t *list_size) {

	size_t str_len = strlen(str);
	buffer = get_possibly_enlarged_buffer(*list_size, str_len, buffer_size, 
			 buffer);

	if (buffer != NULL)
		insert_mt_into_buffer(buffer, list_size, str, str_len);

	return buffer;
}

char *
get_possibly_enlarged_buffer(size_t list_size, size_t str_len, 
	double *buffer_size, char *buffer) {

	double extra_space 				= *buffer_size * 0.3;
	bool sublist_too_big_for_buffer = (list_size + str_len + 1) >= *buffer_size;

	while (sublist_too_big_for_buffer) {
		*buffer_size = *buffer_size + extra_space;
		buffer = realloc(buffer, *buffer_size);
		memset(buffer + (size_t) (*buffer_size - extra_space), 0, 
			(*buffer_size - extra_space));
		sublist_too_big_for_buffer = (list_size + str_len + sizeof(char)) >
								 	 *buffer_size;
	}

	return buffer;
}

char *
create_media_type_str(char *type, size_t type_str_len, char *subtype) {
	size_t max_media_type_str_size	= 128;
	char *media_type_str 		  	= calloc(max_media_type_str_size, 
		sizeof(char));

	if (media_type_str != NULL) {
		media_type_str = strcpy(media_type_str, type);
		media_type_str[type_str_len] = '/';
		media_type_str = strcpy(media_type_str + type_str_len, subtype);
	}

	return media_type_str;
}

void
insert_mt_into_buffer(char *buffer, size_t *list_size, char *str, 
	size_t str_len) {
	
	buffer = strcpy(buffer + *list_size, str);
	*list_size = *list_size + str_len;
	buffer[*list_size] = ',';
	*list_size = *list_size + sizeof(',');
}

bool
delete_media_type(char *type, char *subtype) {
	bool type_found = false, subtype_found = false;
	media_node *type_node = NULL, *prev_type = NULL;
	subtype_node *subt_node = NULL, *prev_subtype = NULL;
	bool could_delete_all = false;

	if (type == NULL || subtype == NULL)
		return false;

	if (strcmp("*", subtype) == 0) {
		if (strcmp("*", type) == 0)
			could_delete_all = free_all_types();
		else
			could_delete_all = free_all_subtypes(type);

		return could_delete_all;
	}

	type_found = find_type(type, &type_node, &prev_type);

	if (type_found)
		subtype_found = find_subtype(subtype, type_node, &subt_node, 
						&prev_subtype);

	if (type_found && subtype_found)
		free_mt_resources(type_node, prev_type, subt_node, prev_subtype);
		
	return type_found && subtype_found;
}

bool
find_type(char *type, media_node **type_node, media_node **prev_type) {
	media_node *current_mt = NULL;
	bool type_found = false;

	if (mt_list != NULL && mt_list->size != 0) {
		current_mt = mt_list->first;
		*prev_type = NULL;
	}

	while (!type_found && current_mt != NULL) {
		type_found = strcmp(type, current_mt->type) == 0;

		if (!type_found) {
			*prev_type = current_mt;
			current_mt = current_mt->next;
		}
	}

	if (type_found)
		*type_node = current_mt;

	return type_found;
}

bool
find_subtype(char *subtype, media_node *current_mt, subtype_node **subt_node,
	subtype_node **prev_subtype) {

	subtype_node *current_msubt = NULL;
	bool subtype_found = false;

	if (current_mt != NULL) {
		current_msubt = current_mt->first_subtype;
		*prev_subtype = NULL;
	}

	while (!subtype_found && current_msubt != NULL) {
		subtype_found = strcmp(subtype, current_msubt->name) == 0;

		if (!subtype_found) {
			*prev_subtype = current_msubt;
			current_msubt = current_msubt->next;
		}
	}

	if (subtype_found)
		*subt_node = current_msubt;

	return subtype_found;
}

void
free_mt_resources(media_node *mt, media_node *prev_mt, subtype_node *msubt,
	subtype_node *prev_msubt) {

	char *first_subt = mt->first_subtype->name;
	char *last_subt  = mt->last_subtype->name;
	bool type_has_only_one_subtype = strcmp(first_subt, last_subt) == 0;

	free_subtype_resources(msubt, prev_msubt, mt);

	if (type_has_only_one_subtype)
		free_type_resources(mt, prev_mt);
}

void
free_subtype_resources(subtype_node *msubt, subtype_node *prev_msubt, 
	media_node *parent_type) {

	if (prev_msubt == NULL) // This is the first subtype node of the type node
		parent_type->first_subtype = msubt->next;

	free(msubt->name);
	free(msubt);
	mt_list->size = mt_list->size - 1;
}

void
free_type_resources(media_node *mt, media_node *prev_mt) {
	if (prev_mt == NULL)
		mt_list->first = mt->next;

	free(mt->type);
	free(mt);

	if (mt_list->first == NULL)
		mt_list->last = NULL;
}

bool
free_all_subtypes(char *type) {
	media_node *type_node = NULL, *prev_type = NULL;
	subtype_node *current_msubt = NULL, *msubt_to_be_freed = NULL;
	bool type_found = find_type(type, &type_node, &prev_type);

	if (!type_found)
		return false;

	current_msubt = type_node->first_subtype;

	while (current_msubt != NULL) {
		msubt_to_be_freed = current_msubt;
		current_msubt = current_msubt->next;
		free_subtype_resources(msubt_to_be_freed, NULL, type_node);
	}

	free_type_resources(type_node, prev_type);

	return true;
}

bool
free_all_types() {
	media_node *current_mt = mt_list->first, *mt_to_be_freed = NULL;
	bool could_delete_subtypes = true;

	while (current_mt != NULL) {
		mt_to_be_freed = current_mt;
		current_mt = current_mt->next;
		could_delete_subtypes = free_all_subtypes(mt_to_be_freed->type);

		if (!could_delete_subtypes)
			return false;

		free_type_resources(mt_to_be_freed, NULL);
	}

	return true;
}
