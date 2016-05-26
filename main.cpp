#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

/* A simple server in the internet domain using TCP
The port number is passed as an argument */

char* TOKEN = "myToken";

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    // FIXME
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option));

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    const int SHARED_MEMORY_ID = 33;
    const int SHARED_MEMORY_SIZE = 1;
    int shmid;

    int pid;
    while (1) {
        newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr,
                           &clilen);

        pid = fork();
        signal(SIGPIPE, SIG_IGN);

        if (pid < 0) {
            error("ERROR in new process creation");
        }
        if (pid != 0) {
            // Parent process
            shmid = shmget(SHARED_MEMORY_ID, SHARED_MEMORY_SIZE, 0666 | IPC_CREAT);
            bool * is_killed_ptr = (bool *)shmat(shmid, 0, 0);
            bool is_killed = *is_killed_ptr;
            shmdt(is_killed_ptr);

            close(newsockfd);
            if (is_killed) {
                printf("killing parent process");
                close(sockfd);
                return 0;
            }
            continue;
        }

        // Child process
        close(sockfd);

        // Check is killed
        shmid = shmget(SHARED_MEMORY_ID, SHARED_MEMORY_SIZE, 0666 | IPC_CREAT);
        bool * is_killed_ptr = (bool *)shmat(shmid, 0, 0);
        bool is_killed = *is_killed_ptr;
        shmdt(is_killed_ptr);
        if (is_killed) {
            shmctl(shmid, IPC_RMID, NULL);
            close(newsockfd);
            return 0;
        }

        if (newsockfd < 0)
            error("ERROR on accept");
        bzero(buffer,256);

        bool isFirstMessage = true;
        while(1) {
            printf("\nStarted reading...\n");
            bzero(buffer, 256);
//            n = read(newsockfd, buffer, 255);
            n = recv(newsockfd, buffer, 255, MSG_NOSIGNAL);

            if (n < 0) {
                error("ERROR reading from socket");
                exit(1);
            }

            printf("Here is the message: %s\n", buffer);
            int response_len = 256 + 21;
            char response[response_len];
            bzero(response, response_len);

            if(isFirstMessage) {
                printf("first message\n");
                if (strncmp(buffer, TOKEN, strlen(TOKEN)) == 0) {
                    printf("auth successful\n");
                    isFirstMessage = false;
                }
                else{
                    printf("auth ERROR!\n");
                    close(newsockfd);
                    return 1;
                }
            }

            time_t cur_time;
            if (strncmp(buffer, "time", 4) == 0) {
                printf("responding with time\n");
                time(&cur_time);
                sprintf(response, "Current time is==>%ld\n", (long) cur_time);
            } else if (strncmp(buffer, "exit", 4) == 0) {
                printf("exiting\n");
//                shmid = shmget(SHARED_MEMORY_ID, SHARED_MEMORY_SIZE, 0);
//                bool *is_killed = (bool *) shmat(shmid, 0, 0);
//                *(is_killed) = true;
//                shmdt(is_killed);

                close(newsockfd);
                kill(getppid(), 9);
                return 3;
            } else if(strncmp(buffer, "kill", 4) == 0){
                printf("killing\n");
                close(newsockfd);
                return 2;
            }
            else {
                printf("responding with message\n");
                sprintf(response, "I got your message==>%s", buffer);
            }
            n = send(newsockfd, response, response_len, MSG_NOSIGNAL);
            if (n < 0) error("ERROR writing to socket");
        }
        //close(newsockfd);
        //return 0;
    }
}
