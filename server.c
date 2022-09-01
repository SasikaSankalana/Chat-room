#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <signal.h>

#define BUFFER_SZ 288
#define MAX_CLIENTS 10

static _Atomic unsigned int client_count = 0;
static int uid = 10;

int max_no_of_clients;
int max_idle_time;
int user_check = 0;




/* Client structure */
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char fullname[20];
    char name[20];
} client_t;

struct snc_command_holder
{
    char nick_name[20];
    char command[50];
    char sub_command[50];
    char sub_text[256];
} snc_command;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


void str_overwrite_stdout()
{
    printf("\r%s", "> ");
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

void print_client_addr(struct sockaddr_in addr)
{
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < max_no_of_clients; ++i)
    {
        // printf("%d\t%d\t%s\t%s", clients[i]->sockfd, clients[i]->uid, clients[i]->name, clients[i]->fullname);
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < max_no_of_clients; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void broadcast(char *s, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < max_no_of_clients; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

/* Send message to target */
void send_message(char *s, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < max_no_of_clients; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

char *get_time()
{
    time_t current_time = time(NULL);
    char *time_str = ctime(&current_time);
    time_str[strlen(time_str) - 1] = '\0';

    return time_str;
}



/* Handle all communication with the client */
void *handle_client(void *arg)
{

char buff_out[BUFFER_SZ];

fd_set readfds;
struct timeval timeout;
int sret;


    int leave_flag = 0;
    char  name[50];
    char fullname[20];
    char nickname[20];

    client_count++;
    client_t *cli = (client_t *)arg;

    // Name
    if (recv(cli->sockfd, name, 50, 0) <= 0 || strlen(name) <= 0)
    {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    }
    else
    {
      

    sscanf(name, "%s %[^\n]", nickname, fullname);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->fullname, fullname) == 0)
            {
                sprintf(buff_out, "SERVER : NAME ALREADY EXIST.\n");
                send_message(buff_out, cli->uid);
                user_check = 1;
                break;
            }
            else if (strcmp(clients[i]->name, nickname) == 0)
            {
                sprintf(buff_out, "SERVER : NICKNAME ALREADY TAKEN. CHOOSE A DIFFERENT NICKNAME AND JOIN.\n");
                send_message(buff_out, cli->uid);
                user_check = 1;
                break;
            }
        }
    }

    if (!user_check)
    {
        strcpy(cli->name, nickname);
        strcpy(cli->fullname, fullname);

        sprintf(buff_out, "SERVER : %s, Welcome to SNC!\n", cli->name);
        send_message(buff_out, cli->uid);

        bzero(buff_out, BUFFER_SZ);

        sprintf(buff_out, "SERVER : A new user %s has joined SNC!\n", cli->name);
        broadcast(buff_out, cli->uid);
        //user_check = 1;
    }
    }

    bzero(buff_out, BUFFER_SZ);

    while (1)
    {

FD_ZERO(&readfds);
FD_SET(cli->sockfd, &readfds);
timeout.tv_sec = max_idle_time;
timeout.tv_usec = 0;


        if (leave_flag)
        {
            break;
        }
        
        
        sret = select(8, &readfds,NULL,NULL,&timeout);
        if(sret == 0){
        printf("%s timeout\n",cli->name);
        sprintf(buff_out,"SERVER : CONNECTION TERMINATED DUE TO INACTIVITY.\n");
        send_message(buff_out, cli->uid);
        bzero(buff_out, BUFFER_SZ);

            sprintf(buff_out, "SERVER : %s TERMINATED DUE TO INACTIVITY.\n", cli->name);
            broadcast(buff_out, cli->uid);
        leave_flag = 1;
        }
	else{
        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
       
        if (receive >= 0)
        {

            sscanf(buff_out, "%s %s %s %[^\n]", snc_command.nick_name, snc_command.command, snc_command.sub_command, snc_command.sub_text);
            bzero(buff_out, BUFFER_SZ);
        }

        if (receive > 0)
        {

            if (strcmp(snc_command.command, "JOIN") == 0)
            {
            timeout.tv_sec = max_idle_time;

                if (user_check)
                {
                user_check = 0;


    //sscanf(name, "%s %[^\n]", nickname, fullname);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->fullname, snc_command.sub_text) == 0)
            {
                sprintf(buff_out, "SERVER : NAME ALREADY EXIST.\n");
                send_message(buff_out, cli->uid);
                user_check = 1;
                break;
            }
            else if (strcmp(clients[i]->name, snc_command.sub_command) == 0)
            {
                sprintf(buff_out, "SERVER : NICKNAME ALREADY TAKEN. CHOOSE A DIFFERENT NICKNAME AND JOIN.\n");
                send_message(buff_out, cli->uid);
                user_check= 1;
                break;
            }
        }
    }

    if (!user_check)
    {
        strcpy(cli->name, snc_command.sub_command);
        strcpy(cli->fullname, snc_command.sub_text);
        strcpy(nickname, snc_command.sub_command);
            strcpy(fullname, snc_command.sub_text);


        sprintf(buff_out, "SERVER : %s, Welcome to SNC!\n", cli->name);
        send_message(buff_out, cli->uid);

        bzero(buff_out, BUFFER_SZ);

        sprintf(buff_out, "SERVER : A new user %s has joined SNC!\n", cli->name);
        broadcast(buff_out, cli->uid);
        //user_check = 1;
    }
                }
                else
                {
                    sprintf(buff_out, "SERVER : ALREADY JOINED\n");
                    send_message(buff_out, cli->uid);
                }
            }
            else if (strcmp(snc_command.command, "MSG") == 0)
            {
            timeout.tv_sec = max_idle_time;
                int check = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(snc_command.sub_command, clients[i]->name) == 0)
            {
                sprintf(buff_out, "%s : %s\n", cli->name, snc_command.sub_text);
                send_message(buff_out, clients[i]->uid);
                check = 1;
                break;
            }
        }
    }
    if (!check)
    {
        printf("CLIENT \"%s\" NOT REGISTERED\n", snc_command.sub_command);
        sprintf(buff_out, "SERVER : CHECK THE NICKNAME AGAIN\n");
        send_message(buff_out, cli->uid);
    }
            }

            else if (strcmp(snc_command.command, "WHOIS") == 0)
            {
            timeout.tv_sec = max_idle_time;
                int check = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(snc_command.sub_command, clients[i]->name) == 0)
            {
                sprintf(buff_out, "SERVER : %s  %s \n", clients[i]->name, clients[i]->fullname);
                send_message(buff_out, cli->uid);
                check = 1;
                break;
            }
        }
    }
    if (!check)
    {
        printf("CLIENT \"%s\" NOT REGISTERED\n", snc_command.sub_command);
        sprintf(buff_out, "SERVER : CHECK THE NICKNAME AGAIN\n");
        send_message(buff_out, cli->uid);
    }
            }
            else if (strcmp(snc_command.command, "TIME") == 0)
            {
            sprintf(buff_out, "SERVER : %s\n",get_time());
                send_message(buff_out, cli->uid);
                timeout.tv_sec = max_idle_time;
            }
            else if (strcmp(snc_command.command, "ALIVE") == 0)
            {
            sprintf(buff_out, "SERVER : IDLE TIME REINITIALIZED.\n");
                send_message(buff_out, cli->uid);
                timeout.tv_sec = max_idle_time;
            }
        }
        else if (receive == 0 || strcmp(buff_out, "QUIT") == 0)
        {
            sprintf(buff_out, "SERVER : Bye %s!\n", cli->name);
            send_message(buff_out, cli->uid);

            bzero(buff_out, BUFFER_SZ);

            sprintf(buff_out, "SERVER : %s has stopped chatting.\n", cli->name);
            broadcast(buff_out, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }
}
        bzero(buff_out, BUFFER_SZ);
    }

    /* Delete client from queue and yield thread */
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Insufficient parameters supplied to the command.\n");
        return EXIT_FAILURE;
    }

    max_idle_time = atoi(argv[2]);
    max_no_of_clients = atoi(argv[1]);

    if (max_idle_time > 100 || max_idle_time < 1)
    {
        printf("Invalid parameters supplied to the command\n");
        return EXIT_FAILURE;
    }

    if (max_no_of_clients > 10 || max_no_of_clients < 1)
    {
        printf("Invalid parameters supplied to the command\n");
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = 4444;

    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    pthread_t thread_id;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(ip);
    server_address.sin_port = htons(port);

    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    /* Bind */
    if (bind(listenfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0)
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");

    while (1)
    {
        socklen_t client_length = sizeof(client_address);
        connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_length);

        /* Check if max clients is reached */
        if ((client_count + 1) == max_no_of_clients)
        {
            printf("Max clients reached. Rejected: ");
            print_client_addr(client_address);
            printf(":%d\n", client_address.sin_port);
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = client_address;
        cli->sockfd = connfd;
        cli->uid = uid++;

        /* Add client to the queue and fork thread */
        queue_add(cli);
        user_check = 0;
        pthread_create(&thread_id, NULL, &handle_client, (void *)cli);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}
