#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "media_types.h"

#define STR_GROWTH 40
#define MEDIA_BLOCK_SIZE 20


typedef struct parser_state {
	bool in_type;
    bool in_subtype;
    bool in_error;
} parser_state;

char  *buffer       = NULL;
size_t buffer_size = 0;
size_t block_size  = 0;


int 
parse_media_types(const char *media_range) {
	char c;
	int i 							= 0;
    char *type_str 					= NULL;
    parser_state state 				= init_parser_state();

    while (media_range[i] != '\0' && !state->in_error) {
        c = media_range[i];
        process_character_with_state_machine(c, state, &type_str);
        i++;
    }

    if (!state->in_error)
	    state = finish_reading_media_type(state, &type_str);

	free_parser_resources(buffer, type_str, state);

    return state->in_error == false;
}

parser_state *
init_parser_state() {
	parser_state *new_state = malloc(sizeof(parser_state));
	new_state->in_type 	   = true;
	new_state->in_subtype  = false;
	new_state->in_error    = false;

	return new_state;
}

parser_state *
process_character_with_state_machine(char c, parser_state *state, 
	char **type_str) {
	
	switch(c) {
        case ',':
            state = finish_reading_media_type(state, type_str);
            break;

        case '/':
           	state = finish_reading_type(state, type_str);	
            break;

        default:
            state = read_type_or_subtype(state, c);
            break;
    }

    return state;
}

parser_state *
finish_reading_media_type(parser_state *state, char **type_str) {
	char *subtype_str = NULL;

	if (!state->in_subtype || buffer_size == 0) {
		state->in_error = true;
    } else {
		state = set_state_to_type_state(state);

		if (!state->in_error) {
			buffer[buffer_size] = '\0';
			subtype_str = buffer;
			state = handle_parsed_media_type(state, *type_str, subtype_str);
		}
	}

	return state;
}

parser_state *
set_state_to_type_state(parser_state *state) {
	size_t possible_extra_space_needed = 1;
	state->in_error     = check_if_full_buffer(possible_extra_space_needed);
	state->in_type      = true;
	state->in_subtype   = false;

	return state;
}

bool 
check_if_full_buffer(size_t extra_space) {
	if (buffer_size == block_size)
        return could_enlarge_buffer(extra_space);
	else
		return false;
}

bool
could_enlarge_buffer(size_t extra_space) {
	buffer = realloc(buffer, (block_size + extra_space) * sizeof(char));

	return buffer == NULL;
}

parser_state *
handle_parsed_media_type(parser_state *state, char **type_str, 
	char **subtype_str) {

	// TODO add media type to the list
    if (add_media_type(media_type, *type_str, *subtype_str) < 0) {
        state->in_error = true;
    } else {
    	*type_str = NULL;
    	reset_buffer();
    }

    return state;
}

void
reset_buffer() {
	buffer 		= NULL;
    block_size 	= 0;
    buffer_size = 0;
}

parser_state *
finish_reading_type(parser_state *state, char **type_str) {
	if (!state->in_type || buffer_size == 0) {
        state->in_error = true;
    } else {
    	state = set_state_to_subtype_state(state);

    	if (!state->in_error) {
        	buffer[buffer_size] = '\0';
        	*type_str = buffer;
        	reset_buffer();
        }
    }

    return state;
}

parser_state *
set_state_to_subtype_state(parser_state *state) {
	unsigned int possible_extra_space_needed = 1;
	state->in_error     = check_if_full_buffer(possible_extra_space_needed);
	state->in_type      = false;
	state->in_subtype   = true;

    return state;
}

parser_state *
read_type_or_subtype(parser_state *state, char c) {
	size_t possible_extra_space_needed = MEDIA_BLOCK_SIZE;

	if (isspace(c)) {
        state->in_error = true;
	} else {
    	state->in_error = check_if_full_buffer(possible_extra_space_needed);

    	if (!state->in_error) {
	        block_size += MEDIA_BLOCK_SIZE;
	        buffer[buffer_size++] = c;
    	}
    }

    return state;
}

void
free_parser_resources(char *buffer, char *type_str, parser_state *state) {
	free(buffer);
	free(type_str);
	free(state);
}

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

media_types *mt_list = NULL;

void
create_media_types_list() {
    mt_list = malloc(sizeof(struct media_types));

    if (mt_list == NULL)
        return NULL;

    mt_list->first = NULL;
    mt_list->end   = NULL;
    mt_list->size  = 0;
}

bool
check_media_type(const char *type, const char *subtype) {
	bool type_found = false, subtype_found = false;
	media_node *current_mt    = mt_list->first;
	media_node *current_msubt = NULL;

	if (type == NULL || subtype == NULL)
		return false;
	
	if (mt_list->size != 0) {
		type_found = check_for_type(type, &current_mt);

		if (type_found) {
			current_msubt = current_mt->first_subtype;
			subtype_found = check_for_subtype(subtype, &current_msubt);
		}
	}

	return type_found && subtype_found;
}

bool
check_for_type(const char type, media_node **current_mt) {
	bool type_found = false;

	if (current_mt == NULL)
		return false;

	do {
		if (strcmp((*current_mt)->type, type) == 0)
			*type_found = true;
		else
			*current_mt = (*current_mt)->next;
	} while (*current_mt != NULL && !type_found);

	return type_found;
}

bool
check_for_subtype(const char subtype, subtype_node **current_msubt) {
	bool subtype_found = false;

	do {
		if (strcmp((*current_msubt)->name, subtype) == 0)
			subtype_found = true;
		else
			*current_msubt = (*current_msubt)->next;
	} while (*current_msubt != NULL && !subtype_found);

	return subtype_found;
}

int
add_media_type(const char *type, const char *subtype) {
	bool type_found = false, subtype_found = false;
	bool mt_added = false;
	media_node *current_mt    = mt_list->first;
	media_node *current_msubt = NULL;

	if (is_mime(type, subtype)) {
		type_found = check_for_type(type, &current_mt);
		
		if (type_found) {
			current_msubt = current_mt->first_subtype;
			subtype_found = check_for_subtype(subtype, &current_msubt);

			if (!subtype_found)
				mt_added = add_new_subtype(subtype, current_mt);
			else
				mt_added = true;	
		} else {
			mt_added = add_new_media_type(type, subtype);
		}

		if (mt_added)
			mt_list->size++;
	}

	return mt_added;
}

bool
add_new_subtype(const char *subtype, media_node *mt) {
	if (mt == NULL)
		return false;

	subtype_node *new_subtype = malloc(sizeof(subtype_node));

	if (new_subtype == NULL)
		return false;

	new_subtype->name = subtype;
	new_subtype->next = NULL;

	if (mt->first_subtype == NULL || mt->last_subtype == NULL) {
		mt->first_subtype = new_subtype;
		mt->last_subtype  = new_subtype;
	} else {
		mt->last_subtype->next = new_subtype;
	}

	return true;
}

bool
add_new_media_type(const char *type, const char *subtype) {
	bool could_add_new_subtype = false;
	media_node *new_type = malloc(sizeof(media_node));

	if (media_node == NULL || new_subtype == NULL)
		return false;

	media_node->type = type;
	media_node->next = NULL;
	could_add_new_subtype = add_new_subtype(subtype, new_type);

	if (!could_add_new_subtype)
		return false;

	mt_list->last->next = media_node;

	if (mt_list->size == 0) {
		mt_list->first = new_type;
		mt_list->last  = new_type;
	} else {
		mt->last->next = new_subtype;
	}
	
	return true;
}
