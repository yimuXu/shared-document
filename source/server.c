// TODO: server code that manages the document and handles client instructions
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include "markdown.h"

#define MAX_CLIENT 10
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


struct clientpipe{
    int c2sfd;
    int s2cfd;
    char * c2sname;
    char * s2cname;
    char* username;
    int clientpid;
    struct epoll_event ev;

};
char* bufferdoc;
uint64_t* size;  // size of document
struct clientpipe* clients;
int epollfd;
document* doc;//// store the old document and send to client server side copy of document
int count = 0; // count of clients

int checkauthorisation(char* username, int c2sfd, int s2cfd){
    // check if username is valid
    (void) s2cfd;(void) c2sfd;
    FILE* fp = fopen("roles.txt", "r");
    if(fp == NULL){
        printf("open roles.txt error!\n");
        return 1;
    }
    char line[256];
    while(fgets(line, sizeof(line), fp)){
        username[strcspn(username, "\n")] = 0; // remove  newline character username from client
        //line[strcspn(line, "\n")] = 0; // remove newline character of file
        char* token = strtok(line, " \t");
        if(strcmp(token, username) == 0){
            token = strtok(NULL, " \t");
            printf("token: %s", token);/// for debug
            write(s2cfd, token, strlen(token)+1);
            fclose(fp);
            return 0;
        }

    }
    write(s2cfd,"Reject UNAUTHORISED\n", 21); //debug
    printf("Reject UNAUTHORISED\n");
    sleep(1);
    fclose(fp);
    return 1;

}

//make fifo for communication
void *makefifo(void* arg, char* c2sname, char* s2cname){
    // create FIFO for communication
    int* clientpid = (int*)arg;
    char name1[] = "FIFO_C2S_";
    char name2[] = "FIFO_S2C_";
    char c2sn[100];
    char s2cn[100];
    sprintf(c2sn, "%s%d",name1,*clientpid);
    sprintf(s2cn, "%s%d",name2,*clientpid);
    c2sname = c2sn;
    s2cname = s2cn;
    unlink(c2sname);
    unlink(s2cname);
    mode_t perm = 0666;
    
    int rt1 = mkfifo(c2sname, perm);
    int rt2 = mkfifo(s2cname, perm);
    if(rt1 == -1){
        printf("mkfifo error!\n");
        return NULL;
    }
    if(rt2 == -1){
        printf("mkfifo error!\n");
        return NULL;
    }
    printf("FIFO created!\n");
    

    return NULL;
}

// add client
void addclient(int c2sfd, int s2cfd, char* username, int clientpid, char* c2s, char* s2c){
    pthread_mutex_lock(&mutex);
    clients[count].c2sfd = c2sfd;
    clients[count].s2cfd = s2cfd;
    clients[count].username = malloc(strlen(username)+1);
    strcpy(clients[count].username, username);
    clients[count].clientpid = clientpid;
    clients[count].c2sname = malloc(strlen(c2s)+1);
    strcpy(clients[count].c2sname, c2s);
    clients[count].s2cname = malloc(strlen(s2c)+1);
    strcpy(clients[count].s2cname, s2c);
    clients[count].ev.events = EPOLLIN;
    clients[count].ev.data.fd = c2sfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, c2sfd, &clients[count].ev);
    count++;
    printf("client %d added in clients! and add event\n", clientpid);
    pthread_mutex_unlock(&mutex);
}
// delete client from list
void deleteclient(int clientpid){
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < count; i++){
        if(clients[i].clientpid == clientpid){
            epoll_ctl(epollfd, EPOLL_CTL_DEL, clients[i].c2sfd, NULL);
            free(clients[i].username);
            close(clients[i].c2sfd);
            close(clients[i].s2cfd);
            unlink(clients[i].c2sname);
            unlink(clients[i].s2cname);
            free(clients[i].c2sname);
            free(clients[i].s2cname);
            for(int j = i; j < count-1; j++){
                clients[j] = clients[j+1];
            }
            count--;
            
            printf("client %d deleted from clients!\n", clientpid);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void* communication_thread(void* arg){
    char c2s[256];
    char s2c[256];
    int* clientpid = (int*)arg;
    makefifo(clientpid, c2s, s2c);
    // send signal to client
    kill(*clientpid, SIGRTMIN+1);
    // open FIFO
    int c2sfd = open(c2s, O_RDONLY);
    int s2cfd = open(s2c, O_WRONLY);
    if(c2sfd == -1){
        printf("open FIFO_C2S error!\n");
        return NULL;
    }
    if(s2cfd == -1){
        printf("open FIFO_S2C error!\n");
        return NULL;
    }
    // read username from client
    char username[256];
    read(c2sfd, username, 256);
    int rt = checkauthorisation(username, c2sfd, s2cfd);
    if(rt == 1){
        close(c2sfd);
        close(s2cfd);
        unlink(c2s);
        unlink(s2c);
        return NULL;
    }

    // add client to list// add event to epoll
    addclient(c2sfd, s2cfd, username, *clientpid, c2s, s2c);

    char* bufferdoc = markdown_flatten(doc);
    (void)bufferdoc;
    write(s2cfd, doc, sizeof(doc));
    // while(1){
    //     char buffer[256];
    //     read(c2sfd, buffer, 256);
    //     printf("read from FIFO: %s\n", buffer);
    //     //handle edit command
    //     handle_edit_command(buffer, c2sfd);
    // }
    //printf("unlink!\n");
    //delete client from list and from event
    //deleteclient(*clientpid);// unlink has done in function
    return NULL;

}

//function to handle editing commands from client
int handle_edit_command(char* command, int c2sfd){
    char* commandtype = strtok(command, " ");(void)c2sfd;
    if(strcmp(commandtype, "INSERT") == 0){
        // insert command
        int pos = atoi(strtok(NULL, " "));
        char* content = strtok(NULL, "");
        // call the insert function //////////////////////
        printf("insert %s at %d\n", content, pos);
        // call insert function
    }else if(strcmp(commandtype, "DELETE") == 0){
        // delete command
        int pos = atoi(strtok(NULL, " "));
        int len = atoi(strtok(NULL, " "));
        // call the delete function //////////////////////
        printf("delete %d at %d\n", len, pos);
    }else if(strcmp(commandtype, "DISCONNECT\n") == 0){
        // edit command
        return -1; // disconnect client
    }
    return 0;

}

// thread handle epoll event
void* epoll_handle_thread(void* arg) {
    int epollfd = *(int*)arg;

    struct epoll_event events[10];
    while(1){
        int n = epoll_wait(epollfd, events, 10, 0);
        if(n == -1){
            perror("epoll_wait");
            return NULL;
        }
        for(int i = 0; i < n; i++){
            if(events[i].events & EPOLLIN){
                // read from FIFO
                int c2sfd = events[i].data.fd;
                char buffer[256];
                read(c2sfd, buffer, 256);
                printf("events read from FIFO: %s\n", buffer);
                // handle edit command
                int result = handle_edit_command(buffer, c2sfd);
                if(result == -1) {
                    // disconnect client
                    printf("client disconnected!\n");
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, c2sfd, NULL);
                    deleteclient(clients[i].clientpid);
                    
                }
            }
        }
    }
    return NULL;
}
// thread to broadcast message to all clients
void broadcast_to_all_clients(const char* msg) {
    pthread_mutex_lock(&mutex);
    char buf[sizeof(uint64_t)];
    for (int i = 0; i < count; ++i) {
        sprintf(buf, "%ld\n%s", doc->size, msg);
        write(clients[i].s2cfd, msg, strlen(msg) + sizeof(uint64_t) + 1);
    }
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char** argv){
    // check if user input time interval for update document
    if(argc != 2){
        printf("input time interval!\n");
        return 1;
    }
    int serverpid = getpid();
    printf("Server PID: %d\n", serverpid);
    int time_interval = atoi(argv[1]);

    // create struct to store client information
    struct clientpipe* clients = malloc(sizeof(struct clientpipe) * MAX_CLIENT);
    int count = 0; // count of clients

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        return 1;
    }    
    pthread_t hdevent;
    pthread_create(&hdevent, NULL, epoll_handle_thread, &epollfd);
    pthread_detach(hdevent);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigprocmask(SIG_BLOCK, &set, NULL);    

    time_t last_broadcast_time = time(NULL);
    while(1){
        struct timespec timeout = {0, 0};
        siginfo_t info;

        int sig = sigtimedwait(&set, &info, &timeout);
        int clientpid = info.si_pid;
        if (sig == -1 && errno != EAGAIN) {
            perror("sigtimedwait");
            return 1;
        }else if (sig == SIGRTMIN) {
            pthread_t thread;
        // initilaize thread
            pthread_create(&thread, NULL, communication_thread, &clientpid);      
            pthread_detach(thread);            
        }
        // broadcast message to all clients every time_interval seconds
        time_t current_time = time(NULL);
        if(current_time - last_broadcast_time >= time_interval) {
            last_broadcast_time = current_time;
            markdown_increment_version(doc);
            bufferdoc = markdown_flatten(doc);
            broadcast_to_all_clients(bufferdoc);
        }
        char quit[256];
        if(fgets(quit, 256, stdin) != NULL){
            if(strcmp(quit, "QUIT\n") == 0){
                if( count == 0){
                    free(clients);
                    //break;
                }else{
                    printf("QUIT rejected, %d clients still connected.\n", count);
                }
            }
        }
    }
    //pthread_join(thread, NULL);  
    // close epoll instance
    close(epollfd);
    return 0;
    
}