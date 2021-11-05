#include "27.h"
#include "verify.h"

int exit_flag = 0;

char* recv_ip;
int recv_port;
int server_port;
int num_threads = 0;
pthread_t threads[MAX_THREADS];

void quit_handler(int unused){
  exit_flag = 1;
}

void free_resources_signal(){
    exit_flag = 1;
}

void init_args(int argc, char** argv){
    if(argc < ARGS_MIN){
        printf(USAGE);
        pthread_exit(NULL);
    } else {
        server_port = atoi(argv[1]);
        recv_ip = argv[2];
        recv_port = atoi(argv[3]);
    }
}

void init_signal(){
    static struct sigaction act;
    act.sa_handler = quit_handler;
    sigaction(SIGINT, &act, NULL);
}

void free_thread_res(int sc, int sc2){
    close(sc);
    if(sc2 != NO_SOCK){
        close(sc2);
    }
    pthread_exit(NULL);
}

int init_server(int* sc){
    struct sockaddr_in addr;

    *sc = verify_e(socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL),
        "ssock open", free_resources_signal); CHECK_FLAG;

    memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(server_port);
	addr.sin_addr.s_addr = INADDR_ANY; //all interfaces
    
    verify_e(bind(*sc, (struct sockaddr*)&addr, sizeof(addr)), 
        "ssock bind", free_resources_signal); CHECK_FLAG;
    verify_e(listen(*sc, BACKLOG), 
        "ssock listen", free_resources_signal); CHECK_FLAG;
    return SUCCESS;
}

int init_connection(int* sc){
    struct sockaddr_in addr;

    *sc = verify_e(socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL),
        "ssock open", free_resources_signal); CHECK_FLAG;
    
    memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(recv_port);
	addr.sin_addr.s_addr = inet_addr(recv_ip); //TODO double-check
    verify_e(connect(*sc, (struct sockaddr*)&addr, sizeof(addr)), 
        "ssock connect", free_resources_signal); CHECK_FLAG;
    printf("[%d]: connected to %s:%d\n", *sc, recv_ip, recv_port);
    return SUCCESS;
}

void * connection_body(void * raw_socket);

int spin_server(int sc){
    printf("Spinning server...\n");
    while(!exit_flag){
        int* cl = (int*)malloc(sizeof(int));
        *cl = verify_e(accept(sc, NO_ADDR, NO_ADDR),
            "ssock accept", free_resources_signal); CHECK_FLAG;
        verify(pthread_create(
            threads + num_threads, NULL, connection_body, (void *)cl),
            "create", free_resources_signal); CHECK_FLAG;
    }

    return SUCCESS;
}

int transmit(int read_socket, int write_socket){
    char buf[BUFLEN];
    memset(buf, 0, BUFLEN);
    verify_e(read(read_socket, buf, BUFLEN),
        "read", free_resources_signal); CHECK_FLAG;
    //printf("[%d]: read: %s\n", read_socket, buf);
    verify_e(write(write_socket, buf, strlen(buf)),
        "write", free_resources_signal); CHECK_FLAG;
    return SUCCESS;
}

int spin_connection(int c_sock, int r_sock){
    struct pollfd fds[2];
    fds[0].fd = c_sock;
    fds[1].fd = r_sock;
    fds[0].events = POLLIN | POLLHUP;
    fds[1].events = POLLIN | POLLHUP;

    while(!exit_flag){
        //TODO test if sockets are open, return if not
        verify_e(poll(fds, 2, TIMEOUT),
            "poll", free_resources_signal); CHECK_FLAG;
        printf("[%d/%d]: polled c/r: %d/%d\n", c_sock, r_sock, fds[0].revents, fds[1].revents);
        if((fds[0].revents & POLLHUP) != 0 || (fds[1].revents & POLLHUP) != 0){
            printf("[%d/%d]: terminating session..", c_sock, r_sock);
            fflush(stdout);
            free_thread_res(c_sock, r_sock);
        }
        if(fds[0].revents != 0){
            transmit(c_sock, r_sock);
        }
        if(fds[1].revents != 0){
            transmit(r_sock, c_sock);
        }
    }

    return SUCCESS;
}

void * connection_body(void * raw_socket){
    int c_sock = *((int *) raw_socket); // client socket
    free((int*)raw_socket);
    int r_sock = NO_SOCK; // receiver socket
    if(init_connection(&r_sock)){
        spin_connection(c_sock, r_sock);
    }
    free_thread_res(c_sock, r_sock);
}

void join_threads(){
    for(int i = 0; i < num_threads; i++){
        verify(pthread_join(threads[i], NULL),
            "join", free_resources_signal);
    }
}

int main(int argc, char** argv){
    int server_sc;
    //printf("%d,%d,%d", POLLERR, POLLHUP, POLLIN);

    init_args(argc, argv);
    init_signal();
    if(init_server(&server_sc)){
        spin_server(server_sc);
    }
    free_resources_signal();
    join_threads();
    free_thread_res(server_sc, NO_SOCK);
}