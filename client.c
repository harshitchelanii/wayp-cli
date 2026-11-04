#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    typedef int socket_t;
    #define closesocket close
#endif

#define MAX_MESSAGE_SIZE 1024
#define SERVER_PORT 8080

#define DISCOVERY_PORT 8081
#define DISCOVERY_REQUEST "WAYP_DISCOVER"
#define DISCOVERY_REPLY "WAYP_SERVER"

static int send_all(socket_t sockfd, const void *buf, size_t len)
{
    size_t total_sent = 0;
    const char *ptr = (const char *)buf;

    while (total_sent < len)
    {
        int bytes_sent = send(
            sockfd,
            ptr + total_sent,
            (int)(len - total_sent),
            0
        );

        if (bytes_sent <= 0)
        {
            return -1;
        }

        total_sent += bytes_sent;
    }

    return 0;
}

static int send_message(socket_t sockfd, const char *message)
{
    uint32_t message_length = (uint32_t)strlen(message);
    uint32_t network_length = htonl(message_length);

    if (send_all(sockfd, &network_length, sizeof(network_length)) == -1)
    {
        return -1;
    }

    if (send_all(sockfd, message, message_length) == -1)
    {
        return -1;
    }

    return 0;
}


static int recv_all(socket_t sockfd, void *buf, size_t len)
{
    size_t total_received = 0;
    char *ptr = (char *)buf;

    while (total_received < len) {
        int bytes_received = recv(
            sockfd,
            ptr + total_received,
            (int)(len - total_received),
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

static int recv_message(socket_t sockfd, char *buffer, size_t buffer_size)
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

void *receive_messages(void *arg)
{
    socket_t client_fd = *(socket_t *)arg;
    free(arg);

    char buffer[1024];

    while(1)
{
    int result = recv_message(client_fd, buffer, sizeof(buffer));

    if (result != 0)
    {
        if (result == 1)
        {
            printf("Server disconnected.\n");
        }
        else
        {
            printf("Error receiving message.\n");
        }

        break;
    }

        if(strcmp(buffer, "__CLEAR__") == 0)
        {
            printf("\033[2J\033[H");
            printf("[WAYP] Messages cleared.\n");
            printf("> \033[2mType your message...\033[0m\r> ");
            fflush(stdout);
        }
        else
        {
            printf("\r\033[2K");
            printf("%s", buffer);
            printf("> \033[2mType your message...\033[0m\r> ");
            fflush(stdout);
        }
    }

    return NULL;
}




static socket_t connect_to_host(const char *host, int port)
{
    char port_text[16];

    snprintf(port_text, sizeof(port_text), "%d", port);

    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *results = NULL;

    if (getaddrinfo(host, port_text, &hints, &results) != 0)
    {
        printf("Could not resolve \"%s\".\n", host);
        return -1;
    }

    socket_t client_fd = -1;

    for (struct addrinfo *entry = results; entry != NULL; entry = entry->ai_next)
    {
        client_fd = socket(
            entry->ai_family,
            entry->ai_socktype,
            entry->ai_protocol
        );

        if (client_fd == -1)
        {
            continue;
        }

        if (connect(client_fd, entry->ai_addr, (int)entry->ai_addrlen) == 0)
        {
            break;
        }

        closesocket(client_fd);
        client_fd = -1;
    }

    freeaddrinfo(results);

    return client_fd;
}

/* Shouts on the local network and waits for a wayp server to answer */
static int discover_server(char *host, size_t host_size)
{
    socket_t discovery_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (discovery_fd == -1)
    {
        return -1;
    }

    int broadcast = 1;

    setsockopt(
        discovery_fd,
        SOL_SOCKET,
        SO_BROADCAST,
        (const char *)&broadcast,
        sizeof(broadcast)
    );

    struct timeval timeout;

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    setsockopt(
        discovery_fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        (const char *)&timeout,
        sizeof(timeout)
    );

    struct sockaddr_in broadcast_addr;

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(
        discovery_fd,
        DISCOVERY_REQUEST,
        strlen(DISCOVERY_REQUEST),
        0,
        (struct sockaddr *)&broadcast_addr,
        sizeof(broadcast_addr)
    );

    char reply[64];

    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    int received = recvfrom(
        discovery_fd,
        reply,
        sizeof(reply) - 1,
        0,
        (struct sockaddr *)&server_addr,
        &addr_len
    );

    closesocket(discovery_fd);

    if (received <= 0)
    {
        return -1;
    }

    reply[received] = '\0';

    if (strncmp(reply, DISCOVERY_REPLY, strlen(DISCOVERY_REPLY)) != 0)
    {
        return -1;
    }

    if (inet_ntop(AF_INET, &server_addr.sin_addr, host, host_size) == NULL)
    {
        return -1;
    }

    return 0;
}

static socket_t connect_to_server(void)
{
    char host[64];

    printf("Looking for a server on this network...\n");

    if (discover_server(host, sizeof(host)) != 0)
    {
        printf("No server found on this network.\n");
        return -1;
    }

    printf("Found a server at %s\n", host);

    return connect_to_host(host, SERVER_PORT);
}

/* Keeps asking until the server accepts the name as unique */
static int register_username(socket_t client_fd)
{
    char username[50];
    char reply[64];

    while (1)
    {
        printf("Enter username : ");
        fflush(stdout);

        if (fgets(username, sizeof(username), stdin) == NULL)
        {
            return -1;
        }

        username[strcspn(username, "\r\n")] = '\0';

        if (username[0] == '\0')
        {
            printf("Username cannot be empty.\n");
            continue;
        }

        if (send_message(client_fd, username) == -1)
        {
            return -1;
        }

        if (recv_message(client_fd, reply, sizeof(reply)) != 0)
        {
            return -1;
        }

        if (strcmp(reply, "__NAME_OK__") == 0)
        {
            printf("Welcome, %s!\n", username);
            return 0;
        }

        if (strcmp(reply, "__NAME_TAKEN__") == 0)
        {
            printf("\"%s\" is already taken, pick another one.\n", username);
            continue;
        }

        printf("That username is not allowed, pick another one.\n");
    }
}

int main(void){
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    socket_t client_fd = connect_to_server();
    if(client_fd == -1){
        printf("Connection Failed.\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server!\n");
    if(register_username(client_fd) == -1){
        printf("Could not register a username.\n");
        closesocket(client_fd);
        exit(EXIT_FAILURE);
    }
    socket_t *socket_ptr = malloc(sizeof(socket_t));

    *socket_ptr = client_fd;

    pthread_t receive_thread;

    pthread_create(&receive_thread, NULL, receive_messages, socket_ptr);

    pthread_detach(receive_thread);

    char message[1024];
    while(1) {
        printf("> \033[2mType your message...\033[0m\r> ");
        fflush(stdout);
        if (fgets(message, sizeof(message), stdin) == NULL)
        {
            break;
        }
        printf("\033[A\033[2K\r");
        fflush(stdout);
        message[strcspn(message, "\r\n")] = '\0';
        if (message[0] == '\0')
        {
            continue;
        }
        if (message[0] != '>')
        {
            printf("You: %s\n", message);
            fflush(stdout);
        }
         send_message(client_fd, message);
    }
    return 0;
}
