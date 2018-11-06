#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <memory.h>
#include <netdb.h>
#include <limits.h>
#include <errno.h>

#include "input_data.h"
#include "media_types.h"

input_data params;

void print_version() {
    printf("Proxy POP3 version %s", params->version);
}

void print_help(){
    printf("Uso: pop3filter [OPTION] <servidor-origen>\n");
    printf("Proxy POP3 que filtra mensajes de <origin-server>.\n");
    printf("\n");
    printf("Opciones:\n");
    printf("%-30s","\t-e archivo-de-error");
    printf("especifica el archivo de error donde se redirecciona stderr de las "
                   "ejecuciones de los filtros\n");
    printf("%-30s", "\t-h");
    printf("imprime la ayuda y termina\n");
    printf("%-30s", "\t-l direccion_pop3");
    printf("establece la dirección donde servirá el proxy\n");
    printf("%-30s", "\t-L direccion_management");
    printf("establece la dirección donde servirá el servicio de management\n");
    printf("%-30s", "\t-m mensaje_de_reemplazo");
    printf("mensaje testigo dejado cuando una parte es reemplazada por el "
                   "filtro\n");
    printf("%-30s", "\t-M media_types_censurables");
    printf("lista de media types censurados\n");
    printf("%-30s", "\t-o puerto_de_management");
    printf("puerto SCTP donde servirá el servicio de management\n");
    printf("%-30s", "\t-p puerto_local");
    printf("puerto TCP donde escuchará conexiones entrantes POP3\n");
    printf("%-30s", "\t-P puerto_origen");
    printf("puerto TCP donde se encuentra el servidor POP3 origen\n");
    printf("%-30s", "\t-t cmd");
    printf("comando utilizado para las transofmraciones externas\n");
    printf("%-30s", "\t-v");
    printf("imprime la versión y termina\n");
}

int get_user_pass();

int parse_media_types(struct media_types *media_type, const char *media_type_name);

void resolve_args(int argc, char **argv) {
    int command;
    int index = 0;
    int messages_count = 0;

    opterr = 0;

    init_params();

    while ((command = getopt(argc, argv, "e:h:l:L:m:M:o:p:P:t:v")) != -1){
        switch (command) {
            case 'e': // error file
                params->error_file = optarg;
                break;
                
            case 'h': // help
                print_help();
                exit(0);
                break;
                
            case 'l': // listen address
                params->listen_address = optarg;
                break;
                
            case 'L': // management listen address
                params->management_address = optarg;
                break;
                
            case 'm': // replacement message
                messages_count++;
                if (messages_count == 1){
                    params->replacement_msg = optarg;
                } else {
                    strcat(params->replacement_msg, "\n");
                    strcat(params->replacement_msg, optarg);
                }
                break;

            case 'M':
                parse_media_types(params->filtered_media_types, optarg);
                break;
                 
            case 'o': // management SCTP port
                params->management_port = (uint16_t)resolve_port("Management", 
                    optarg);
                break;
                
            case 'p': // proxy port
                params->port = (uint16_t)resolve_port("Listen", optarg);
                break;
                
            case 'P': // pop3 server port
                params->origin_port = (uint16_t)resolve_port("Origin server", 
                    optarg);
                break;
                
            case 't': // filter command
                int size = sizeof(char) * strlen(optarg) + 1;
                char * cmd = malloc(size);
                if (cmd == NULL)
                    exit(0);
                strcpy(cmd, optarg);
                params->filter_command = cmd;
                params->external_transf_activated   = true;
                break;

            case 'v':
                print_version();
                exit(0);
                break;

            case '?':
                if (optopt == 'e' || optopt == 'l' || optopt == 'L'
                    || optopt == 'm' || optopt == 'M' || optopt == 'o'
                    || optopt == 'p' || optopt == 'P' || optopt == 'v')
                    fprintf(stderr, "Option -%c requires an argument.\n",
                             optopt);
                else if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                exit(1);

            default:
                abort();
        }
    }

    index = optind;

    if ((argc - index) == 1) {
        params->origin_server = argv[index];
    } else {
        fprintf(stderr, "Usage: %s [ POSIX style options ] <origin-server>\n",
                argv[0]);
        exit(1);
    }

    resolve_address(params->listen_address, params->port, 
        &params->listenadddrinfo);
    resolve_address(params->management_address, params->management_port, 
        &params->managementaddrinfo);

}

void
init_params() {
    // default values
    params                            = malloc(sizeof(*params));
    params->listen_address            = "0.0.0.0";
    params->port                      = 1110;
    params->replacement_msg           = "Parte reemplazada.";
    params->filtered_media_types = new_media_types();
    params->origin_port               = 110;
    params->filter_command            = NULL;
    params->version                   = "0.0";
    params->listenadddrinfo           = 0;
    params->management_address        = "127.0.0.1";
    params->management_port           = 9090;
    params->external_transf_activated = false;
    params->managementaddrinfo        = 0;
    params->error_file                = "/dev/null";
}

long resolve_port(char * port_name, char *optarg) {
    char *endptr    = 0;
    const long port = strtol(optarg, &endptr, 10);

    if (endptr == optarg || *endptr != '\0' || port < 0 || port > USHRT_MAX ||
     ((LONG_MIN == port || LONG_MAX == port) && ERANGE == errno) ) {
        fprintf(stderr, "%s port should be an integer: %s\n", port_name, 
            optarg);
        exit(1);
    }

    return port;
}

void resolve_address(char *address, uint16_t port, 
    struct addrinfo ** addrinfo) {

    struct addrinfo hints = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%hu", port);

    // revisar, se podria usar la direccion de 32 bit (loopback) si no se 
    // cambio el default
    if (0 != getaddrinfo(address, buff, &hints, addrinfo)){
        fprintf(stderr,"Domain name resolution error\n");
    }
}
