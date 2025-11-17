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
#include <time.h>
#include "linkedlist.c"
#include "aesdsocket.h"

// defines

#define ERR -1
#define PEER_NAME_LENGTH 50
#define MAX_CHARS_TO_PROCESS_AT_ONCE 100
#define USE_AESD_CHAR_DEVICE 1

// structs, enums

typedef struct 
{
    int acceptedSocketFd;
    struct addrinfo *addrInfo;
    bool *hasReturned;
    FILE *fp;
} threadParameters_s;

static pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t listMutex = PTHREAD_MUTEX_INITIALIZER;   // protects linkedList
static List globalLinkedList = {.listMutex = &listMutex}; // make the list global so acceptor + main can share

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

void ThreadCleanUp(char *buffer, FILE *fsfdr, threadParameters_s *params, char *location)
{
    if(location != NULL)
    {
        printf("Thread Cleanup Location: %s\n", location);
    } 

    if(buffer != NULL)
    {
        free(buffer);
    }

    if(fsfdr != NULL)
    {
        fclose(fsfdr);
    }

    if(params != NULL)
    {
        free(params);
    }
}

void MainCleanUp(struct addrinfo *addrInfo, FILE *fp, char *location)
{
    if(location != NULL)
    {
        printf("Main Clean Up Location: %s\n", location);
    } 

    if(addrInfo != NULL)
    {
        free(addrInfo);
    }

    if(fp != NULL)
    {
        fclose(fp);
    }
}

void MakeProcessDaemon(int argc, char *argv[])
{
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
}

int SetupSocket(struct addrinfo **addrInfo, int *socketFd)
{
    int retVal = 0;
    struct addrinfo hints = {0};
    int flags = 0;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    retVal = getaddrinfo(NULL, "9000", &hints, addrInfo);
    if(retVal != 0)
    {
        fprintf(stderr, "error when getting address information errno: %s\n", gai_strerror(retVal));
        return -1;
    }
    // ===========================================================
    // Get the socket file descriptor
    // ===========================================================

    *socketFd = socket((*addrInfo)->ai_family, (*addrInfo)->ai_socktype, (*addrInfo)->ai_protocol);
    if(*socketFd == ERR)
    {
        fprintf(stderr, "error when getting socket file descriptor\n");
        return -1;
    }

    // ===========================================================
    // Set additional configuration options for the socket
    // ===========================================================

    retVal = setsockopt(*socketFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if(retVal != 0)
    {
        fprintf(stderr, "error when setting socket options\n");
        return -1;
    }

    // ===========================================================
    // Set socket to be non-blocking
    // ===========================================================

    flags = fcntl(*socketFd, F_GETFL, 0);
    fcntl(*socketFd, F_SETFL, flags | O_NONBLOCK);

    // ===========================================================
    // Bind the socket to an address
    // ===========================================================

    retVal = bind(*socketFd, (*addrInfo)->ai_addr, (*addrInfo)->ai_addrlen);
    if(retVal != 0)
    {
        fprintf(stderr, "error when binding the socket\n");
        PrintErrno(errno);
        return -1;
    }

    return 0;
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

void *OperateOnConnection(void* param)
{
    int retVal = -1;
    threadParameters_s *params = (threadParameters_s *)(param);
    char *buffer = NULL;
    FILE *fileSocketFd = NULL;
    unsigned int peerNameLength = PEER_NAME_LENGTH;
    char peerName[PEER_NAME_LENGTH] = {0};
    size_t bufferLength = 0;


    // ===========================================================
    // Log who connected
    // ===========================================================
    retVal = getpeername(params->acceptedSocketFd, (struct sockaddr *)peerName, &peerNameLength);
    if(retVal != 0)
    {

        ThreadCleanUp(buffer, fileSocketFd, params, "1");
        *params->hasReturned = true;
        pthread_exit(&retVal);
    }

    syslog(LOG_INFO, "Accepted connection from %s\n", peerName);


    // make a file descriptor out of the socket
    fileSocketFd = fdopen(params->acceptedSocketFd, "r+");
    if (!fileSocketFd) {
        ThreadCleanUp(buffer, fileSocketFd, params, "2");
        *params->hasReturned = true;
        pthread_exit(&retVal);
    }

    while(getline(&buffer, &bufferLength, fileSocketFd) != -1)
    {
        #ifndef USE_AESD_CHAR_DEVICE
            pthread_mutex_lock(&fileMutex);
        #endif

        // then write them to the file
        fprintf(params->fp, "%s", buffer);
        fflush(params->fp); 
        
        rewind(params->fp);

        // iterate over built up file and send it out the socket
        while(getline(&buffer, &bufferLength, params->fp) != -1)
        {
            // write the packet received back to the client
            fprintf(fileSocketFd, "%s", buffer);
            fflush(fileSocketFd); 
        }
        #ifndef USE_AESD_CHAR_DEVICE
            pthread_mutex_unlock(&fileMutex);
        #endif
    }
    
    *(params->hasReturned) = true;
    ThreadCleanUp(buffer, fileSocketFd, params, "5");
    retVal = 0;
    pthread_exit(&retVal);
}

int CreateThread(struct addrinfo *addrInfo, int acceptedSocketFd, List *linkedList, FILE *fd)
{
    int retVal = 0;
    pthread_t threadId = {0}; 
    threadParameters_s *params = (threadParameters_s*)malloc(sizeof(threadParameters_s));

    // create a thread when a new connection is found
    params->addrInfo = addrInfo;
    params->acceptedSocketFd = acceptedSocketFd;
    params->hasReturned = (bool*)malloc(sizeof(bool));
    *params->hasReturned = false;
    params->fp = fd;

    retVal = pthread_create(&threadId, NULL, OperateOnConnection, params);
    if(retVal != 0)
    {
        fprintf(stderr, "error while creating a new thread\n");
        return -1;
    }

    retVal = (int)list_push(linkedList, threadId, params->hasReturned);
    if(!retVal) // if there is an error
    {
        fprintf(stderr, "error while adding to linked list\n");
        return -1;
    }

    return 0;
}

typedef struct 
{
    int listenfd;
    FILE *fd;
} acceptorParams_s;

static void* AcceptorMain(void* arg)
{
    int retVal = 0;
    acceptorParams_s *params = (acceptorParams_s*) arg;
    int listenfd = params->listenfd;
    FILE *fd = params->fd;

    for (;;) {
        if (!running) break;
        
        int cfd = accept(listenfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR)   continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) { sched_yield(); continue; }
            perror("accept");
            break; // fatal accept failure
        }

        // If you use stdio+getline on this fd, keep it blocking (default).
        // If you previously set the listening socket non-blocking, the accepted
        // fd may inherit flags; ensure blocking for stdio:
        int fl = fcntl(cfd, F_GETFL, 0);
        fcntl(cfd, F_SETFL, fl & ~O_NONBLOCK);

        // Spawn worker and push (CreateThread does the push under listMutex)
        (void)CreateThread(NULL , cfd, &globalLinkedList, fd);
    }
    pthread_exit(&retVal);
}

static void next_wall_10s(struct timespec *t) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    time_t s = (now.tv_sec / 10) * 10 + 10;   // next multiple of 10s
    t->tv_sec  = s;
    t->tv_nsec = 0;
}

typedef struct 
{
    struct timespec next;
    FILE *fp;
}printParams_s;

void* PrintEvery10Seconds(void *params)
{
    printParams_s *printParams = (printParams_s*)params;
    int retVal = 0;

    if (!printParams->fp) {
        // optionally log errno somewhere visible
        return NULL;
    }

    while (running) {
        // sleep until absolute wall clock boundary
        int rc;
        do {
            rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &printParams->next, NULL);
        } while (rc == EINTR && running);

        // format current local time (RFC 2822)
        time_t now = time(NULL);
        struct tm tm_local;
        char buf[64];
        localtime_r(&now, &tm_local);
        strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %z", &tm_local);
        
        pthread_mutex_lock(&fileMutex);
        fprintf(printParams->fp, "timestamp:%s\n", buf);  // <-- newline matters for getline()
        fflush(printParams->fp);                           // ensure it hits the kernel
        pthread_mutex_unlock(&fileMutex);

        // compute the NEXT absolute boundary (re-align each loop)
        next_wall_10s(&printParams->next);
    }

    pthread_exit(&retVal);
}

int main(int argc, char *argv[])
{
    int socketFd = 0;
    int retVal = 0;
    struct timespec next;
    struct addrinfo *addrInfo = NULL;
    pthread_t acceptorTid = 0, timestampTid = 0;
    FILE *fp;
    acceptorParams_s acceptorParams = {0};
    printParams_s printParams = {0};
    
    char fileName[] = 
    #ifndef USE_AESD_CHAR_DEVICE 
        "/var/tmp/aesdsocketdata"
    #else
        "/dev/aesdchar" 
    #endif
    ;
    
    // ===========================================================
    // install the signal handler
    // ===========================================================

    signal(SIGINT, EndProcessSignalHandler);
    signal(SIGTERM, EndProcessSignalHandler);

    MakeProcessDaemon(argc, argv);

    // open up the file to append to
    
    unlink(fileName);
    fp = fopen(fileName, "a+");
    if (fp == NULL) {
        MainCleanUp(addrInfo, fp, "5");
        return -1;
    }

    // start at next 10s boundary from now
    clock_gettime(CLOCK_REALTIME, &next);
    next.tv_sec += 10 - (next.tv_sec % 10);  // align to 10s boundary

    retVal = pthread_mutex_init(&listMutex, NULL);

    // ===========================================================
    // Open and configure sys log
    // ===========================================================
    openlog(NULL, 0, 0);

    // setup socket connection
    retVal = SetupSocket(&addrInfo, &socketFd);
    if(retVal != 0)
    {
        fprintf(stderr, "Error while setting up socket\n");
        MainCleanUp(addrInfo, fp, "1");
        return -1;
        // break;
    }

    // ===========================================================
    // Listen for someone to connect
    // ===========================================================
    retVal = listen(socketFd, 5);
    if(retVal != 0)
    {
        fprintf(stderr, "error while listening for a connection on the socket\n");
        MainCleanUp(addrInfo, fp, "2");
        return -1;
        // break;
    }

    // ===========================================================
    // When a connection has been made to the socket, accept it
    // otherwise keep looping until one has been made, stop running
    // if signal handler was triggered
    // ===========================================================

    acceptorParams.fd = fp;
    acceptorParams.listenfd = socketFd;

    pthread_create(&acceptorTid, NULL, AcceptorMain, &acceptorParams);

    #ifndef USE_AESD_CHAR_DEVICE 
        printParams.fp = fp;
        printParams.next = next;

        pthread_create(&timestampTid, NULL, PrintEvery10Seconds, (void *)&printParams);
    #endif

    // ===========================================================
    // While this process has not been terminated
    // ===========================================================

    ManageThreads(retVal);

    // only get here if the signal was thrown
    syslog(LOG_INFO, "Caught signal, exiting");

    // if the list hasn't been cleared, clean up
    while(globalLinkedList.length != 0)
    {
        pthread_t value;
        bool *hasReturned = NULL;
        list_pop(&globalLinkedList, &value, &hasReturned);
        pthread_join(value, NULL);
        free(hasReturned);
    }

    close(socketFd);

    #ifndef USE_AESD_CHAR_DEVICE 
        pthread_join(timestampTid, NULL);
    #endif

    // clean up the acceptor
    pthread_join(acceptorTid, NULL);

    MainCleanUp(addrInfo, fp, "12");

    sleep(1);

    return 0;
}
void ManageThreads(int retVal)
{
    while (running)
    {
        if (retVal == 0)
        {

            for (unsigned int i = globalLinkedList.length; i > 0; i--)
            {
                pthread_t value;
                bool *hasReturned = NULL;
                list_pop(&globalLinkedList, &value, &hasReturned);

                if (!(*hasReturned)) // if it has not returned
                {

                    // place it back on the list
                    list_push(&globalLinkedList, value, hasReturned);
                }
                else
                {

                    // if it has returned, join on the thread
                    pthread_join(value, NULL);

                    // then free the malloc'd bool
                    free(hasReturned);
                }
            }
        }
        else
        {
            printf("Error when creating thread\n");
        }

        struct timespec ts = {0, 20 * 1000 * 1000}; // ~20 ms tick
        nanosleep(&ts, NULL);
    }
}