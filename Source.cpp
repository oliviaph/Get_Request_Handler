/*
Olivia Houghton
CS 371-001
Programming Assignment 1
2/26/16
*/

#include "TCPEchoServer.h"  /* TCP echo server includes */
#include <sys/wait.h>       /* for waitpid() */
#include <string>
#include <iostream>

int main(int argc, char *argv[])
{
    int servSock;                    /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */
    unsigned short ServPort;     /* Server port */
    pid_t processID;                 /* Process ID from fork() */
    unsigned int childProcCount = 0; /* Number of child processes */ 
	std::string directory = argv[2];
    if (argc != 3)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server Port Directory>\n", argv[0]);
        exit(1);
    }

    ServPort = atoi(argv[1]);  /* First arg:  local port */

    servSock = CreateTCPServerSocket(ServPort);

    for (;;) /* Run forever */
    {
        clntSock = AcceptTCPConnection(servSock);
        /* Fork child process and report any errors */
        if ((processID = fork()) < 0)
            DieWithError("fork() failed");
        else if (processID == 0)  /* If this is the child process */
        {
            close(servSock);   /* Child closes parent socket */
            HandleTCPClient(clntSock, directory);

            exit(0);           /* Child process terminates */
        }

        printf("with child process: %d\n", (int) processID);
        close(clntSock);       /* Parent closes child socket descriptor */
        childProcCount++;      /* Increment number of outstanding child processes */

        while (childProcCount) /* Clean up all zombies */
        {
            processID = waitpid((pid_t) -1, NULL, WNOHANG);  /* Non-blocking wait */
            if (processID < 0)  /* waitpid() error? */
                DieWithError("waitpid() failed");
            else if (processID == 0)  /* No zombie to wait on */
                break;
            else
                childProcCount--;  /* Cleaned up after a child */
        }
    }
    /* NOT REACHED */
}
