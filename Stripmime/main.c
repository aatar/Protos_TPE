#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "main.h"

#define FILTER_MEDIAS 	"FILTER_MEDIAS"
#define FILTER_MSG 		"FILTER_MSG"
#define MAX_BOUNDARIES_LENGTH = 1024;

//Delete later
#define HEADER_0 0

#define HEADER_DOT_0 0
#define HEADER_DOT_1 0

#define HEADER_1 1
#define HEADER_1_1 1
#define HEADER_1_2 1
#define HEADER_1_3 1
#define HEADER_1_4 1
#define HEADER_1_LWSP 1
#define HEADER_1_PARENTHESIS 1

#define HEADER_2 1

#define HEADER_3 1
#define HEADER_3_1 1
#define HEADER_3_2 1
#define HEADER_3_3 1
#define HEADER_3_4 1
#define HEADER_3_5 1
#define HEADER_3_6 1
#define HEADER_3_7 1


#define HEADER_4 1
#define HEADER_4_1 1
#define HEADER_4_2 1
#define HEADER_4_3 1

#define HEADER_5 1

#define HEADER_6 1

#define HEADER_7 1

#define HEADER_NOT_COPY_0 0
#define HEADER_NOT_COPY_1 0
#define HEADER_NOT_COPY_2 0
#define HEADER_NOT_COPY_3 0

//Body
#define BODY_0 0

#define BODY_DOT_0 0
#define BODY_DOT_1 0

#define BODY_COPY_0 0
#define BODY_COPY_1 0
#define BODY_COPY_2 0
#define BODY_COPY_3 0
#define BODY_COPY_4 0
#define BODY_COPY_5 0
#define BODY_COPY_6 0

#define BODY_NOT_COPY_0 0
#define BODY_NOT_COPY_1 0
#define BODY_NOT_COPY_2 0
#define BODY_NOT_COPY_3 0
#define BODY_NOT_COPY_4 0
#define BODY_NOT_COPY_5 0
#define BODY_NOT_COPY_6 0

#define FINISHED 0

#define ERROR 0


// Status
#define COPY 0
#define NOT_COPY 0
//////////////////////////////

char* stringAppendChar (char *str, char c);
int isContentType();
int isBoundary();
int matchMime(char *filter_medias);
int matchBoundary();

char c, last_char;
int status = COPY;
int state = HEADER_0;
int get_next = 1;
char *auxString;
char *auxStringBoundary;
char *boundary;
char *boundaries[MAX_BOUNDARIES_LENGTH];
int boundaries_length = 0;

int main(int argc, char const *argv[]) {

    char *filter_medias = getenv(FILTER_MEDIAS);

    if (filter_medias == NULL) {
        fprintf(stderr, "-ERR No filter medias specified.\r\n");
        return -1;
    }

    char *filter_msg = getenv(FILTER_MSG);

    if (filter_msg == NULL) {
        filter_msg = "This part of the email was replaced because of the MIME Type.";
    }

    char *filter_medias_copy = malloc(strlen(filter_medias) + 1);
    if (filter_medias_copy == NULL) {
        return -1;
    }
    strcpy(filter_medias_copy, filter_medias);

    
    auxString = malloc(1);
    auxString[0] = 0;

    auxStringBoundary = malloc(1);
    auxStringBoundary[0] = 0;

    boundary = malloc(1);
    boundary[0] = 0;

    while(state != FINISHED && state != ERROR) {
        if(get_next) c = getchar();
        else get_next = 1;
        switch(state) {
            case HEADER_0:
                if(status == COPY) {
                    if(c == 'c' || c == 'C') {
                        auxString = stringAppendChar(auxString, c);
                        state = HEADER_1;
                    }
                    else if(c == '.') {
                        state = HEADER_DOT_0;
                    }
                    else {
                        get_next = 0;
                        state = HEADER_1_1;
                    }
                }
                else {
                    state = HEADER_NOT_COPY_0;
                }
            break;
            case HEADER_DOT_0:
                if(c == '\r') state = HEADER_DOT_1;
                else {
                    putchar(c);
                    state = HEADER_0;
                }
            break;
            case HEADER_DOT_1:
                if(c == '\n') state = FINISHED;
                else {
                    putchar('\r');
                    putchar(c);
                    state = HEADER_0;
                }
            break;
            case HEADER_1: //ahora la c deberia valer O u o para que tenga chances de ser content-type
                auxString = stringAppendChar(auxString, c);
                if((c == 'o'|| c == 'O') && isContentType()) state = HEADER_2;
                else {
                    printf( "%s", auxString );
                    free(auxString);
                    auxString = malloc(1);
                    auxString[0] = 0;
                    state = HEADER_1_1;
                }
            break;
            case HEADER_1_1:
                if(c == '\r') {
                    putchar(c);
                    state = HEADER_1_2;
                }
                else if(c == '(') {
                    state = HEADER_1_PARENTHESIS;
                }
                else {
                    putchar(c);
                }
            break;
            case HEADER_1_2:
                if(c == '\n') {
                    putchar(c);
                    state = HEADER_1_3;
                }
                else if(c == '(') {
                    state = HEADER_1_PARENTHESIS;
                }
                else {
                    putchar(c);
                    state = HEADER_1_1;
                }
            break;
            case HEADER_1_3:
                if(c == '\r') {
                    putchar(c);
                    state = HEADER_1_4;
                }
                else if(isspace(c) && c != '\r' && c != '\n') {
                    putchar(c);
                    state = HEADER_1_LWSP;
                }
                else {
                    get_next = 0;
                    state = HEADER_0;
                }
            break;
            case HEADER_1_4:
                if(c == '\n') {
                    putchar(c);
                    state = BODY_0;
                }
                else {
                    get_next = 0;
                    state = HEADER_1_3;
                }
            break;
            case HEADER_1_LWSP:
                if(isspace(c) && c != '\r' && c != '\n') {
                    putchar(c);
                }
                else if(c == '(') {
                    state = HEADER_1_PARENTHESIS;
                }
                else {
                    putchar(c);
                    state = HEADER_1_1;
                }
            break;
            case HEADER_1_PARENTHESIS:
                if(c == ')') {
                    state = HEADER_1_1;
                }
                // Else, do nothing
            break;
            case HEADER_2:
                auxString = stringAppendChar(auxString, c);
                if(! (isspace(c) && c != '\r' && c != '\n')) { //it's not a white space
                    get_next = 0;
                    state = HEADER_3;
                }
            break;
            case HEADER_3: //en c esta guardado el primer caracter del content type
                auxString = stringAppendChar(auxString, c);
                if(matchMime(filter_medias)) { //deberia pasar tambien c, pero como es global puede leer su valor
                    free(auxString);
                    auxString = malloc(1);
                    auxString[0] = 0;
                    state = HEADER_4;
                }
                else {
                    state = HEADER_3_1;
                }
            break;
            case HEADER_3_1:
                auxString = stringAppendChar(auxString, c);
                if(c == 'b' || c == 'B') {
                    state = HEADER_3_2;
                }
                else if(c == '\r') {
                    state = HEADER_3_5;
                }
                // Else, do nothing
            break;
            case HEADER_3_2:
                auxString = stringAppendChar(auxString, c);
                if((c == 'o'|| c == 'O') && isBoundary()) state = HEADER_3_3;
                else {
                    state = HEADER_3_1;
                }
            break;
            case HEADER_3_3:
                auxString = stringAppendChar(auxString, c);
                if(c == '\"') {
                    free(boundary);
                    boundary = malloc(1);
                    boundary[0] = 0;
                    state = HEADER_3_4;
                }
            break;
            case HEADER_3_4:
                auxString = stringAppendChar(auxString, c);
                if(c == '\"') {
                    char *boundary_aux = malloc(strlen(boundary) + 1);
                    strcpy(boundary_aux, boundary);
                    boundary_aux[strlen(boundary)] = 0;
                    boundaries[boundaries_length++] = boundary_aux;
                    
                    free(boundary);
                    boundary = malloc(1);
                    boundary[0] = 0;

                    state = HEADER_3_1;
                }
                else {
                    boundary = stringAppendChar(boundary, c);
                }
            break;
            case HEADER_3_5:
                auxString = stringAppendChar(auxString, c);
                if(c == '\n') {
                    printf( "%s", auxString );
                    free(auxString);
                    auxString = malloc(1);
                    auxString[0] = 0;
                    state = HEADER_3_6;
                }
                else {
                    state = HEADER_3_1;
                }
            break;
            case HEADER_3_6:
                if(c == '\r') {
                    putchar(c);
                    state = HEADER_3_7;
                }
                else {
                    get_next = 0;
                    state = HEADER_0;
                }
            break;
            case HEADER_3_7:
                if(c == '\n') {
                    putchar(c);
                    state = BODY_0;
                }
                else {
                    putchar(c);
                    state = HEADER_3_6;
                }
            break;
            case HEADER_4:
                if(c == 'b' || c == 'B'){
                    state = HEADER_5;
                }
                else if(c == '\r') {
                    state = HEADER_4_1;
                }
                // Else, do nothing
            break;
            case HEADER_4_1:
                if(c == '\n') {
                    status = NOT_COPY;
                    state = HEADER_4_2;
                }
                else {
                    state = HEADER_4;
                }
            break;
            case HEADER_4_2:
                if(c == '\r') {
                    state = HEADER_4_3;
                }
                else {
                    get_next = 0;
                    state = HEADER_0;
                }
            break;
            case HEADER_4_3:
                if(c == '\n') {
                    state = BODY_0;
                }
                else {
                    get_next = 0;
                    state = HEADER_0;
                }
            break;
            case HEADER_5:
                if((c == 'o'|| c == 'O') && isBoundary()) state = HEADER_6;
                else {
                    state = HEADER_4;
                }
            break;
            case HEADER_6:
                if(c == '\"') {
                    free(boundary);
                    boundary = malloc(1);
                    boundary[0] = 0;
                    state = HEADER_7;
                }
            break;
            case HEADER_7:
                if(c == '\"') {
                    char *boundary_aux = malloc(strlen(boundary) + 1);
                    strcpy(boundary_aux, boundary);
                    boundary_aux[strlen(boundary)] = 0;
                    boundaries[boundaries_length++] = boundary_aux;
                    
                    free(boundary);
                    boundary = malloc(1);
                    boundary[0] = 0;

                    state = HEADER_4;
                }
                else {
                    boundary = stringAppendChar(boundary, c);
                }
            break;
            case HEADER_NOT_COPY_0:
                if(c == '\r') state = HEADER_NOT_COPY_1;
            break;
            case HEADER_NOT_COPY_1:
                if(c == '\n') state = HEADER_NOT_COPY_2;
                else state = HEADER_NOT_COPY_0;
            break;
            case HEADER_NOT_COPY_2:
                if(c == '\r') state = HEADER_NOT_COPY_3;
                else state = HEADER_NOT_COPY_0;
            break;
            case HEADER_NOT_COPY_3:
                if(c == '\n') state = BODY_0;
                else state = HEADER_NOT_COPY_0;
            break;
            case BODY_0:
                if(status == COPY) {
                    if(c == '.') {
                        state = BODY_DOT_0;
                    }
                    else {
                        get_next = 0;
                        state = BODY_COPY_0;
                    }
                }
                else {
                    get_next = 0;
                    state = BODY_NOT_COPY_0;
                }
            break;
            case BODY_DOT_0:
                if(c == '\r') state = BODY_DOT_1;
                else {
                    putchar(c);
                    state = BODY_0;
                }
            break;
            case BODY_DOT_1:
                if(c == '\n') state = FINISHED;
                else {
                    putchar('\r');
                    putchar(c);
                    state = BODY_0;
                }
            break;
            case BODY_COPY_0:
                putchar(c);
                if(c == '-') {
                    state = BODY_COPY_1;
                }
            break;
            case BODY_COPY_1:
                putchar(c);
                if(c == '-') state = BODY_COPY_2;
                else state = BODY_COPY_0;
            break;
            case BODY_COPY_2:
                if(!matchBoundary()) state = ERROR;
                else {
                    putchar(last_char);
                    if(last_char == '-') state = BODY_COPY_4;
                    else state = BODY_COPY_3; //last char was \r
                }
            break;
            case BODY_COPY_3:
                if(c == '\n') {
                    putchar(c);
                    state = HEADER_0;
                }
                else state = ERROR;
            break;
            case BODY_COPY_4:
                if(c == '-') {
                    putchar(c);
                    free(boundaries[boundaries_length-1]);
                    boundaries_length--;
                    state = BODY_COPY_5;
                }
                else state = ERROR;
            break;
            case BODY_COPY_5:
                putchar(c);
                if(c == '\r') state = BODY_COPY_6;
            break;
            case BODY_COPY_6:
                putchar(c);
                if(c == '\n') state = BODY_0;
                else state = BODY_COPY_5;
            break;
            case BODY_NOT_COPY_0:
                if(c == '-') {
                    free(auxStringBoundary);
                    auxStringBoundary = malloc(2);
                    auxStringBoundary[0] = '-';
                    auxStringBoundary[1] = 0;
                    state = BODY_NOT_COPY_1;
                }
                // Else, do nothing
            break;
            case BODY_NOT_COPY_1:
                auxStringBoundary = stringAppendChar(auxStringBoundary, c);
                if(c == '-') {
                    state = BODY_NOT_COPY_2;
                }
                else {
                    free(auxStringBoundary);
                    auxStringBoundary = malloc(1);
                    auxStringBoundary[0] = 0;
                    state = BODY_NOT_COPY_0;
                }
            break;
            case BODY_NOT_COPY_2:
                if(!matchBoundary()) state = ERROR;
                else {
                    auxStringBoundary = stringAppendChar(auxStringBoundary, last_char);
                    if(last_char == '-') state = BODY_NOT_COPY_4;
                    else state = BODY_NOT_COPY_3; //last char was \r
                }
            break;
            case BODY_NOT_COPY_3:
                if(c == '\n') {
                    auxStringBoundary = stringAppendChar(auxStringBoundary, c);
                    printf("Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n%s\r\n\r\n", filter_msg);
                    status = COPY;
                    printf( "%s", auxStringBoundary );
                    free(auxStringBoundary);
                    auxStringBoundary = malloc(1);
                    auxStringBoundary[0] = 0;
                    state = HEADER_0;
                }
                else state = ERROR;
            break;
            case BODY_NOT_COPY_4:
                if(c == '-') {
                    free(boundaries[boundaries_length-1]);
                    boundaries_length--;
                    state = BODY_NOT_COPY_5;
                }
                else state = ERROR;
            break;
            case BODY_NOT_COPY_5:
                if(c == '\r') state = BODY_NOT_COPY_6;
            break;
            case BODY_NOT_COPY_6:
                if(c == '\n') {
                    printf("Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n%s\r\n\r\n", filter_msg);
                    status = COPY;
                    state = BODY_0;
                }
                else state = BODY_NOT_COPY_5;
            break;
        }
    }

    free(auxString);
    return 0;
}

char* stringAppendChar (char *str, char c) {
    size_t len = strlen(str);
    char *ret = malloc(len + 2 ); /* one for extra char, one for trailing zero */
    strcpy(ret, str);
    ret[len] = c;
    ret[len + 1] = '\0';
    free(str);
    return ret;
}