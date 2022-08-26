#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048
#define ID "123"

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[20];

void str_overwrite_stdout()
{
    printf("%s > ", ID);
    fflush(stdout);
}

void str_trim_lf(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { // trim \n
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

void catch_ctrl_c_and_exit(int sig)
{
    flag = 1;
}

void send_msg_handler()
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    while (1)
    {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0)
        {
            break;
        }
        else
        {
            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler()
{
    char message[LENGTH] = {};
    while (1)
    {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0)
        {
            printf("%s", message);
            str_overwrite_stdout();
        }
        else if (receive == 0)
        {
            break;
        }
        else
        {
            // -1
        }
        memset(message, 0, sizeof(message));
    }
}

char *toUpper(char *text)
{
    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] >= 'a' && text[i] <= 'z')
        {
            upper_text[i] = text[i] - 32;
        }
    }
    return upper_text;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Insufficient parameters supplied to the command.\n");
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    char command[LENGTH] = {};
    while (1)
    {
        str_overwrite_stdout();
        fgets(command, LENGTH, stdin);
        str_trim_lf(command, LENGTH);
        char token[20];
        char sub_text[LENGTH];

        sscanf(command, "%s %[^\n]", token, sub_text);

        char *upText = toUpper(token);

        if (strcmp(upText, "JOIN") == 0)
        {
            strcpy(name, sub_text);
            str_trim_lf(name, strlen(name));

            struct sockaddr_in server_address;

            /* Socket settings */
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            server_address.sin_family = AF_INET;
            server_address.sin_addr.s_addr = inet_addr(ip);
            server_address.sin_port = htons(port);

            // Connect to Server
            int connect_error = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
            if (connect_error == -1)
            {
                printf("ERROR: connect\n");
                return EXIT_FAILURE;
            }

            // Send name

            send(sockfd, name, 32, 0);

            printf("=== WELCOME TO THE CHATROOM ===\n");
        }
        else if (strcmp(upText, "WHOIS") == 0)
        {
            // do something else
        }
        else if (strcmp(upText, "MSG") == 0)
        {
            // do something else
        }
        else if (strcmp(upText, "TIME") == 0)
        {
            if (token != NULL)
            {
                printf("arg error");
            }

            char *time_str = get_time();
            // printf("Current Time : %s\n", time_str);
        }
        else if (strcmp(upText, "ALIVE") == 0)
        {
            // do something else
        }
        else if (strcmp(upText, "QUIT") == 0)
        {
            // do something else
        }
        else /* default: */
        {
        }
    }

    // printf("Please enter your name: ");
    // fgets(name, 32, stdin);
    // str_trim_lf(name, strlen(name));

    // if (strlen(name) > 32 || strlen(name) < 2)
    // {
    //     printf("Name must be less than 30 and more than 2 characters.\n");
    //     return EXIT_FAILURE;
    // }

    // struct sockaddr_in server_address;

    // /* Socket settings */
    // sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // server_address.sin_family = AF_INET;
    // server_address.sin_addr.s_addr = inet_addr(ip);
    // server_address.sin_port = htons(port);

    // // Connect to Server
    // int connect_error = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    // if (connect_error == -1)
    // {
    //     printf("ERROR: connect\n");
    //     return EXIT_FAILURE;
    // }

    // // Send name
    // send(sockfd, name, 32, 0);

    // printf("=== WELCOME TO THE CHATROOM ===\n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0)
    {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t receive_msg_thread;
    if (pthread_create(&receive_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0)
    {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}
