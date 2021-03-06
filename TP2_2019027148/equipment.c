#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 15

int equipment_id = 0;
int target_equip_id = 0;

struct client_data
{
    int csock;
    int client_id;
};

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

void *receiver(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    char buf[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    char substr[3]; // Guarda os comandos vindos do cliente

    while (1)
    {
        memset(buf, 0, BUFFER_SIZE);
        size_t count = recv(cdata->csock, buf, BUFFER_SIZE - 1, 0);
        if (!count)
        {
            close(cdata->csock);
            exit(EXIT_SUCCESS);
        }
        strtok(buf, "\n");
        strtok(buf, "\0");

        if (strlen(buf) > 1)
        {
            strncpy(substr, buf, 2); // Os comandos vindos do cliente são sempre os dois primeiros digitos do payload
            substr[2] = '\0';
            int comando = atoi(substr);

            switch (comando)
            {
            case 1:
                strncpy(substr, buf + 5, 2); // Pega o ID enviado pelo servidor
                substr[2] = '\0';
                int equip_id_int = atoi(substr);
                if (equipment_id == 0)
                {
                    equipment_id = equip_id_int;
                    if (equip_id_int < 10)
                        printf("New ID: 0%d\n", equip_id_int);
                    else
                        printf("New ID: %d\n", equip_id_int);
                }
                else
                {
                    if (equip_id_int < 10)
                    {
                        printf("Equipment 0%d added\n", equip_id_int);
                    }
                    else
                    {
                        printf("Equipment %d added\n", equip_id_int);
                    }
                }
                break;
            case 2:
                strncpy(substr, buf + 5, 2); // Pega o ID do equipamento que foi removido
                substr[2] = '\0';
                int id_equipment_i = atoi(substr);
                if (id_equipment_i < 10 && id_equipment_i != equipment_id)
                    printf("Equipment 0%d removed\n", id_equipment_i);
                else if (id_equipment_i != equipment_id)
                    printf("Equipment %d removed\n", id_equipment_i);
                else if (id_equipment_i == equipment_id)
                {
                    equipment_id = 0;
                    printf("Successful removal\n");
                    exit(EXIT_SUCCESS);
                }
                break;
            case 3:
                memset(msg, 0, BUFFER_SIZE);
                strcpy(msg, buf + 6); // Atribui a msg a string contendo a lista de IDs de equipamentos conectados
                printf("%s\n", msg);
                break;
            case 4:
                memset(msg, 0, BUFFER_SIZE);
                strcpy(msg, buf + 5);
                if (target_equip_id < 10)
                    printf("Value from 0%d: %s\n", target_equip_id, msg);
                else
                    printf("Value from %d: %s\n", target_equip_id, msg);
                break;
            case 5:
                memset(msg, 0, BUFFER_SIZE);
                strcpy(msg, buf + 5);
                printf("%s\n", msg);
                break;
            case 6:
                printf("Target equipment not found\n");
                break;
            case 7:
                memset(msg, 0, BUFFER_SIZE);
                strcpy(msg, buf + 5);
                printf("%s\n", msg);
                break;
            default:
                printf("nenhum comando reconhecido\n");
                break;
            }
        }
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != addrparse(argv[1], argv[2], &storage))
    {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(s, addr, sizeof(storage)))
    {
        logexit("connect");
    }

    char addrstr[BUFFER_SIZE];
    addrtostr(addr, addrstr, BUFFER_SIZE);
    struct client_data *cdata = malloc(sizeof(*cdata));
    if (!cdata)
    {
        logexit("malloc");
    }

    cdata->csock = s;
    pthread_t tid;
    pthread_create(&tid, NULL, receiver, cdata);

    char buf[BUFFER_SIZE];

    snprintf(buf, BUFFER_SIZE, "01xxx");
    strtok(buf, "\0");
    send(s, buf, strlen(buf) + 1, 0);

    while (equipment_id == 0)
    {
        continue;
    }

    char substr[BUFFER_SIZE];

    while (1)
    {
        memset(buf, 0, BUFFER_SIZE);
        fgets(buf, BUFFER_SIZE - 1, stdin);
        strtok(buf, "\0");

        if (strlen(buf) > 1)
        {
            if (strcmp(buf, "close connection\n") == 0 || strcmp(buf, "close connection") == 0 || strcmp(buf, "close connection\0") == 0)
            {
                if (equipment_id < 10)
                {
                    sprintf(buf, "02xxx0%d", equipment_id);
                    strtok(buf, "\0");
                }
                else
                {
                    sprintf(buf, "02xxx%d", equipment_id);
                    strtok(buf, "\0");
                }

                size_t count = send(s, buf, strlen(buf) + 1, 0);
                if (count != strlen(buf) + 1)
                {
                    logexit("send");
                }
                if (!count)
                {
                    break;
                }
            }
            else if (strcmp(buf, "list equipment\n") == 0 || strcmp(buf, "list equipment") == 0 || strcmp(buf, "list equipment\0") == 0)
            {
                if (equipment_id < 10)
                    sprintf(buf, "04xxx0%d", equipment_id);
                else
                    sprintf(buf, "04xxx%d", equipment_id);

                strtok(buf, "\0");
                size_t count = send(s, buf, strlen(buf) + 1, 0);
                if (count != strlen(buf) + 1)
                {
                    logexit("send");
                }
            }
            else
            {
                strncpy(substr, buf, 25);
                substr[24] = '\0';
                if (strcmp(substr, "request information from \n") == 0 || strcmp(substr, "request information from") == 0)
                {
                    // Substr guardará o valor do ID do equipamento a ser requerido
                    strncpy(substr, buf + 25, 2);
                    substr[2] = '\0';
                    target_equip_id = atoi(substr);
                    if (equipment_id < 10 && target_equip_id < 10)
                        sprintf(buf, "030%d0%d", equipment_id, target_equip_id);
                    else if (equipment_id >= 10 && target_equip_id >= 10)
                        sprintf(buf, "03%d%d", equipment_id, target_equip_id);
                    else if (target_equip_id >= 10)
                        sprintf(buf, "030%d%d", equipment_id, target_equip_id);
                    else
                        sprintf(buf, "03%d0%d", equipment_id, target_equip_id);
                    strtok(buf, "\0");
                    send(s, buf, strlen(buf) + 1, 0);
                }
            }
        }
    }

    close(s);
    exit(EXIT_SUCCESS);
}

/*
Códigos que o cliente envia para o servidor (client -> server):
    1- Cliente abrindo conexão com o servidor
    2- Cliente encerrando conexão com o servidor
    3- Cliente envia mensagem "request information from <id>" para o servidor
    4- Cliente envia mensagem "list equipment" para o servidor
*/