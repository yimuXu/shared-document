// TODO: server code that manages the document and handles client instructions
//#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_C_SOURCE 
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include "markdown.h"

#define MAX_CLIENT 10
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define QUEUE_SIZE 100

typedef struct {
    char* data;
    int authorisation; //0 is write, 1 is read
    int premission;//
    char* username;
    int reject;
    struct timespec timestamp;// if reject, this store the reject type;
}msginfo;

typedef struct {
    msginfo* msg[QUEUE_SIZE];
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

} msg_q;

typedef struct versionlog {
    int version;
    size_t len;
    char* editlog; // current version command log
    struct versionlog* next;
}versionlog;

versionlog* log_head;
versionlog* log_tail;


msg_q queue = {.front = 0, .rear = 0, .count = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

struct clientpipe{
    int c2sfd;
    int s2cfd;
    char * c2sname;
    char * s2cname;
    char* username;
    int clientpid;

};
char* bufferdoc;// doc size + text
uint64_t docsize;  // size of document
struct clientpipe* clients;
document* doc;//// store the old document and send to client server side copy of document
int clientcount; // count of clients
char* editlog;
char* dcdata;

int quit_edit;

void versionline_free(){
    versionlog* cur = log_head;
    versionlog* temp = log_head;
    while(cur){
        temp = cur->next;
        free(cur->editlog);
        free(cur);
        cur = temp;
    }
}
int checkauthorisation(char* username, int c2sfd, int s2cfd, int* rw_flag){
    // check if username is valid
    (void) s2cfd;(void) c2sfd;
    FILE* fp = fopen("source/roles.txt", "r");
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
            char* edit = strtok(NULL, " \t");
            //printf("editstatus: %s\n", edit);/// for debug
            write(s2cfd, edit, strlen(edit)+1);
            if(strncmp(edit,"write",strlen(edit)+1) == 0){
                *rw_flag = 0;
            }else{
                *rw_flag = 1;
            }
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
    sprintf(c2sname, "%s%d",name1,*clientpid);
    sprintf(s2cname, "%s%d",name2,*clientpid);
    printf("c2s:%s",c2sname);
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
    printf("FIFO created: %s, %s!\n",c2sname,s2cname);
    

    return NULL;
}

void queue_push(msginfo* msg) {
    pthread_mutex_lock(&queue.mutex);
    if(queue.count < QUEUE_SIZE) {
        printf("queue push msg into queue\n");
        queue.msg[queue.rear] = msg;
        queue.rear = (queue.rear + 1) %  QUEUE_SIZE;
        queue.count++;
        pthread_cond_signal(&queue.cond);
    }
    pthread_mutex_unlock(&queue.mutex);
}


// add client
struct clientpipe* addclient(int c2sfd, int s2cfd, char* username, int clientpid, char* c2s, char* s2c){
    pthread_mutex_lock(&mutex);
    struct clientpipe* new_client = &clients[clientcount];
    clients[clientcount].c2sfd = c2sfd;
    clients[clientcount].s2cfd = s2cfd;
    clients[clientcount].username = malloc(strlen(username)+1);
    strcpy(clients[clientcount].username, username);
    clients[clientcount].clientpid = clientpid;
    clients[clientcount].c2sname = malloc(strlen(c2s)+1);
    strcpy(clients[clientcount].c2sname, c2s);
    clients[clientcount].s2cname = malloc(strlen(s2c)+1);
    strcpy(clients[clientcount].s2cname, s2c);
    clientcount++;
    printf("client %d added in clients! and add event\n", clientpid);
    pthread_mutex_unlock(&mutex);
    return new_client;
}
// delete client from list
void deleteclient(char* username){
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < clientcount; i++){
        if(strcmp(clients[i].username,username) == 0){
            free(clients[i].username);
            close(clients[i].c2sfd);
            close(clients[i].s2cfd);
            unlink(clients[i].c2sname);
            unlink(clients[i].s2cname);
            free(clients[i].c2sname);
            free(clients[i].s2cname);
            for(int j = i; j < clientcount-1; j++){
                clients[j] = clients[j+1];
            }
            clientcount--;
            printf("client %s deleted from clients!\n", username);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void append_to_editlog(char* log_line){
    versionlog* newlog = malloc(sizeof(versionlog));
    newlog->len = strlen(log_line);
    newlog->editlog = malloc(newlog->len+1);
    newlog->editlog[newlog->len] = '\0';
    log_tail->next = newlog;
    log_tail = newlog;
}

//function to handle editing commands from client
int handle_edit_command() {

    pthread_mutex_lock(&queue.mutex);
    while (queue.count == 0 && !quit_edit) {
        pthread_cond_wait(&queue.cond, &queue.mutex);

    }
    if(quit_edit){
        pthread_mutex_unlock(&queue.mutex);
        return 1;
    }
    printf("handle the first command in queue!\n");
    //copy and free 
    char *username_copy = strdup(queue.msg[queue.front]->username);
    char *data_copy = strdup(queue.msg[queue.front]->data);
    msginfo* current = queue.msg[queue.front];// copy current msginfo
    int premission = current->premission;
    free(queue.msg[queue.front]->username);
    free(queue.msg[queue.front]->data);
    free(queue.msg[queue.front]);
    
    queue.front = (queue.front + 1) % QUEUE_SIZE;
    queue.count--;
    pthread_mutex_unlock(&queue.mutex);
    char log_line[256];
    if(premission != 0) {
        snprintf(log_line,256, "EDIT %s %s %s %s",username_copy,data_copy,"Reject","UNAUTHORISED");
        append_to_editlog(log_line);
        return 0;
    }

    char* commandtype = strtok(data_copy, " ");
    if(strcmp(commandtype, "INSERT") == 0){
        // insert command
        int pos = atoi(strtok(NULL, " "));
        char* content = strtok(NULL, "");
        content[strcspn(content, "\n")] = 0;
        // call the insert function //////////////////////
        printf("insert %s at %d\n", content, pos);
        // call insert function
        int sta = markdown_insert(doc,doc->version,pos, content);
        if(sta == 0){
            printf("insert success\n");
            snprintf(log_line,256,"EDIT %s %s SUCCESS",username_copy, data_copy);
        }else if(sta == -1){

            printf("insert failed: position out of range\n");
            snprintf(log_line, 256, "EDIT %s %s %s %s", username_copy, data_copy, "Reject", "INVALID_POSITION");
        }
    }else if(strcmp(commandtype, "DELETE") == 0){
        // delete command
        int pos = atoi(strtok(NULL, " "));
        int len = atoi(strtok(NULL, " "));

        // call the delete function //////////////////////
        printf("delete %d at %d\n", len, pos);
    }else if(strncmp(data_copy, "DISCONNECT\n",11) == 0){
        // disconnect client
        printf("delete user\n");
        deleteclient(username_copy);
    }
    append_to_editlog(log_line);
    free(username_copy);
    free(data_copy);
    return 0;

}

// thread handle priority queue event
void* queue_handle_thread(void* arg) {
    (void)arg;
    printf("start to handle edit command!\n");
    while(1) {
        if(quit_edit == 1){
            break;
        }
        handle_edit_command();
    }
    printf("quit the queue handle thread\n");
    return NULL;
}
// thread to broadcast message to all clients  and update the verison and do
void* broadcast_to_all_clients_thread(void* arg) {
    int interval = *(int*)arg;
    printf("broadcast is running!\n");
    while(1){
        if(quit_edit == 1){
            break;
        }
        usleep(interval*1000);
        //printf("broadcast to clients!\n");
        pthread_mutex_lock(&queue.mutex);
        while (queue.count > 0) {
            pthread_cond_wait(&queue.cond, &queue.mutex);
        }
        pthread_mutex_unlock(&queue.mutex);  
        pthread_mutex_lock(&mutex);
        //edit thel log 
        char* end = "END\n";
        char* bufversion = "VERSION";
        size_t len = strlen(bufversion) + sizeof(int) + 3;
        char* versionline = malloc(len);
        
        append_to_editlog(end);
        markdown_increment_version(doc);
        int verisonnum = doc->version + 1;
        snprintf(versionline,len,"%s %d\n",bufversion,verisonnum);
        append_to_editlog(versionline);
        free(versionline);
        

        // for(int i = 0; i< clientcount;i++){       do not delete!!!!!!!!!!!!!!!
        //     if(clients->s2cfd){
        //         int fd = clients[i].s2cfd;
        //         write(fd,editlog,strlen(editlog));            
        //     }            
        // }

        pthread_mutex_unlock(&mutex);
              
    }
    printf("quit the broadcast\n");
    return NULL;
}

void* servercom(void* arg){

    (void)arg;
    char quit[256];
    printf("server side debug is running!\n");
    while(1){
        if(fgets(quit, 256, stdin)){
            if(strcmp(quit, "QUIT\n") == 0){
                if( clientcount == 0){
                    FILE* fp = fopen("doc.md","w");
                    markdown_print(doc,fp);
                    fclose(fp);
                    free(clients);
                    pthread_mutex_lock(&mutex);
                    quit_edit = 1;
                    pthread_cond_broadcast(&queue.cond); // wake up
                    pthread_mutex_unlock(&mutex);
                    break;
                }else{
                    printf("QUIT rejected, %d clients still connected.\n", clientcount);
                }
            }else if(strcmp(quit, "DOC?\n") == 0){
                printf("%s",quit);
                printf("%s",dcdata);
            }else if(strcmp(quit, "LOG?\n")== 0){
                printf("print log!\n");
                printf("%s", dcdata);
            }            
        }

    } 
    printf("quit the server command loop\n");
    return NULL;
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
    printf("pipe: %s, %s set!\n",c2s,s2c);
    // read username from client
    char username[256];
    int rw_flag = -1;//read or write
    read(c2sfd, username, 256);
    int rt = checkauthorisation(username, c2sfd, s2cfd, &rw_flag);
    if(rt == 1){
        close(c2sfd);
        close(s2cfd);
        unlink(c2s);
        unlink(s2c);
        return NULL;
    }

    // add client to list// add event to epoll
    addclient(c2sfd, s2cfd, username, *clientpid, c2s, s2c);
    
    dcdata = markdown_flatten(doc);
    char* perm;
    if(rw_flag == 0){
        perm = "write";
    }else if (rw_flag == 1)
    {
        perm = "read";
    }
    
    size_t needed = snprintf(NULL, 0, "%s\nVERSION %ld\n%ld\n%s\n",perm, doc->version+1, (uint64_t)strlen(dcdata), dcdata) + 1;
    bufferdoc = malloc(needed);
    if (!bufferdoc) {
        perror("malloc bufferdoc");
        // handle error
    }
    snprintf(bufferdoc, needed, "%s\nVERSION %ld\n%ld\n%s\n",perm,doc->version +1, (uint64_t)strlen(dcdata), dcdata);
    int x = write(s2cfd, bufferdoc, strlen(bufferdoc));
    if (x == -1) {
        printf("write error:\n");
    }
    free(dcdata);
    free(bufferdoc);
    
    // int epollfd = epoll_create1(0);
    // if(epollfd == -1){
    //     perror("epoll_create1");
    // }
    
    // epoll 
    // struct epoll_event ev, events[10];
    // ev.events = EPOLLIN;
    // ev.data.fd = c2sfd;
    // epoll_ctl(epollfd, EPOLL_CTL_ADD, c2sfd, &ev);
    while(1){
        // int n = epoll_wait(epollfd, events, 10, -1);
        // for(int i = 0; i < n; i++) {
        //     if(events[i].events & EPOLLIN){
        //         int fd = ev.data.fd;
                char buf[256];
                read(c2sfd,buf,256);
                msginfo* new_msg = malloc(sizeof(msginfo));
                size_t len =strlen(buf);
                new_msg->data = malloc(len+1);
                strncpy(new_msg->data,buf,len);
                new_msg->data[len] = '\0';
                //new_msg->data[255] = '\0';
                clock_gettime(CLOCK_REALTIME, &new_msg->timestamp);

                username[strcspn(username, "\n")] = 0;
                int name_len = strlen(username);
                new_msg->username = malloc(name_len+1);
                strncpy(new_msg->username, username,name_len);
                new_msg->username[name_len] = '\0';
                new_msg->premission = rw_flag;
                new_msg->reject = -1;
                queue_push(new_msg);
                
                if(strncmp(new_msg->data, "DISCONNECT\n", 11) == 0){
                    break;
                }
        //      }
        // }        
    }
    printf("quit the communication thread\n");

    return NULL;

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
    clients = malloc(sizeof(struct clientpipe) * MAX_CLIENT);
    clientcount = 0; // count of clients
    
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigprocmask(SIG_BLOCK, &set, NULL);       



    doc = markdown_init();
    // linklist of log
    log_head = malloc(sizeof(versionlog));
    log_head->version = 1;
    char* header = "VERSION 1\n";
    log_head->len =strlen(header);
    log_head->editlog = malloc(strlen(header)+1);
    log_head->editlog[log_head->len] = '\0';
    log_tail = log_head;

    //initialise the quit_edit flag;
    quit_edit = 0;

    pthread_t handle_event;
    pthread_create(&handle_event, NULL, queue_handle_thread, NULL);
    // broadcast message to all clients every time_interval seconds
    pthread_t broadcast;
    pthread_create(&broadcast,NULL, broadcast_to_all_clients_thread, &time_interval);

    // server debuging thread
    pthread_t server_command;
    pthread_create(&server_command, NULL,servercom, NULL);
    //while(1){
        siginfo_t info;
        int sig = sigwaitinfo(&set, &info);
        int clientpid = info.si_pid;
        pthread_t communication;
        if (sig == -1 && errno != EAGAIN) {
            perror("sigtimedwait");
            return 1;
        }else if (sig == SIGRTMIN) {
            // initial thread 
            pthread_create(&communication, NULL, communication_thread, &clientpid);      
            //pthread_detach(thread);            
        }
    //}
    //pthread_join(communication, NULL);  
    pthread_join(handle_event, NULL);
    pthread_join(broadcast,NULL);
    pthread_join(server_command, NULL);
    printf("finish the editing\n");
    // close epoll instance
    return 0;
    
}