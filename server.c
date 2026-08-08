#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

#define MAX_CLIENTS 100

struct Client {
    int socket;
    char username[50];
};

struct Client clients[MAX_CLIENTS];
pthread_mutex_t client_mutex;

enum MessageType {
    PRIVATE,
    COMMAND,
    PUBLIC
};

int find_client_by_username(const char *username) {
    pthread_mutex_lock(&client_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 &&
            strcmp(clients[i].username, username) == 0) {

            pthread_mutex_unlock(&client_mutex);
            return i;
        }
    }

    pthread_mutex_unlock(&client_mutex);
    return -1;
}

void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&client_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 &&
            clients[i].socket != sender_socket) {

            send(clients[i].socket, message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&client_mutex);
}

enum MessageType get_message_type(const char *buffer) {
    if (buffer[0] == '@') {
        return PRIVATE;
    }

    if (buffer[0] == '/') {
        return COMMAND;
    }

    return PUBLIC;
}

void handle_private(int sender_index, int receiver_index, char *msg) {
    char private_message[1200];

    snprintf(
        private_message,
        sizeof(private_message),
        "[WAYP] 🔒 %s: %s\n",
        clients[sender_index].username,
        msg
    );

    send(
        clients[receiver_index].socket,
        private_message,
        strlen(private_message),
        0
    );
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    int client_index = -1;
    char buffer[1024];
    char final_message[1200];
    char system_message[200];

    pthread_mutex_lock(&client_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == -1) {
            clients[i].socket = client_fd;
            client_index = i;
            break;
        }
    }

    pthread_mutex_unlock(&client_mutex);

    if (client_index == -1) {
        printf("Server full, rejecting connection\n");
        close(client_fd);
        return NULL;
    }

    /* Receive username */
    char username[50];

    int bytes_received = recv(
        client_fd,
        username,
        sizeof(username) - 1,
        0
    );

    if (bytes_received <= 0) {
        pthread_mutex_lock(&client_mutex);

        clients[client_index].socket = -1;

        pthread_mutex_unlock(&client_mutex);

        close(client_fd);
        return NULL;
    }

    username[bytes_received] = '\0';

    pthread_mutex_lock(&client_mutex);

    strncpy(
        clients[client_index].username,
        username,
        sizeof(clients[client_index].username) - 1
    );

    clients[client_index].username[
        sizeof(clients[client_index].username) - 1
    ] = '\0';

    snprintf(
        system_message,
        sizeof(system_message),
        "[WAYP] %s joined the chat.\n",
        clients[client_index].username
    );

    printf("%s", system_message);

    pthread_mutex_unlock(&client_mutex);

    broadcast_message(system_message, client_fd);

    printf(
        "Client registered: %s\n",
        clients[client_index].username
    );

    while (1) {

        int bytes_received = recv(
            client_fd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (bytes_received == -1) {
            perror("recv");
            break;
        }

        if (bytes_received == 0) {
            break;
        }

        buffer[bytes_received] = '\0';

        enum MessageType type = get_message_type(buffer);

        switch (type) {

            case PRIVATE: {
                char *space = strchr(buffer, ' ');

                if (space == NULL) {
                    char error_message[200];

                    snprintf(
                        error_message,
                        sizeof(error_message),
                        "[WAYP] Private message format: @username message\n"
                    );

                    send(
                        client_fd,
                        error_message,
                        strlen(error_message),
                        0
                    );

                    break;
                }

                *space = '\0';

                char *target_username = buffer + 1;
                char *msg_content = space + 1;

                int receiver_index =
                    find_client_by_username(target_username);

                if (receiver_index == -1) {

                    char error_message[200];

                    snprintf(
                        error_message,
                        sizeof(error_message),
                        "[WAYP] \"%s\" is not online.\n",
                        target_username
                    );

                    send(
                        client_fd,
                        error_message,
                        strlen(error_message),
                        0
                    );

                    break;
                }

                handle_private(
                    client_index,
                    receiver_index,
                    msg_content
                );

                break;
            }

            case COMMAND: {
                char error_message[200];

                snprintf(
                    error_message,
                    sizeof(error_message),
                    "[WAYP] Commands aren't implemented yet.\n"
                );

                send(
                    client_fd,
                    error_message,
                    strlen(error_message),
                    0
                );

                break;
            }

            case PUBLIC:

                snprintf(
                    final_message,
                    sizeof(final_message),
                    "%s: %s\n",
                    clients[client_index].username,
                    buffer
                );

                broadcast_message(final_message, client_fd);

                printf("%s", final_message);

                break;
        }
    }

    /* Notify everyone else that this client left */

    snprintf(
        system_message,
        sizeof(system_message),
        "[WAYP] %s left the chat.\n",
        clients[client_index].username
    );

    broadcast_message(system_message, client_fd);

    printf("%s", system_message);

    /* Remove client from client list */

    pthread_mutex_lock(&client_mutex);

    clients[client_index].socket = -1;
    clients[client_index].username[0] = '\0';

    pthread_mutex_unlock(&client_mutex);

    close(client_fd);

    return NULL;
}

int main(void) {

    pthread_mutex_init(&client_mutex, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].username[0] = '\0';
    }

    printf("TCP server is starting...\n");

    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully\n");

    struct sockaddr_in server_addr;

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Socket bound successfully\n");

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port 8080...\n");

    while (1) {

        printf("Waiting for a client...\n");

        int client_fd = accept(
            server_fd,
            NULL,
            NULL
        );

        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf("Client connection successful!\n");

        int *client = malloc(sizeof(int));

        if (client == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client = client_fd;

        pthread_t thread;

        if (pthread_create(
                &thread,
                NULL,
                handle_client,
                client
            ) != 0) {

            perror("pthread_create");

            free(client);
            close(client_fd);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    pthread_mutex_destroy(&client_mutex);

    return 0;
}