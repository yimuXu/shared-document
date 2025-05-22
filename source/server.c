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

#define VERSION_ALL ((uint64_t)-1)
#define MAX_CLIENT 10
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t doc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

#define QUEUE_SIZE 100

typedef struct {
    char* data;
    int authorisation; //0 is write, 1 is read
    char username[64];
    int reject;
    struct timespec timestamp;// if reject, this store the reject type;
}msginfo;

typedef struct {
    msginfo** msgs;
    int size;
}minheap;
minheap* hp;

versionlog* log_head;
all_log* a_log;
all_log* buflog;  // used in broadcast, the log about to send to the pipe

char* whole_log;


struct clientpipe{
    int c2sfd;
    int s2cfd;
    char c2sname[64];
    char s2cname[64];
    char username[64];
    int clientpid;
    msginfo* mq[50]; //queue of this client
    int command_count;
    pthread_mutex_t mutex;

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

int checkauthorisation(char* username, int c2sfd, int s2cfd, int* rw_flag){
    // check if username is valid
    (void) s2cfd;(void) c2sfd;
    FILE* fp = fopen("roles.txt", "r");////////////////////////////////////
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

// add client
struct clientpipe* addclient(int c2sfd, int s2cfd, char* username, int clientpid, char* c2s, char* s2c){
    //pthread_mutex_lock(&mutex);
    struct clientpipe* new_client = &clients[clientcount];
    clients[clientcount].c2sfd = c2sfd;
    clients[clientcount].s2cfd = s2cfd;
    strcpy(clients[clientcount].username, username);
    clients[clientcount].clientpid = clientpid;
    strcpy(clients[clientcount].c2sname, c2s);
    strcpy(clients[clientcount].s2cname, s2c);
    //clients[clientcount].mq = {.front = 0, .rear = 0, .count = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
    clients[clientcount].command_count = 0;
    clientcount++;
    pthread_mutex_init(&(new_client->mutex),NULL);
    //printf("client %d added in clients!\n", clientpid);
    //pthread_mutex_unlock(&mutex);
    return new_client;
}
// delete client from list
void deleteclient(char* username){
    //pthread_mutex_lock(&mutex);
    for(int i = 0; i < clientcount; i++){
        if(strcmp(clients[i].username,username) == 0){
            close(clients[i].c2sfd);
            close(clients[i].s2cfd);
            unlink(clients[i].c2sname);
            unlink(clients[i].s2cname);
            // for(int j = 0; j < 50; j++){
            //     if(clients[i].mq[j] != NULL){
            //         free(clients[i].mq[j]->data);
            //         free(clients[i].mq[j]);                    
            //     }

            // }
            pthread_mutex_destroy(&(clients[i].mutex));
            for(int j = i; j < clientcount-1; j++){
                clients[j] = clients[j+1];
            }
            clientcount--;
            printf("client %s deleted from clients!\n", username);
            break;
        }
    }
    //pthread_mutex_unlock(&mutex);
}

versionlog* append_to_editlog(all_log* lg, char** log_line){

    versionlog* newlog = malloc(sizeof(versionlog));
    newlog->len = strlen(*log_line);
    newlog->editlog = malloc(newlog->len+1);
    strncpy(newlog->editlog, *log_line,newlog->len);
    newlog->editlog[newlog->len] = '\0';
    newlog->version = doc->version;
    newlog->next = NULL;
    if(lg == NULL || lg->head == NULL || lg->head->editlog == NULL){
        lg->head = newlog;
        lg->tail =newlog;
        lg->last_start = newlog;
    }else{
        versionlog* cu = lg->head;
        while(cu->next){
            cu = cu->next;
        }
        cu->next = newlog;
        lg->tail->next = newlog;
        lg->tail = newlog;        
    }    
    
    lg->size += newlog->len;
    if(strncmp(*log_line,"VERSION",7) == 0){
        lg->last_start = newlog;
    }
    if(strncmp(*log_line, "END\n",4)== 0){
        lg->last_end = newlog;
    }
    return newlog;
}

char* test_flatten_all(all_log* lg){
    char* da;
    size_t of =0;
    versionlog* cu;
    size_t size = 0;
    versionlog* temp = lg->head;
    while(temp){
        size += strlen(temp->editlog);           
        temp = temp->next;
    }
    da = malloc(size+1);
    if(!da){
        printf("malloc failed\n");
        return NULL;
    }
    cu = lg->head;
    //printf("KAITOU\n");
    while(cu){
        if(cu->editlog != NULL){
            for(size_t i = 0; i < strlen(cu->editlog); i++) {
                da[of + i] = cu->editlog[i];
            }
            //printf("%s",cu->editlog);
            of += cu->len;
        }
        cu = cu->next;
    }
    //printf("jiewei\n");
    da[size] ='\0';
    return da;
}

// flatten the log linked list to text
char* editlog_flatten(all_log* log, uint64_t version){
    char* logdata;
    size_t offset = 0;
    versionlog* cur;
    if(log->size == 0 || log->head == NULL || log->head->editlog == NULL){
        logdata = malloc(1);
        logdata[0] = '\0';
        printf("log is null\n");
        return logdata;
    }
    if (version == VERSION_ALL){
        size_t size = 0;
        versionlog* temp = log->head;
        while(temp){
            size += temp->len;
            if(temp == log->last_end){
                //printf("find a last end size: %ld\n",size);
                break;
            }            
            temp = temp->next;
        }
        logdata = malloc(size+1);
        if(!logdata){
            printf("malloc failed\n");
            return NULL;
        }
        cur = log->head;
        while(cur){
            //printf("offset : %ld\n",offset);
            //printf("line: %s",cur->editlog);

            if(cur->editlog != NULL){
                for(size_t i = 0; i < cur->len; i++) {
                    logdata[offset + i] = cur->editlog[i];
                }

                offset += cur->len;
                //printf("current cursor: %ld\n",offset);
            }
            if(cur == log->last_end){
                break;
            }
            
            cur = cur->next;
        }
        logdata[size] ='\0';
    }else{
        //printf("current version log\n");
        cur = log->last_start;
        //calculate the version chunk size
        size_t size = 0;
        versionlog* temp = cur;
        while(temp){

            if(temp->version == version){
                size += strlen(temp->editlog);
            }
            if(temp == log->last_end){
                break;
            }            
            temp = temp->next;
        }
        logdata = malloc(size+1);
        while(cur){
            //if(cur->version == version){
                for(size_t i = 0; i < strlen(temp->editlog); i++) {
                    logdata[offset + i] = cur->editlog[i];
                }
                offset += cur->len;                
            //}
            if(cur == log->last_end){
                break;
            }
            cur = cur->next;
        }
        logdata[size] = '\0';
    }
    return logdata;
}
// append the command infomation into log;
void input_log(int edit_result,char* username_copy, char* data_copy, char** log_line){
    if(edit_result == 0){
        num_com_success ++;
        size_t size = snprintf(NULL,0,"EDIT %s %s SUCCESS\n",username_copy, data_copy) +1;
        *log_line = realloc(*log_line,size); 
        snprintf(*log_line,size,"EDIT %s %s SUCCESS\n",username_copy, data_copy);
    }else if(edit_result == -1){
        size_t size = snprintf(NULL, 0, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "INVALID_POSITION") +1;
        *log_line = realloc(*log_line,size);
        snprintf(*log_line, size, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "INVALID_POSITION");
    }else if(edit_result == -2){
        size_t size = snprintf(NULL, 0, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "DELETED_POSITION") +1;
        *log_line = realloc(*log_line,size);
        snprintf(*log_line, size, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "DELETED_POSITION");
    }else if(edit_result == -3){

        size_t size = snprintf(NULL, 0, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "OUTDATED_VERSION") +1;
        *log_line = realloc(*log_line,size);
        snprintf(*log_line, size, "EDIT %s %s %s %s\n", username_copy, data_copy, "Reject", "OUTDATED_VERSION");
    }
}

//function to handle editing commands from client
int handle_edit_command(msginfo* msg) {
    //copy and free
    char* username = msg->username;
    char* data = msg->data;
    //printf("data:%s\n",data);
    char* log_line = NULL;
    if(msg->authorisation != 0) {
        size_t size = snprintf(NULL,0, "EDIT %s %s %s %s %s %s %s\n",username,data,"Reject","UNAUTHORISED",data,"write","read");
        log_line = realloc(log_line,size);
        snprintf(log_line,size, "EDIT %s %s %s %s %s %s %s\n",username,data,"Reject","UNAUTHORISED",data,"write","read");
        pthread_mutex_lock(&log_mutex);
        append_to_editlog(a_log,&log_line);
        append_to_editlog(buflog,&log_line);//////////
        pthread_mutex_unlock(&log_mutex);
        free(log_line);
        return 0;
    }
    if(strncmp(data, "DISCONNECT",10) == 0){
        // disconnect client
        printf("user %s disconnected\n",username);
        pthread_mutex_lock(&clients_mutex);
        deleteclient(username);
        pthread_mutex_unlock(&clients_mutex);
        free(log_line);
        return 0;
    }
    if(strlen(data)>256){
        size_t size = snprintf(NULL,0, "EDIT %s %s %s %s\n",username,data,"Reject","INTERNAL ERROR");
        log_line = realloc(log_line,size);
        snprintf(log_line,size, "EDIT %s %s %s %s\n",username,data,"Reject","INTERNAL ERROR");       
        free(log_line);
        return 1;
    }
    int result = edit_doc(doc,data);
    input_log(result,username,data,&log_line);
    pthread_mutex_lock(&log_mutex);
    //printf("current log_line: %s", log_line);
    versionlog* bu = append_to_editlog(a_log,&log_line);
    append_to_editlog(buflog,&log_line);////////////////////////
    bu->version += 1;
    pthread_mutex_unlock(&log_mutex);
    free(log_line);
    //printf("handle edit command log in the new log: %s", bu->editlog);
    return 0;

}

int time_spec_cmp(struct timespec a,struct timespec b){
    if (a.tv_sec < b.tv_sec) return -1;
    if (a.tv_sec > b.tv_sec) return 1;
    if (a.tv_nsec < b.tv_nsec) return -1;
    if (a.tv_nsec > b.tv_nsec) return 1;
    return 0;
}

msginfo* heap_pop(minheap *heap) {
    if (heap->size == 0) {
        printf("Heap is empty!\n");
        return NULL;
    }
    msginfo* min = heap->msgs[0];
    heap->msgs[0] = heap->msgs[--heap->size];
    heap->msgs[heap->size] = NULL;
    int i = 0;
    //sift down
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < heap->size && time_spec_cmp( heap->msgs[left]->timestamp, heap->msgs[smallest]->timestamp) < 0){
            smallest = left;
        }
        if (right < heap->size && time_spec_cmp(heap->msgs[right]->timestamp, heap->msgs[smallest]->timestamp) < 0){
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        msginfo* temp = heap->msgs[i];
        heap->msgs[i] = heap->msgs[smallest];
        heap->msgs[smallest] = temp;

        i = smallest;
    }
    return min;
}

void heap_push(minheap* heap, int size, msginfo* msg) {
    if (heap->size >= size) {
        printf("Heap is full!\n");
        return;
    }
    int i = heap->size++;
    //copy msg to heap
    heap->msgs[i] = malloc(sizeof(msginfo));
    memcpy(heap->msgs[i],msg,sizeof(msginfo));
    size_t len = strlen(msg->data)+1;
    heap->msgs[i]->data = malloc(len);
    strcpy (heap->msgs[i]->data,msg->data);
    heap->msgs[i]->data[len -1] = '\0';
    
    //heap->msgs[i] = msg;
    //printf("index of this msg in heap is : %d\n",i);

    //sift up
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (time_spec_cmp(heap->msgs[parent]->timestamp, heap->msgs[i]->timestamp)<0){
             break;
        }
        // swap
        msginfo* temp = heap->msgs[i];
        heap->msgs[i] = heap->msgs[parent];
        heap->msgs[parent] = temp;
        i = parent;
    }
    free(msg->data);
    free(msg);

}
void collect_command(){
    int num_commands = 0;
    for(int i = 0; i < clientcount; i++){
        pthread_mutex_lock(&(clients[i].mutex));
        num_commands += clients[i].command_count;
        pthread_mutex_unlock(&(clients[i].mutex));
    }
    if(num_commands == 0){
        return;
    }
    //printf("in the collect\n");
    hp->msgs = malloc(sizeof(msginfo*)*num_commands);
    hp->size = 0;

    for(int j = 0; j< clientcount; j++){
        pthread_mutex_lock(&(clients[j].mutex));
        for(int k = 0; k < clients[j].command_count; k++){
            heap_push(hp,num_commands, clients[j].mq[k]);
            clients[j].mq[k] = NULL;
        }
        clients[j].command_count = 0;
        pthread_mutex_unlock(&(clients[j].mutex));
    } 
    //printf("msg heap com num: %d\n",num_commands);
    for(int h = 0; h < num_commands; h++){
        msginfo* msg = heap_pop(hp);
        //printf("msg data: %s",msg->data);
        //printf("i m in the pop loop\n");
        handle_edit_command(msg);
        free(msg->data);
        free(msg);   
    }
    num_commands = 0;
    free(hp->msgs);
    
    
}

// thread to broadcast message to all clients  and update the verison and do
void* broadcast_to_all_clients_thread(void* arg) {
    int interval = *(int*)arg;
    //printf("broadcast is running!\n");
    while(1){
        if(quit_edit == 1){
            break;
        }
        buflog = log_init();
        pthread_mutex_lock(&doc_mutex);
        char* bufversion = "VERSION";
        size_t len = snprintf(NULL,0,"%s %ld\n",bufversion,doc->version) + 1;
        //printf("doc version : %ld len: %ld\n",doc->version,len);
        char* versionline = malloc(len+3);
        snprintf(versionline,len,"%s %ld\n",bufversion,doc->version);
        versionline[len-1] = '\0';
        pthread_mutex_unlock(&doc_mutex);
        //printf("versionline: %s\n",versionline);
        //lock
        pthread_mutex_lock(&log_mutex);
        versionlog* current_version_log = append_to_editlog(a_log,&versionline);
        versionlog* ver = append_to_editlog(buflog,&versionline);
        //a_log->last_start = current_version_log;
        pthread_mutex_unlock(&log_mutex);
            
        collect_command(); 
        
        if(num_com_success != 0){
            //lock
            //printf("command success more tha 0\n");
            pthread_mutex_lock(&doc_mutex);
            markdown_increment_version(doc);
            //printf("doc version:%ld\n",doc->version);
            num_com_success = 0;
            free(current_version_log->editlog);  
            int leng = snprintf(NULL,0,"%s %ld\n",bufversion,doc->version);  
            char* modify = malloc(leng+3);
            snprintf(modify,leng+1,"%s %ld\n",bufversion,doc->version);              
            current_version_log->editlog = modify;
            current_version_log->version = doc->version;
            // for buffer log send to pipe
            free(ver->editlog);
            char* modify2 = malloc(leng+3);
            snprintf(modify2,leng+1,"%s %ld\n",bufversion,doc->version);
            ver->editlog = modify2;
            ver->version = doc->version;

            pthread_mutex_lock(&log_mutex);
            //printf("current version line is %s\n",current_version_log->editlog);
            pthread_mutex_unlock(&log_mutex);
            pthread_mutex_unlock(&doc_mutex);
        }
        //edit the log 
        char* end = "END\n";
        pthread_mutex_lock(&log_mutex);
        versionlog* end_log = append_to_editlog(a_log,&end);
        a_log->last_end = end_log;
        append_to_editlog(buflog,&end);
        pthread_mutex_unlock(&log_mutex);
        
        // broadcast to all clients
        pthread_mutex_lock(&log_mutex);
        char* vlog = test_flatten_all(buflog);
        //printf("vlog:\n%s\n",vlog);
       log_free(buflog);
        //printf("all the log:\n%s",x);
        pthread_mutex_unlock(&log_mutex);        
        for(int i = 0; i< clientcount;i++){   
            if(clients->s2cfd){
                int fd = clients[i].s2cfd;
                write(fd,vlog,strlen(vlog));
                           
            }            
        }
        if(whole_log != NULL){
           free(whole_log); 
        }
        free(vlog);
        free(versionline);          
        whole_log =test_flatten_all(a_log);
        usleep(interval * 1000); 
        //pthread_mutex_unlock(&mutex);
      
    }
    //printf("quit the broadcast\n");
    return NULL;
}

//server client thread
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
    // add client to list//
    pthread_mutex_lock(&clients_mutex);
    struct clientpipe* current_client =  addclient(c2sfd, s2cfd, username, *clientpid, c2s, s2c);
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_lock(&doc_mutex);
    dcdata = markdown_flatten(doc);
    pthread_mutex_unlock(&doc_mutex);
    size_t needed = snprintf(NULL, 0, "%ld\n%ld\n%s\n", doc->version, (uint64_t)strlen(dcdata), dcdata) + 1;
    bufferdoc = malloc(needed);
    if (!bufferdoc) {
        perror("malloc bufferdoc");
        // handle error
    }
    snprintf(bufferdoc, needed, "%ld\n%ld\n%s\n",doc->version, (uint64_t)strlen(dcdata), dcdata);
    int x = write(s2cfd, bufferdoc, strlen(bufferdoc));
    if (x == -1) {
        printf("write error:\n");
    }
    free(dcdata);
    free(bufferdoc);

    while(1){
        char buf[512];///// NEED TO CHANGE to check the len
        int x =read(c2sfd,buf,512);
        if(x <= 0){
            break;
        }   
       if(strncmp(buf,"DISCONNECT",10)==0){
            printf("user %s disconnected\n",username);
            pthread_mutex_lock(&clients_mutex);
            deleteclient(username);
            pthread_mutex_unlock(&clients_mutex);            
            break;
        }   
        //printf("receive msg\n");
        buf[strcspn(buf, "\n")] = 0;
        msginfo* new_msg = malloc(sizeof(msginfo));
        size_t len =strlen(buf);
        new_msg->data = malloc(len+1);
        strncpy(new_msg->data,buf,len);
        new_msg->data[len] = '\0';
        clock_gettime(CLOCK_REALTIME, &new_msg->timestamp);
        //printf("Time: %ld.%09ld\n", new_msg->timestamp.tv_sec, new_msg->timestamp.tv_nsec);
        username[strcspn(username, "\n")] = 0;
        strcpy(new_msg->username, username);
        new_msg->authorisation = rw_flag;
        new_msg->reject = -1;
        pthread_mutex_lock(&(current_client->mutex));
        current_client->mq[current_client->command_count] = new_msg;
        //printf("receive a msg from pipe\n");
        current_client->command_count++;
        pthread_mutex_unlock(&(current_client->mutex));
   
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
    //test_msginfo_log_flow();
    if(argc != 2){
        printf("input time interval!\n");
        return 1;
    }
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    pthread_sigmask(SIG_BLOCK, &set, NULL);  
    doc = markdown_init();
    // linklist of log
    a_log = log_init();
    
    int time_interval = atoi(argv[1]);
    //(void)time_interval;
    whole_log = NULL;


    //pthread_t handle_event;
    // pthread_create(&handle_event, NULL, queue_handle_thread, NULL);

    // create struct to store client information
    clients = malloc(sizeof(struct clientpipe) * MAX_CLIENT);
    clientcount = 0; // count of clients
    hp = malloc(sizeof(minheap));
    // broadcast message to all clients every time_interval seconds
    pthread_t broadcast;
    pthread_create(&broadcast,NULL, broadcast_to_all_clients_thread, &time_interval);

    // create thread to register client 
    pthread_t register_clients;
    pthread_create(&register_clients, NULL, register_client, &set);

    int serverpid = getpid();
    printf("Server PID: %d\n", serverpid);
    
    quit_edit = 0;
    char quit[256];
    
    while(1){
        //printf("server side debug is running!\n");
        if(fgets(quit, 256, stdin)){
            if(strcmp(quit, "QUIT\n") == 0){
                
                if(clientcount == 0){
                    //printf("receive quit\n");
                    pthread_cancel(register_clients);
                    pthread_cancel(broadcast);  
                    pthread_join(broadcast,NULL);                                      
                    // FILE* fp = fopen("doc.md","w");
                    // if(fp == NULL){
                    //     perror("file open failed");
                    // }
                    // markdown_print(doc,fp);
                    // fclose(fp);
                    free(clients);
                    //pthread_mutex_lock(&mutex);
                    quit_edit = 1;
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
                // pthread_mutex_lock(&log_mutex);
                // char* alog = editlog_flatten(a_log,VERSION_ALL);
                // pthread_mutex_unlock(&log_mutex);
                printf("%s", whole_log);
                //free(alog);
            }            
        }

    } 
    markdown_free(doc);
    log_free(a_log);
    free(hp);
    //pthread_join(communication, NULL);  
    //pthread_join(handle_event, NULL);

    printf("finish the editing\n");
    return 0;
    
}