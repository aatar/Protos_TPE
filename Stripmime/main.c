#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "contentType.h"

#define FILTER_MEDIAS 	"FILTER_MEDIAS"
#define FILTER_MSG 		"FILTER_MSG"
#define MAX_BOUNDARIES_LENGTH = 1024;

//Delete later
#define HEADER_0 0

#define HEADER_DOT_0 1
#define HEADER_DOT_1 2

#define HEADER_1 3
#define HEADER_1_1 4
#define HEADER_1_2 5
#define HEADER_1_3 6
#define HEADER_1_4 7
#define HEADER_1_LWSP 8
#define HEADER_1_PARENTHESIS 9

#define HEADER_2 10

#define HEADER_3 11
#define HEADER_3_1 12
#define HEADER_3_2 13
#define HEADER_3_3 14
#define HEADER_3_4 15
#define HEADER_3_5 16
#define HEADER_3_6 17
#define HEADER_3_7 18


#define HEADER_4 19
#define HEADER_4_1 20
#define HEADER_4_2 21
#define HEADER_4_3 22

#define HEADER_5 23

#define HEADER_6 24

#define HEADER_7 25

#define HEADER_NOT_COPY_0 26
#define HEADER_NOT_COPY_1 27
#define HEADER_NOT_COPY_2 28
#define HEADER_NOT_COPY_3 29

//Body
#define BODY_0 30

#define BODY_DOT_0 31
#define BODY_DOT_1 32

#define BODY_COPY_0 33
#define BODY_COPY_1 34
#define BODY_COPY_2 35
#define BODY_COPY_3 36
#define BODY_COPY_4 37
#define BODY_COPY_5 38
#define BODY_COPY_6 39

#define BODY_NOT_COPY_0 40
#define BODY_NOT_COPY_1 41
#define BODY_NOT_COPY_2 42
#define BODY_NOT_COPY_3 43
#define BODY_NOT_COPY_4 44
#define BODY_NOT_COPY_5 45
#define BODY_NOT_COPY_6 46

#define FINISHED 47

#define ERROR 48

// Content-type
#define O_CONTENTTYPE 49
#define N1_CONTENTTYPE 50
#define T1_CONTENTTYPE 51
#define E1_CONTENTTYPE 52
#define N2_CONTENTTYPE 53
#define T2_CONTENTTYPE 54
#define UNDERSCORE_CONTENTTYPE 55
#define T3_CONTENTTYPE 56
#define Y_CONTENTTYPE 57
#define P_CONTENTTYPE 58
#define E2_CONTENTTYPE 59
#define FINISHED_CONTENT_TYPE 60
/////////////////////////

// Boundary
#define O_BOUNDARY 61
#define U_BOUNDARY 62
#define N_BOUNDARY 63
#define D_BOUNDARY 64
#define A_BOUNDARY 65
#define R_BOUNDARY 66
#define Y_BOUNDARY 67
#define FINISHED_BOUNDARY 68

// Status
#define COPY 69
#define NOT_COPY 70
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
                if(c == '\n'){
                    printf(".\r\n");
                    state = FINISHED;
                }
                else {
                    putchar('\r');
                    putchar(c);
                    state = HEADER_0;
                }
            break;
            case HEADER_1:
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
            case HEADER_3:
                auxString = stringAppendChar(auxString, c);
                if(matchMime(filter_medias)) { //deberia pasar tambien c, pero como es global puede leer su valor
                    free(auxString);
                    auxString = malloc(1);
                    auxString[0] = 0;
                    status = NOT_COPY;
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
                if(c == '\n'){
                    printf(".\r\n");
                    state = FINISHED;
                }
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
            default:
                // Do nothing
            break;
        }
    }

    ree(auxString);
    free(filter_medias_copy);
    free(auxStringBoundary);
    free(boundary);
    for(int i = 0; i < boundaries_length; i++)
        free(boundaries[i]);

    if(state == ERROR) {
        fprintf(stderr, "-ERR The email has one or more errors.\r\n");
        return -1;
    }

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

int isContentType() {
    int contentTypeState = O_CONTENTTYPE;
    while (contentTypeState != FINISHED_CONTENT_TYPE) {
        c = getchar();
        auxString = stringAppendChar(auxString, c);
        switch(contentTypeState) {
            case O_CONTENTTYPE:
                if(toupper(c) == 'N') contentTypeState = N1_CONTENTTYPE;
                else return 0;
            break;
            case N1_CONTENTTYPE:
                if(toupper(c) == 'T') contentTypeState = T1_CONTENTTYPE;
                else return 0;
            break;
            case T1_CONTENTTYPE:
                if(toupper(c) == 'E') contentTypeState = E1_CONTENTTYPE;
                else return 0;
            break;
            case E1_CONTENTTYPE:
                if(toupper(c) == 'N') contentTypeState = N2_CONTENTTYPE;
                else return 0;
            break;
            case N2_CONTENTTYPE:
                if(toupper(c) == 'T') contentTypeState = T2_CONTENTTYPE;
                else return 0;
            break;
            case T2_CONTENTTYPE:
                if(c == '-') contentTypeState = UNDERSCORE_CONTENTTYPE;
                else return 0;
            break;
            case UNDERSCORE_CONTENTTYPE:
                if(toupper(c) == 'T') contentTypeState = T3_CONTENTTYPE;
                else return 0;
            break;
            case T3_CONTENTTYPE:
                if(toupper(c) == 'Y') contentTypeState = Y_CONTENTTYPE;
                else return 0;
            break;
            case Y_CONTENTTYPE:
                if(toupper(c) == 'P') contentTypeState = P_CONTENTTYPE;
                else return 0;
            break;
            case P_CONTENTTYPE:
                if(toupper(c) == 'E') contentTypeState = E2_CONTENTTYPE;
                else return 0;
            break;
            case E2_CONTENTTYPE:
                if(c == ':') contentTypeState = FINISHED_CONTENT_TYPE;
                else return 0;
            break;
        }
    }
    return 1;
}

int isBoundary() {
    int boundaryState = O_BOUNDARY;
    while (boundaryState != FINISHED_BOUNDARY) {
        c = getchar();
        if(status == COPY) auxString = stringAppendChar(auxString, c);
        switch(boundaryState) {
            case O_BOUNDARY:
                if(toupper(c) == 'U') boundaryState = U_BOUNDARY;
                else return 0;
            break;
            case U_BOUNDARY:
                if(toupper(c) == 'N') boundaryState = N_BOUNDARY;
                else return 0;
            break;
            case N_BOUNDARY:
                if(toupper(c) == 'D') boundaryState = D_BOUNDARY;
                else return 0;
            break;
            case D_BOUNDARY:
                if(toupper(c) == 'A') boundaryState = A_BOUNDARY;
                else return 0;
            break;
            case A_BOUNDARY:
                if(toupper(c) == 'R') boundaryState = R_BOUNDARY;
                else return 0;
            break;
            case R_BOUNDARY:
                if(toupper(c) == 'Y') boundaryState = Y_BOUNDARY;
                else return 0;
            break;
            case Y_BOUNDARY:
                if(toupper(c) == '=') boundaryState = FINISHED_BOUNDARY;
                else return 0;
            break;
        }
    }
    return 1;
}

int matchBoundary() {
    if(boundaries_length == 0)
        return 0;
    char *last_boundary = boundaries[boundaries_length - 1];
    int index = 0;
    
    while(c != '\r' && c != '-') {
        if(status == COPY) putchar(c);
        else auxStringBoundary = stringAppendChar(auxStringBoundary, c);

        if(c != last_boundary[index++]) return 0;
        c = getchar();
    }
    last_char = c;
    return last_boundary[index] == 0;
}