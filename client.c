#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>


void *receive_messages(void *arg)
{
    int client_fd = *(int *)arg;

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
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
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
    int *socket_ptr = malloc(sizeof(int));

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
