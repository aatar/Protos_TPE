#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "media_types.h"

#define STR_GROWTH 40

struct media_types * new_media_types(){
    struct media_types * ret = malloc(sizeof(struct media_types));
    if (ret == NULL)
        return NULL;
    ret->first = NULL;
    ret->end   = NULL;
    ret->size  = 0;
    return ret;
}

