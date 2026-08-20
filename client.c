#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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


void *receive_messages(void *arg)
{
    socket_t client_fd = *(socket_t *)arg;

    char buffer[1024];

    while(1)
    {
        int bytes_received = recv(
            client_fd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if(bytes_received <= 0)
        {
            printf("Server disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        if(strcmp(buffer, "__CLEAR__") == 0)
        {
            printf("\033[2J\033[H");
            printf("[WAYP] Messages cleared.\n");
            fflush(stdout);
        }
        else
        {
            printf("%s", buffer);
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
    inet_pton(AF_INET, "182.16.54.79", &server_addr.sin_addr);
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
        send(client_fd, username, strlen(username), 0);
    while(1) {
        fgets(message, sizeof(message), stdin);
         send(client_fd, message, strlen(message), 0);
    }
    return 0;
}
