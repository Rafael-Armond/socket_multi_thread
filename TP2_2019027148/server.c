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
                if (equipments[i]->client_id < 10)
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
        snprintf(buf, BUFFER_SIZE, "Equipment limit exceeded\n");
        send(client_data->client_socket, buf, BUFFER_SIZE, 0);
        close(client_data->client_socket); // Verificar se realmente vai precisar fechar o socket
        return 0;
    }
}

int remove_equipment(int equipment_id)
{
    char buf[BUFFER_SIZE];
    int equipment_found = 0;

    for (int i = 0; i < numEquipmentsConnected; i++)
    {
        if (equipments[i]->client_id == equipment_id)
        {
            // Imprime mensagem no servidor
            if (equipment_id < 10)
                printf("Equipment 0%d removed\n", equipment_id);
            else
                printf("Equipment %d removed\n", equipment_id);

            // Envia mensagem de desconexão para o equipamento cliente
            printf(buf, "Successful removal");
            strtok(buf, "\0");
            send(equipments[i]->client_socket, buf, strlen(buf) + 1, 0);

            // Desconecta o cliente e libera o espaço de memória anteriormente alocado por ele
            close(equipments[i]->client_socket);
            free(equipments[i]);
            equipments[i] = NULL;
            numEquipmentsConnected--;
            equipment_found = 1;
            break;
        }
    }

    // Broadcast para os equipamentos conectados informando que o equipamento X foi removido
    for (int i = 0; i < numEquipmentsConnected; i++)
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

    return equipment_found;
}

void *client_thread(void *data)
{
    struct client_data *client_data = (struct client_data *)data;
    struct sockaddr *client_addr = (struct sockaddr *)(&client_data->storage);

    char client_addrstr[BUFFER_SIZE];
    addrtostr(client_addr, client_addrstr, BUFFER_SIZE);

    char buf[BUFFER_SIZE];
    // char response[BUFFER_SIZE];
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
            // int id_equipment_i = 0;
            // int id_equipment_j = 0;

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
                int id_equipment_i = atoi(substr);
                if (remove_equipment(id_equipment_i) == 0)
                {
                    sprintf(buf, "Equipment not found");
                    send(client_data->client_socket, buf, strlen(buf), 0);
                }
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

    // Configurando o storage do socket
    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init("v4", argv[1], &storage))
    {
        usage(argc, argv);
    }

    // Criando o socket
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

    // Binds the socket to the address and port number specified in addr
    struct sockaddr *server_socket_addr = (struct sockaddr *)(&storage);
    if (bind(server_socket, server_socket_addr, sizeof(storage)) != 0)
    {
        logexit("bind");
    }

    // Listens for incoming connections on the socket
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

        // Aceita uma conexão de cliente
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
*/