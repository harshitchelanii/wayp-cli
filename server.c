#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>

#ifdef ENABLE_BLUETOOTH
    #include <bluetooth/bluetooth.h>
    #include <bluetooth/rfcomm.h>
#endif

#define MAX_MESSAGE_SIZE 1024
#define SERVER_PORT 8080
#define DISCOVERY_PORT 8081
#define RFCOMM_CHANNEL 1
#define DISCOVERY_REQUEST "WAYP_DISCOVER"
#define DISCOVERY_REPLY "WAYP_SERVER"

static int send_all(int sockfd, const void *buf, size_t len){
    size_t total_sent = 0;
    const char *ptr = (const char *)buf;
    while (total_sent < len){
        ssize_t bytes_sent = send(sockfd,
            ptr + total_sent,
            len - total_sent,
             0);
        if(bytes_sent <= 0){
        return -1;

    } 
    total_sent += bytes_sent;
}
    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len)
{
    size_t total_received = 0;
    char *ptr = (char *)buf;

    while (total_received < len) {
        ssize_t bytes_received = recv(
            sockfd,
            ptr + total_received,
            len - total_received,
            0
        );

        if (bytes_received == 0) {
            return 1;
        }
        if (bytes_received < 0){
            return -1;
        }

        total_received += bytes_received;
    }

    return 0;
}

static int recv_message(int sockfd, char *buffer, size_t buffer_size)
{
    uint32_t network_length;

   int result = recv_all(sockfd, &network_length, sizeof(network_length));

    if (result !=0) {
        return result;
    }

    uint32_t message_length = ntohl(network_length);

    if (message_length >= buffer_size || message_length > MAX_MESSAGE_SIZE) {
    return -1;
}

    result = recv_all(sockfd, buffer, message_length);

if (result != 0) {
    return result;
}

    buffer[message_length] = '\0';

    return 0;
}


static int send_message(int sockfd, const char *message){
    uint32_t message_length = strlen(message);
    uint32_t network_length = htonl(message_length);

    if(send_all(sockfd, &network_length, sizeof(network_length)) == -1){
        return -1;
    }
    if(send_all(sockfd, message, message_length) == -1){
        return -1;
    }
        return 0;
}





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

struct Command {
    char *name;
    char *description;
    void (*handler)(int sender_index, char *command);
};

void handle_users(int sender_index, char *command);
void handle_help(int sender_index, char *command);
void handle_whoami(int sender, char *command);
void handle_clear(int sender_index, char *command);
void handle_pong(int sender_index, char *command);

struct Command commands[] = {
    {">users", "List online users", handle_users},
    {">help", "Show available commands", handle_help},
    {">whoami", "Show your username", handle_whoami},
    {">clear", "Clear your terminal", handle_clear},
    {">pong", "Check server response", handle_pong}
};

static void sanitize_text(char *text)
{
    char *read_ptr = text;
    char *write_ptr = text;

    while (*read_ptr != '\0') {
        unsigned char current = (unsigned char)*read_ptr;

        if (current >= 0x20 && current != 0x7f) {
            *write_ptr = *read_ptr;
            write_ptr++;
        }

        read_ptr++;
    }

    *write_ptr = '\0';
}

/* client_mutex must already be held by the caller */
static int username_taken(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 &&
            strcmp(clients[i].username, username) == 0) {

            return 1;
        }
    }

    return 0;
}

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

            send_message(clients[i].socket, message);
        }
    }

    pthread_mutex_unlock(&client_mutex);
}

enum MessageType get_message_type(const char *buffer) {
    if (buffer[0] == '@') {
        return PRIVATE;
    }

    if (buffer[0] == '>') {
        return COMMAND;
    }

    return PUBLIC;
}



void handle_clear(int sender_index, char *command){
    send_message(clients[sender_index].socket, "__CLEAR__");
}


void handle_users(int sender_index, char *command){
     char user_list[1200];
        snprintf(user_list, sizeof(user_list), "[WAYP] Online users :\n");

        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++){
            if(clients[i].socket != -1){
                snprintf(
                    user_list + strlen(user_list),
                    sizeof(user_list) - strlen(user_list),
                    "- %s\n",
                    clients[i].username
                );
            }
        }
        pthread_mutex_unlock(&client_mutex);

        send_message(clients[sender_index].socket, user_list);
    }

void handle_help(int sender_index, char *command){
     char help_message[1200];

    snprintf(
        help_message,
        sizeof(help_message),
        "[WAYP] Available commands:\n"
    );

    int command_count = sizeof(commands) / sizeof(commands[0]);

    for (int i = 0; i < command_count; i++)
    {
        snprintf(
            help_message + strlen(help_message),
            sizeof(help_message) - strlen(help_message),
            "%s - %s\n",
            commands[i].name,
            commands[i].description
        );
    }

    send_message(
        clients[sender_index].socket,
        help_message
    );
}



void handle_whoami(int sender, char *command){
    char whoami_message[200];
    snprintf(
        whoami_message,
        sizeof(whoami_message),
        "[WAYP] You are : %s\n",
        clients[sender].username
    );

    send_message(
    clients[sender].socket,
    whoami_message
);

}


void handle_pong(int sender_index, char *command)
{
    send_message(clients[sender_index].socket,
    "[WAYP] pong!\n"
    );
}



void handle_command(int sender_index, char *command){
    char *args = "";
    char *space = strchr(command, ' ');
    if (space != NULL) {
    *space = '\0';
    args = space + 1;
}
    int command_found = 0;
    char command_unknown[200];
    int command_count = sizeof(commands) / sizeof(commands[0]);
    for(int i = 0; i < command_count; i++){
        if(strcmp(command, commands[i].name) == 0){
            commands[i].handler(sender_index, args);
            command_found = 1;
            break;
        }
    }
    if(command_found == 0){
        snprintf(command_unknown,
        sizeof(command_unknown),
        "[WAYP] Command %s not found.\n",
        command
        );
        
        send_message(clients[sender_index].socket ,
        command_unknown);
    }
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

    send_message(
        clients[receiver_index].socket,
        private_message
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

    while (1) {

        if (recv_message(client_fd, username, sizeof(username)) != 0)
{
    pthread_mutex_lock(&client_mutex);

    clients[client_index].socket = -1;

    pthread_mutex_unlock(&client_mutex);

    close(client_fd);
    return NULL;
}

        username[strcspn(username, "\r\n")] = '\0';
        sanitize_text(username);

        if (username[0] == '\0') {
            send_message(client_fd, "__NAME_BAD__");
            continue;
        }

        pthread_mutex_lock(&client_mutex);

        if (username_taken(username) == 1) {

            pthread_mutex_unlock(&client_mutex);

            send_message(client_fd, "__NAME_TAKEN__");
            continue;
        }

        strncpy(
            clients[client_index].username,
            username,
            sizeof(clients[client_index].username) - 1
        );

        clients[client_index].username[
            sizeof(clients[client_index].username) - 1
        ] = '\0';

        pthread_mutex_unlock(&client_mutex);

        break;
    }

    send_message(client_fd, "__NAME_OK__");

    snprintf(
        system_message,
        sizeof(system_message),
        "[WAYP] %s joined the chat.\n",
        username
    );

    printf("%s", system_message);

    broadcast_message(system_message, client_fd);

    printf(
        "Client registered: %s\n",
        username
    );

    while (1) {

    int result = recv_message(client_fd, buffer, sizeof(buffer));

if (result != 0)
{
    if (result == 1)
    {
        printf("Client disconnected.\n");
    }
    else
    {
        printf("Error receiving message from client.\n");
    }

    break;
}
 
        sanitize_text(buffer);

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

                    send_message(
                        client_fd,
                        error_message
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

                    send_message(
                        client_fd,
                        error_message
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
                handle_command(client_index, buffer);
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

    snprintf(
        system_message,
        sizeof(system_message),
        "[WAYP] %s left the chat.\n",
        clients[client_index].username
    );

    broadcast_message(system_message, client_fd);

    printf("%s", system_message);


    pthread_mutex_lock(&client_mutex);

    clients[client_index].socket = -1;
    clients[client_index].username[0] = '\0';

    pthread_mutex_unlock(&client_mutex);

    close(client_fd);

    return NULL;
}

struct Listener {
    int socket;
    const char *name;
};

void *accept_loop(void *arg) {

    struct Listener *listener = (struct Listener *)arg;

    while (1) {

        int client_fd = accept(
            listener->socket,
            NULL,
            NULL
        );

        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf(
            "Client connected over %s\n",
            listener->name
        );

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

    return NULL;
}

static int create_tcp_listener(void)
{
    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    int reuse = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );

    struct sockaddr_in server_addr;

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

#ifdef ENABLE_BLUETOOTH
static int create_bluetooth_listener(void)
{
    int server_fd = socket(
        AF_BLUETOOTH,
        SOCK_STREAM,
        BTPROTO_RFCOMM
    );

    if (server_fd == -1) {
        perror("bluetooth socket");
        return -1;
    }

    struct sockaddr_rc local_addr;

    memset(
        &local_addr,
        0,
        sizeof(local_addr)
    );

    local_addr.rc_family = AF_BLUETOOTH;
    local_addr.rc_bdaddr = *BDADDR_ANY;
    local_addr.rc_channel = RFCOMM_CHANNEL;

    if (bind(
            server_fd,
            (struct sockaddr *)&local_addr,
            sizeof(local_addr)
        ) < 0) {

        perror("bluetooth bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("bluetooth listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}
#endif

/* Answers the broadcast a client sends while looking for us on the wlan */
void *discovery_loop(void *arg) {

    (void)arg;

    int discovery_fd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (discovery_fd == -1) {
        perror("discovery socket");
        return NULL;
    }

    int reuse = 1;

    setsockopt(
        discovery_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );

    struct sockaddr_in discovery_addr;

    memset(
        &discovery_addr,
        0,
        sizeof(discovery_addr)
    );

    discovery_addr.sin_family = AF_INET;
    discovery_addr.sin_port = htons(DISCOVERY_PORT);
    discovery_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(
            discovery_fd,
            (struct sockaddr *)&discovery_addr,
            sizeof(discovery_addr)
        ) < 0) {

        perror("discovery bind");
        close(discovery_fd);
        return NULL;
    }

    printf(
        "Wlan discovery listening on port %d...\n",
        DISCOVERY_PORT
    );

    while (1) {

        char request[64];

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        ssize_t received = recvfrom(
            discovery_fd,
            request,
            sizeof(request) - 1,
            0,
            (struct sockaddr *)&client_addr,
            &addr_len
        );

        if (received <= 0) {
            continue;
        }

        request[received] = '\0';

        if (strcmp(request, DISCOVERY_REQUEST) != 0) {
            continue;
        }

        char reply[64];

        snprintf(
            reply,
            sizeof(reply),
            "%s %d",
            DISCOVERY_REPLY,
            SERVER_PORT
        );

        sendto(
            discovery_fd,
            reply,
            strlen(reply),
            0,
            (struct sockaddr *)&client_addr,
            addr_len
        );
    }

    close(discovery_fd);

    return NULL;
}

int main(void) {

    signal(SIGPIPE, SIG_IGN);

    pthread_mutex_init(&client_mutex, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].username[0] = '\0';
    }

    printf("TCP server is starting...\n");

    struct Listener tcp_listener;

    tcp_listener.socket = create_tcp_listener();
    tcp_listener.name = "internet/wlan";

    if (tcp_listener.socket == -1) {
        return 1;
    }

    printf(
        "Server listening on port %d (internet and wlan)...\n",
        SERVER_PORT
    );

    pthread_t discovery_thread;

    if (pthread_create(
            &discovery_thread,
            NULL,
            discovery_loop,
            NULL
        ) == 0) {

        pthread_detach(discovery_thread);
    }

#ifdef ENABLE_BLUETOOTH
    struct Listener bluetooth_listener;

    bluetooth_listener.socket = create_bluetooth_listener();
    bluetooth_listener.name = "bluetooth";

    if (bluetooth_listener.socket != -1) {

        printf(
            "Server listening on rfcomm channel %d (bluetooth)...\n",
            RFCOMM_CHANNEL
        );

        pthread_t bluetooth_thread;

        if (pthread_create(
                &bluetooth_thread,
                NULL,
                accept_loop,
                &bluetooth_listener
            ) == 0) {

            pthread_detach(bluetooth_thread);
        }
    }
#endif

    accept_loop(&tcp_listener);

    close(tcp_listener.socket);

    pthread_mutex_destroy(&client_mutex);

    return 0;
}