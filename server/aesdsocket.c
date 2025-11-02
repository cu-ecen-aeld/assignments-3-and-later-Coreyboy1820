#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "linkedlist.c"

// defines

#define ERR -1
#define PEER_NAME_LENGTH 50
#define MAX_CHARS_TO_PROCESS_AT_ONCE 100

// globals

// used to flag when a signal was asserted to stop processing
volatile unsigned int running = 1;

// functions

void EndProcessSignalHandler(int signum)
{
    running = 0;
}

void PrintErrno(int err) {
    fprintf(stderr, "Error %d: %s\n", err, strerror(err));
}

void MainCleanUp(char *buffer, struct addrinfo *addrInfo, FILE *fp, FILE *fsfdr, FILE *fsfdw, char *location)
{
    if(buffer != NULL)
    {
        free(buffer);
    }

    if(addrInfo != NULL)
    {
        freeaddrinfo(addrInfo);
    }

    if(fp != NULL)
    {
        fclose(fp);
        remove("/var/tmp/aesdsocketdata");
    }

    if(fsfdr != NULL)
    {
        fclose(fsfdr);
    }

    if(fsfdw != NULL)
    {
        fclose(fsfdw);
    }

    if(errno)
    {
        PrintErrno(errno);
    }
}

int WaitForAndAcceptConnection(int socketFd)
{
    unsigned int addrSize = 0;
    struct sockaddr_storage their_addr = {0};
    int acceptedSocketFd = 0;
    
    addrSize = sizeof(their_addr);
    do
    {
        // get the file descriptor for the new connection
        acceptedSocketFd = accept(socketFd, (struct sockaddr *)&their_addr, &addrSize);
        if(acceptedSocketFd == -1)
        {
            if( (errno == EAGAIN) || (errno == EAGAIN) )
            {
                continue;
            }
            else
            {
                return -1;
            }
        }
        
        break;

    } while ( running );

    return acceptedSocketFd;
}

typedef struct 
{
    int acceptedSocketFd;
    struct addrinfo *addrInfo;
    pthread_mutex_t *mutex;
    bool *hasReturned;
} threadParameters_s;

void *AcceptConnectionThenOperate(void* param)
{
    int retVal = -1;
    threadParameters_s *params = (threadParameters_s *)(param);
    char *buffer = NULL;
    FILE *fp = NULL;
    FILE *fileSocketFdRead = NULL;
    FILE *fileSocketFdWrite = NULL;
    unsigned int peerNameLength = PEER_NAME_LENGTH;
    char peerName[PEER_NAME_LENGTH] = {0};
    size_t bufferLength = 0;


    // ===========================================================
    // Log who connected
    // ===========================================================
    retVal = getpeername(params->acceptedSocketFd, (struct sockaddr *)peerName, &peerNameLength);
    if(retVal != 0)
    {

        MainCleanUp(buffer, params->addrInfo, fp, NULL, NULL, "10");
        pthread_exit(&retVal);
    }

    syslog(LOG_INFO, "Accepted connection from %s\n", peerName);


    // make a file descriptor out of the socket
    fileSocketFdRead = fdopen(params->acceptedSocketFd, "r");
    if (!fileSocketFdRead) {
        MainCleanUp(buffer, params->addrInfo, fp, fileSocketFdRead, fileSocketFdWrite, "13");
        pthread_exit(&retVal);
    }

    fileSocketFdWrite = fdopen(params->acceptedSocketFd, "w");
    if (!fileSocketFdWrite) {
        MainCleanUp(buffer, params->addrInfo, fp, fileSocketFdRead, fileSocketFdWrite, "13");
        pthread_exit(&retVal);
    }
    
    // open up the file to append to
    fp = fopen("/var/tmp/aesdsocketdata", "a+");
    if (fp == NULL) {
        MainCleanUp(buffer, params->addrInfo, fp, fileSocketFdRead, fileSocketFdWrite, "11");
        pthread_exit(&retVal);
    }

    while(getline(&buffer, &bufferLength, fileSocketFdRead) != -1)
    {
        pthread_mutex_lock(params->mutex);

        // then write them to the file
        fprintf(fp, "%s", buffer);
        fflush(fp); 
        
        rewind(fp);
        
        pthread_mutex_unlock(params->mutex);

        // iterate over built up file and send it out the socket
        while(getline(&buffer, &bufferLength, fp) != -1)
        {
            // write the packet received back to the client
            fprintf(fileSocketFdWrite, "%s", buffer);
            fflush(fileSocketFdWrite); 
        }
    }

    retVal = 0;
    pthread_exit(&retVal);
}

int main(int argc, char *argv[])
{
    int socketFd = 0;
    int retVal = 0;
    struct addrinfo hints;
    struct addrinfo *addrInfo = NULL;
    int yes = 1;
    int flags = 0;
    pthread_t threadId = {0}; 
    threadParameters_s params = {0};
    pthread_mutex_t fileMutex;
    int acceptedSocketFd = 0;

    List linkedList = {0};

    // ===========================================================
    // install the signal handler
    // ===========================================================

    signal(SIGINT, EndProcessSignalHandler);
    signal(SIGTERM, EndProcessSignalHandler);

    // ===========================================================
    // Open and configure sys log
    // ===========================================================
    openlog(NULL, 0, 0);

    // ===========================================================
    // configure type of socket
    // ===========================================================

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    retVal = getaddrinfo(NULL, "9000", &hints, &addrInfo);
    if(retVal != 0)
    {
        fprintf(stderr, "error when getting address information errno: %s\n", gai_strerror(retVal));
        MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "4");
        return -1;
    }

    // ===========================================================
    // Ge the socket file descriptor
    // ===========================================================

    socketFd = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
    if(socketFd == ERR)
    {
        MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "5");
        fprintf(stderr, "error when getting socket file descriptor\n");
        return -1;
    }

    // ===========================================================
    // Set additional configuration options for the socket
    // ===========================================================

    retVal = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if(retVal != 0)
    {
        MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "6");
        return -1;
    }

    // ===========================================================
    // Set socket to be non-blocking
    // ===========================================================

    flags = fcntl(socketFd, F_GETFL, 0);
    fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);

    // ===========================================================
    // Bind the socket to an address
    // ===========================================================

    retVal = bind(socketFd, addrInfo->ai_addr, addrInfo->ai_addrlen);
    if(retVal != 0)
    {
        MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "7");
        return -1;
    }

    // ===========================================================
    // Once binded, make the process a daemon if selected to
    // ===========================================================
    if(argc > 1)
    {
        if(!strcmp(argv[1], "-d"))
        {
            // fork to make a child process and exit the parent
            pid_t pid = fork();
            if(pid != 0)
            {
                printf("%d\n",pid);
                exit(EXIT_SUCCESS);
            }
            
            // set the session id to the child
            setsid();

            // change the daemon to being in the root directory to not block
            // unmounting a directory
            chdir("/");

            // redirect stdin, stdout, and fileno to dev/null
            // to get rid of all output
            int fd = open("/dev/null", O_RDWR);
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        }
    }

    // ===========================================================
    // While this process has not been terminated
    // ===========================================================

    while(running)
    {

        // ===========================================================
        // Listen for someone to connect
        // ===========================================================
        retVal = listen(socketFd, 5);
        if(retVal != 0)
        {
            fprintf(stderr, "error while listening for a connection on the socket\n");
            MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "8");
            return -1;
            // break;
        }

        // ===========================================================
        // When a connection has been made to the socket, accept it
        // otherwise keep looping until one has been made, stop running
        // if signal handler was triggered
        // ===========================================================
        acceptedSocketFd = WaitForAndAcceptConnection(socketFd);
        if(acceptedSocketFd == -1)
        {

            MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "9");
            return -1;
        }

        retVal = pthread_mutex_init(&fileMutex, NULL);
        if(retVal != 0)
        {
            fprintf(stderr, "error while initing the mutex\n");
            MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "8");
            return -1;
            // break;
        }

        // create a thread when a new connection is found
        params.addrInfo = addrInfo;
        params.acceptedSocketFd = acceptedSocketFd;
        params.mutex = &fileMutex;
        params.hasReturned = malloc(sizeof(bool));

        retVal = pthread_create(&threadId, NULL, AcceptConnectionThenOperate, &params);
        if(retVal != 0)
        {
            fprintf(stderr, "error while creating a new thread\n");
            // break;
        }

        retVal = (int)list_push(&linkedList, threadId, params.hasReturned);
        if(!retVal) // if there is an error
        {
            fprintf(stderr, "error while adding to linked list\n");
        }

        for(unsigned int i = 0; i < linkedList.length; i++)
        {
            int value;
            bool *hasReturned;
            list_pop(&linkedList, &value, &hasReturned);
            if(!(*hasReturned)) // if it has not returned, place it back on the list
            {
                list_push(&linkedList, value, hasReturned);
            }
            else
            {
                pthread_join(value, NULL);
            }
        }

    }

    // only get here if the signal was thrown
    syslog(LOG_INFO, "Caught signal, exiting");

    MainCleanUp(NULL, addrInfo, NULL, NULL, NULL, "12");

    return 0;
}