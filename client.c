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
    #include <netinet/in.h>
    #include <sys/socket.h>
    typedef int socket_t;
    #define closesocket close
#endif
#define MAX_MESSAGE_SIZE 1024

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




int main(void){
    char username[50];
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    socket_t client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        perror("Connection Failed.\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server!\n");
    printf("Enter username : ");
    fgets(username, sizeof(username), stdin);
    if (username[strlen(username) - 1] == '\n'){
        username[strlen(username)- 1] = '\0';
    }
    socket_t *socket_ptr = malloc(sizeof(socket_t));

    *socket_ptr = client_fd;

    pthread_t receive_thread;

    pthread_create(&receive_thread, NULL, receive_messages, socket_ptr);

    pthread_detach(receive_thread);

    char message[1024];
        send_message(client_fd, username);
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
