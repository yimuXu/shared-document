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
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t doc_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    uint64_t version;
    size_t len;
    char* editlog; // current version command log
    struct versionlog* next;
}versionlog;

typedef struct all_log {
    versionlog* head;
    versionlog* tail;

    size_t size;
}all_log;

versionlog* log_head;

all_log* a_log;


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

int quit_edit; //flag for quit all threads
int num_com_success;


void versionline_free(){
    versionlog* cur = a_log->head;
    versionlog* temp = a_log->head;
    while(cur){
        temp = cur->next;
        free(cur->editlog);
        free(cur);
        cur = temp;
    }
    free(a_log);
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
    //printf("FIFO created: %s, %s!\n",c2sname,s2cname);
    

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
    strncpy(newlog->editlog, log_line,newlog->len);
    newlog->editlog[newlog->len] = '\0';
    newlog->version = doc->version;
    newlog->next = NULL;
    if(a_log->head == NULL){
        a_log->head = newlog;
        a_log->tail =newlog;
    }else{
        a_log->tail->next = newlog;
        a_log->tail = newlog;        
    }    
    a_log->size += newlog->len;
}

// flatten the log linked list to text
char* editlog_flatten(all_log* log, uint64_t version){
    char* logdata;
    size_t offset = 0;
    versionlog* cur;
    if(log->size == 0 || log->head == NULL || log->head->editlog == NULL){
        logdata = malloc(1);
        logdata[0] = '\0';
        return logdata;
    }
    uint64_t v = -1;
    if (version == v){
        printf("all the log\n");
        size_t size = log->size;
        printf("size: %ld\n",size);
        logdata = malloc(size+1);
        if(!!logdata){
            printf("malloc failed\n");
        }
        cur = log->head;
        while(cur){
            for(size_t i = 0; i < cur->len; i++) {
                logdata[offset + i] = cur->editlog[i];
            }
            offset += cur->len;
            cur = cur->next;
        }
        logdata[size] = '\0';
    }else{
        printf("current version log\n");
        cur = log->head;
        //fine current verison header
        while(cur){
            if(cur->version == version){
                break;
            }
            cur = cur->next;
        }
        //calculate the version chunk size
        size_t size = 0;
        versionlog* temp = cur;
        while(temp){
            if(temp->version == version){
                size += temp->len;
            }
            temp = temp->next;
        }
        logdata = malloc(size+1);
        while(cur){
            if(cur->version == version){
                for(size_t i = 0; i < cur->len; i++) {
                    logdata[offset + i] = cur->editlog[i];
                }
                offset += cur->len;                
            }
            cur = cur->next;
        }
        logdata[size] = '\0';
    }
    return logdata;
}

void edit_doc(char* data_copy, char* username_copy, char* log_line){
    // char cmd[32];
    // int pos,len,start_pos,end_pos;
    // char content[200];
    int edit_result;
    //int matched = 0;
    char* commandtype = strtok(data_copy, " ");
    //matched = sscanf(data_copy, "%31s %d %[^\n]", cmd, &pos, content); // handle the wrong format, do it later
    if(strcmp(commandtype, "INSERT") == 0){
        // insert command
        int pos = atoi(strtok(NULL, " "));
        char* content = strtok(NULL, "");
        content[strcspn(content, "\n")] = 0;
        // call the insert function //////////////////////
        printf("insert %s at %d\n", content, pos);
        // call insert function
        edit_result = markdown_insert(doc,doc->version,pos, content);

    }else if(strcmp(commandtype, "DEL") == 0){
        // delete command
        int pos = atoi(strtok(NULL, " "));
        int len = atoi(strtok(NULL, " "));
        edit_result = markdown_delete(doc,doc->version,pos, len);
        //error handle
    }else if(strcmp(commandtype, "NEWLINE") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_newline(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "HEADING") == 0){
        int level = atoi(strtok(NULL, " "));
        int pos = atoi(strtok(NULL, " "));
        edit_result= markdown_heading(doc, doc->version,level, pos);
        //error handle
    }else if(strcmp(commandtype, "BOLD") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_bold(doc, doc->version, start_pos,end_pos);
        //error handle
    }else if(strcmp(commandtype, "ITALIC") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_italic(doc, doc->version, start_pos, end_pos);
        //error handle
    }else if(strcmp(commandtype, "BLOCKQUOTE") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_blockquote(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "ORDERED_LIST") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_ordered_list(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "UNORDERED_LIST") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_unordered_list(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "CODE") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_code(doc, doc->version, start_pos,end_pos);
        //error handle
    }else if(strcmp(commandtype, "LINK") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        char* link = strtok(NULL, " ");
        edit_result = markdown_link(doc, doc->version, start_pos,end_pos,link);
        //error handle
    }else if(strncmp(data_copy, "DISCONNECT\n",11) == 0){
        // disconnect client
        printf("delete user\n");
        deleteclient(username_copy);
    }else{
        snprintf(log_line, 256, "EDIT %s %s %s %s", username_copy, data_copy, "Reject", "INVALID_POSITION");
    }
    if(edit_result == 0){
        num_com_success ++;
        snprintf(log_line,256,"EDIT %s %s SUCCESS",username_copy, data_copy);
    }else if(edit_result == -1){
        snprintf(log_line, 256, "EDIT %s %s %s %s", username_copy, data_copy, "Reject", "INVALID_POSITION");
    }else if(edit_result == -2){
        snprintf(log_line, 256, "EDIT %s %s %s %s", username_copy, data_copy, "Reject", "DELETED_POSITION");
    }else if(edit_result == -3){
        snprintf(log_line, 256, "EDIT %s %s %s %s", username_copy, data_copy, "Reject", "OUTDATED_VERSION");
    }

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
    edit_doc(data_copy,username_copy,log_line);
    append_to_editlog(log_line);
    free(username_copy);
    free(data_copy);
    return 0;

}

// thread handle priority queue event
void* queue_handle_thread(void* arg) {
    (void)arg;
    //printf("start to handle edit command!\n");
    while(1) {
        if(quit_edit == 1){
            break;
        }
        handle_edit_command();
    }
    //printf("quit the queue handle thread\n");
    return NULL;
}
// thread to broadcast message to all clients  and update the verison and do
void* broadcast_to_all_clients_thread(void* arg) {
    int interval = *(int*)arg;
    //printf("broadcast is running!\n");
    while(1){
        if(quit_edit == 1){
            break;
        }
        usleep(interval * 1000);
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
        size_t len = strlen(bufversion) + 1 + 3;
        char* versionline = malloc(len);
        //lock
        pthread_mutex_lock(&log_mutex);
        append_to_editlog(end);
        pthread_mutex_unlock(&log_mutex);
        
        if(num_com_success != 0){
            //lock
            pthread_mutex_lock(&doc_mutex);
            markdown_increment_version(doc);
            num_com_success = 0;
            pthread_mutex_unlock(&doc_mutex);
        }
        int verisonnum = doc->version;
        snprintf(versionline,len,"%s %c\n",bufversion,verisonnum + '0');
        //lock
        pthread_mutex_lock(&log_mutex);
        append_to_editlog(versionline);
        pthread_mutex_unlock(&log_mutex);
    
        free(versionline);
        for(int i = 0; i< clientcount;i++){       //do not delete!!!!!!!!!!!!!!!
            if(clients->s2cfd){
                int fd = clients[i].s2cfd;
                //lock
                pthread_mutex_lock(&log_mutex);
                char* vlog = editlog_flatten(a_log,doc->version);
                pthread_mutex_unlock(&log_mutex);
                write(fd,vlog,strlen(vlog));
                free(vlog);            
            }            
        }
        pthread_mutex_unlock(&mutex);
              
    }
    //printf("quit the broadcast\n");
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
    char* perm;
    if(rw_flag == 0){
        perm = "write";
    }else if(rw_flag == 1){
        perm = "read";
    }

    // add client to list// add event to epoll
    addclient(c2sfd, s2cfd, username, *clientpid, c2s, s2c);
    
    pthread_mutex_lock(&doc_mutex);
    dcdata = markdown_flatten(doc);
    pthread_mutex_unlock(&doc_mutex);
    size_t needed = snprintf(NULL, 0, "%s\n%ld\n%ld\n%s",perm, doc->version, (uint64_t)strlen(dcdata), dcdata) + 1;
    bufferdoc = malloc(needed);
    if (!bufferdoc) {
        perror("malloc bufferdoc");
        // handle error
    }
    snprintf(bufferdoc, needed, "%s\n%ld\n%ld\n%s",perm,doc->version, (uint64_t)strlen(dcdata), dcdata);
    int x = write(s2cfd, bufferdoc, strlen(bufferdoc));
    if (x == -1) {
        printf("write error:\n");
    }
    free(dcdata);
    free(bufferdoc);

    while(1){
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
    }
    printf("quit communication thread\n");

    return NULL;

}

void* register_client(void* arg){
    (void)arg;
    int sig,clientpid;
    sigset_t set;
    siginfo_t info;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    pthread_sigmask(SIG_BLOCK, &set, NULL); 
    printf("register_client start work\n");   
    while(1){
        sig = sigwaitinfo(&set, &info);
        clientpid = info.si_pid;
        pthread_t communication;
        if (sig == -1 && errno != EAGAIN) {
            perror("sigtimedwait");
            //continue;
        }else if (sig == SIGRTMIN) {
            // initial thread 
            pthread_create(&communication, NULL, communication_thread, &clientpid);      
            pthread_detach(communication);            
        }
    }
    return NULL;
}

int main(int argc, char** argv){
    // check if user input time interval for update document
    if(argc != 2){
        printf("input time interval!\n");
        return 1;
    }
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    pthread_sigmask(SIG_BLOCK, &set, NULL);        
    // create thread to register client immediately
    pthread_t register_clients;
    pthread_create(&register_clients, NULL, register_client, &set);
    int serverpid = getpid();
    printf("Server PID: %d\n", serverpid);
    int time_interval = atoi(argv[1]);
    (void)time_interval;  /// will be delete
    // create struct to store client information
    clients = malloc(sizeof(struct clientpipe) * MAX_CLIENT);
    clientcount = 0; // count of clients
    
    doc = markdown_init();

    // linklist of log
    a_log = malloc(sizeof(all_log));
    a_log->size = 0;
    log_head = malloc(sizeof(versionlog));
    log_head->version = 0;
    char* header = "VERSION 0\n";
    log_head->len =strlen(header);
    log_head->editlog = malloc(strlen(header)+1);
    log_head->editlog[log_head->len] = '\0';
    a_log->head = log_head;
    a_log->tail = log_head;
    ///test
    append_to_editlog("User1: insert Hello");
    append_to_editlog("User2: insert World");
    append_to_editlog("User1: delete 3");

    // 获取所有日志内容
    char *flattened = editlog_flatten(a_log,-1);  // -1 表示获取全部
    if (flattened) {
        printf("---- FLATTENED LOG OUTPUT ----\n%s\n-------------------------------\n", flattened);
        free(flattened);
    } else {
        printf("Failed to flatten log.\n");
    }
    /// test

    //initialise the quit_edit flag;
    quit_edit = 0;

    pthread_t handle_event;
    pthread_create(&handle_event, NULL, queue_handle_thread, NULL);
    // broadcast message to all clients every time_interval seconds
    // pthread_t broadcast;
    // pthread_create(&broadcast,NULL, broadcast_to_all_clients_thread, &time_interval);

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
                    //pthread_cancel(register_clients);
                    //pthread_cancel(broadcast);
                    pthread_cancel(handle_event);
                    break;
                }else{
                    printf("QUIT rejected, %d clients still connected.\n", clientcount);
                }
            }else if(strcmp(quit, "DOC?\n") == 0){
                printf("print doc\n");
                //lock
                pthread_mutex_lock(&doc_mutex);
                dcdata = markdown_flatten(doc);
                pthread_mutex_unlock(&doc_mutex);
                printf("%s",dcdata);
                free(dcdata);
            }else if(strcmp(quit, "LOG?\n")== 0){
                printf("print log!\n");
                pthread_mutex_lock(&log_mutex);
                char* alog = editlog_flatten(a_log,-1);
                pthread_mutex_unlock(&log_mutex);
                printf("%s", alog);
                free(alog);
            }            
        }

    } 
    //pthread_join(communication, NULL);  
    pthread_join(handle_event, NULL);
    //pthread_join(broadcast,NULL);
    printf("finish the editing\n");
    // close epoll instance
    return 0;
    
}