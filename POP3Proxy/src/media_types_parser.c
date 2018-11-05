#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "media_types_storage.h"
#include "media_types_parser.h"

typedef struct parser_state {
	bool in_type;
    bool in_subtype;
    bool in_error;
} parser_state;

parser_state *
init_parser_state();

parser_state *
process_character_with_state_machine(char c, parser_state *state, 
	char **type_str);

parser_state *
finish_reading_media_type(parser_state *state, char **type_str);

parser_state *
set_state_to_type_state(parser_state *state);

parser_state *
set_state_to_type_state(parser_state *state);

bool 
check_if_full_buffer(size_t extra_space);

bool
could_enlarge_buffer(size_t extra_space);

parser_state *
handle_parsed_media_type(parser_state *state, char **type_str, 
	char **subtype_str);

void
reset_buffer();

parser_state *
finish_reading_type(parser_state *state, char **type_str);

parser_state *
set_state_to_subtype_state(parser_state *state);

parser_state *
read_type_or_subtype(parser_state *state, char c);

void
free_parser_resources(char *buffer, char *type_str, parser_state *state);






char  *buffer      = NULL;
size_t buffer_size = 0;
size_t block_size  = 0;


bool 
parse_media_types(const char *media_range) {
	char c;
	int i 							= 0;
    char *type_str 					= NULL;
    parser_state *state				= init_parser_state();

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
			state = handle_parsed_media_type(state, type_str, &subtype_str);
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

	return buffer != NULL;
}

parser_state *
handle_parsed_media_type(parser_state *state, char **type_str, 
	char **subtype_str) {

    if (add_media_type(*type_str, *subtype_str) == false) {
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

int main(int argc, char const *argv[])
{
	/* code */
	return 0;
}