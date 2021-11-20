#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

#define USAGE "Usage: local port + receiving ip + port\n"
#define ARGS_MIN 4
#define BUFSIZE 1024
#define DEFAULT_PROTOCOL 0

#define CHECK_FLAG if(exit_flag){ printf("%d\n", num_threads); return 0; }
#define NO_SOCK -1
#define NO_ADDR NULL
#define MAX_THREADS 510
#define TIMEOUT 3000
#define SUCCESS 1

