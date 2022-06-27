#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define MAX_EQUIPMENTS 15
#define BUFFER_SIZE 1024

int numEquipmentsConnected = 0;
int equipmentId = 0;

struct client_data
{
    int client_socket;
    int client_id;
    struct sockaddr_storage storage;
};

struct client_data *equipments[MAX_EQUIPMENTS];

void usage(int argc, char **argv)
{
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

// void remove_array_element(struct client_data *client_data)
int insert_new_equipment(void *data)
{
    struct client_data *client_data = (struct client_data *)data;
    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);

    if (numEquipmentsConnected < MAX_EQUIPMENTS)
    {
        client_data->client_id = equipmentId + 1;
        equipmentId++;
        equipments[numEquipmentsConnected] = client_data;
        numEquipmentsConnected++;

        if (client_data->client_id < 10)
        {
            printf("Equipment 0%d added\n", client_data->client_id);
            sprintf(buf, "01xxx0%d", client_data->client_id);
        }
        else
        {
            printf("Equipment %d added\n", client_data->client_id);
            sprintf(buf, "01xxx%d", client_data->client_id);
        }

        // Manda a mensagem de novo equipamento inserido, com o seu ID, para o equipamento cliente
        strtok(buf, "\0");
        send(client_data->client_socket, buf, BUFFER_SIZE, 0);

        // Percorre todos os equipamentos informando que um novo equipamento foi inserido e seu ID
        for (int i = 0; i < numEquipmentsConnected; i++)
        {
            if (equipments[i]->client_id != client_data->client_id)
            {
                memset(buf, 0, BUFFER_SIZE);
                if (client_data->client_id < 10)
                {
                    sprintf(buf, "01xxx0%d", client_data->client_id);
                }
                else
                {
                    sprintf(buf, "01xxx%d", client_data->client_id);
                }
                strtok(buf, "\0");
                send(equipments[i]->client_socket, buf, BUFFER_SIZE, 0);
            }
        }

        return 1;
    }
    else
    {
        snprintf(buf, BUFFER_SIZE, "07xxxEquipment limit exceeded\n");
        send(client_data->client_socket, buf, BUFFER_SIZE, 0);
        close(client_data->client_socket);
        return 0;
    }
}

int remove_equipment(int equipment_id)
{
    char buf[BUFFER_SIZE];
    int equipment_found = 0;

    for (int i = 0; i <= MAX_EQUIPMENTS; i++)
    {
        if (equipments[i] != NULL)
            if (equipments[i]->client_id == equipment_id)
            {
                if (equipment_id < 10)
                    printf("Equipment 0%d removed\n", equipment_id);
                else
                    printf("Equipment %d removed\n", equipment_id);

                // Envia código de desconexão para o equipamento cliente
                if (equipment_id < 10)
                    sprintf(buf, "02xxx0%d", equipment_id);
                else
                    sprintf(buf, "02xxx%d", equipment_id);
                strtok(buf, "\0");
                send(equipments[i]->client_socket, buf, strlen(buf) + 1, 0);

                // Desconecta o cliente e libera o espaço de memória anteriormente alocado por ele
                close(equipments[i]->client_socket);
                equipments[i] = NULL;
                free(equipments[i]);
                numEquipmentsConnected--;

                equipment_found = 1;
                break;
            }
    }

    // Broadcast para os equipamentos conectados informando que o equipamento X foi removido
    for (int i = 0; i < MAX_EQUIPMENTS; i++)
    {
        if (equipments[i] != NULL)
        {
            if (equipments[i]->client_id != equipment_id)
            {
                memset(buf, 0, BUFFER_SIZE);
                if (equipment_id < 10)
                    sprintf(buf, "02xxx0%d", equipment_id);
                else
                    sprintf(buf, "02xxx%d", equipment_id);
                strtok(buf, "\0");

                send(equipments[i]->client_socket, buf, BUFFER_SIZE, 0);
            }
        }
    }

    return equipment_found;
}

void *client_thread(void *data)
{
    struct client_data *client_data = (struct client_data *)data;
    struct sockaddr *client_addr = (struct sockaddr *)(&client_data->storage);

    char client_addrstr[BUFFER_SIZE];
    addrtostr(client_addr, client_addrstr, BUFFER_SIZE);

    char buf[BUFFER_SIZE];
    char substr[3];

    while (1)
    {
        memset(buf, 0, BUFFER_SIZE); // Zera o buffer
        size_t count = recv(client_data->client_socket, buf, BUFFER_SIZE - 1, 0);

        // Encerra a conexão entre cliente e servidor, quando o cliente não envia nada
        if (count == 0)
        {
            printf("[log] connection closed by %s\n", client_addrstr);
            break;
        }

        if (strlen(buf) > 1)
        {
            strncpy(substr, buf, 2);
            substr[2] = '\0';
            int comando = atoi(substr);
            int id_equipment_i = 0;

            switch (comando)
            {
            case 1:
                if (insert_new_equipment(client_data) == 0)
                {
                    close(client_data->client_socket);
                    pthread_exit(EXIT_SUCCESS);
                }
                break;
            case 2:
                strncpy(substr, buf + 5, 2); // Pega o ID do equipamento a ser removido
                substr[2] = '\0';
                id_equipment_i = atoi(substr);
                if (remove_equipment(id_equipment_i) == 0)
                {
                    sprintf(buf, "Equipment not found");
                    send(client_data->client_socket, buf, strlen(buf), 0);
                }
                break;
            case 3:
                strncpy(substr, buf + 4, 2);
                substr[2] = '\0';
                id_equipment_i = atoi(substr);
                int equipment_found = 0;
                memset(buf, 0, BUFFER_SIZE);
                strtok(buf, "\0");
                sprintf(buf, "05xxxrequested information");
                for (int i = 0; i <= numEquipmentsConnected; i++)
                {
                    if (equipments[i] != NULL)
                        if (equipments[i]->client_id == id_equipment_i)
                        {
                            equipment_found = 1;
                            send(equipments[i]->client_socket, buf, BUFFER_SIZE, 0);
                            memset(buf, 0, BUFFER_SIZE);
                        }
                }

                if (equipment_found == 0)
                {
                    if (id_equipment_i < 10)
                        printf("Equipment 0%d not found\n", id_equipment_i);
                    else
                        printf("Equipment %d not found\n", id_equipment_i);

                    sprintf(buf, "06");
                    send(client_data->client_socket, buf, BUFFER_SIZE, 0);
                    memset(buf, 0, BUFFER_SIZE);
                }
                else
                {
                    memset(buf, 0, BUFFER_SIZE);
                    sprintf(buf, "04xxx%.2f", get_random());
                    strtok(buf, "\0");
                    send(client_data->client_socket, buf, BUFFER_SIZE, 0);
                    memset(buf, 0, BUFFER_SIZE);
                }
                break;
            case 4:
                strncpy(substr, buf + 5, 2);
                substr[2] = '\0';
                id_equipment_i = atoi(substr);
                memset(buf, 0, BUFFER_SIZE);
                sprintf(buf, "03xxx");
                char id_str[4];
                for (int i = 0; i <= numEquipmentsConnected; i++)
                {
                    if (equipments[i] != NULL)
                        if (equipments[i]->client_id != id_equipment_i)
                        {
                            memset(id_str, 0, 4);
                            if (i < 9)
                            {
                                if (i == 1)
                                    sprintf(id_str, "0%d", equipments[i]->client_id);
                                sprintf(id_str, " 0%d", equipments[i]->client_id);
                            }
                            else
                            {
                                sprintf(id_str, " %d", equipments[i]->client_id);
                            }
                            strcat(buf, id_str);
                        }
                }
                send(client_data->client_socket, buf, strlen(buf), 0);
                break;
            default:
                printf("nenhum comando reconhecido\n");
                break;
            }
        }
    }

    close(client_data->client_socket);
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init("v4", argv[1], &storage))
    {
        usage(argc, argv);
    }

    int server_socket;
    server_socket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *server_socket_addr = (struct sockaddr *)(&storage);
    if (bind(server_socket, server_socket_addr, sizeof(storage)) != 0)
    {
        logexit("bind");
    }

    if (listen(server_socket, 15) != 0)
    {
        logexit("listen");
    }

    // Limpando o vetor de clientes conectados
    for (int i = 0; i < MAX_EQUIPMENTS; i++)
    {
        equipments[i] = NULL;
    }

    char addrstr[BUFFER_SIZE];
    addrtostr(server_socket_addr, addrstr, BUFFER_SIZE);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1)
    {
        struct sockaddr_storage client_storage;
        struct sockaddr *client_addr = (struct sockaddr *)(&client_storage);
        socklen_t client_addrlen = sizeof(client_storage);

        int client_socket = accept(server_socket, client_addr, &client_addrlen);
        if (client_socket == -1)
        {
            logexit("accept");
        }

        struct client_data *client_data = malloc(sizeof(*client_data));
        if (!client_data)
        {
            logexit("malloc");
        }
        client_data->client_socket = client_socket;
        memcpy(&(client_data->storage), &client_storage, sizeof(client_storage));

        // Cria uma thread para tratar a conexão de cliente
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, client_data);
    }

    exit(EXIT_SUCCESS);
}

/*
Códigos que o servidor envia para o cliente (server -> client)
    1- Servidor enviando para o cliente o seu ID
    2- Servidor enviando para o cliente o ID do equipamento desconectado
    3- Servidor enviando para o cliente a lista de IDs dos equipamentos conectados
    4- Servidor envia para o cliente um valor referente a leitura do equipamento
    5- Servidor envia para o cliente um comando para imprimir uma mensagem específica no console
*/