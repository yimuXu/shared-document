//TODO: client code that can send instructions to server.
#include "markdown.h"

document* local_doc;
char* editlog;
char* current_verison_log;
char* offset_a;

all_log* local_log;
pthread_mutex_t doc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


// to analyse the log from the server, then send to edit the local document.
void get_command(char* log){
    char* log_copy = malloc(strlen(log)+1);
    
    strcpy(log_copy,log);
    log_copy[strlen(log)] ='\0'; 
    char* end_result = NULL;
    char* serach = log_copy;
    while((serach = strstr(serach, "SUCCESS")) != NULL){
        end_result = serach;
        serach += 7; // jeep over this word
    }
    if(!end_result){
        free(log_copy);
        return;
    }

    end_result -= 1;
    *end_result = '\0';
    char* tk = strtok(log_copy, " ");
    char* username = strtok(NULL," ");
    (void)username, (void)tk;
    char* content = strtok(NULL, "");
    edit_doc(local_doc,content);
    free(log_copy);
    return;
}
void edit_local_doc(char* cur_log){
    char* cur_copy = malloc(strlen(cur_log)+1);
    
    strcpy(cur_copy,cur_log);
    cur_copy[strlen(cur_log)] = '\0';
    char* temp = cur_copy;
    int count = 0;
    char* tokens[256];
    tokens[0] = strtok(temp, "\n");
    while (tokens[count] != NULL && count < 256){
        count++;
        tokens[count] = strtok(NULL,"\n");
    }
    for(int j = 0; j< count; j++){
        get_command(tokens[j]);
    }
    free(cur_copy);
}
void insert_original_doc_data(char* line){

    char* version = strtok(line,"\n");
    uint64_t ver = atoi(version);
    char* size = strtok(NULL,"\n");
    (void)size;
    char* data = strtok(NULL, "\n");
    local_doc->version = ver;
    markdown_insert(local_doc,ver,0,data);
    local_doc->head->chunkversion = local_doc->version;
}

char* log_flatten(){
    versionlog* cur = local_log->head;
    char* buf;
    size_t offset = 0;
    if(local_log->head == NULL || local_log == NULL || local_log->head->editlog == NULL){
        buf = malloc(1);
        buf[0]= '\0';
        return buf;
    }
    buf = malloc(local_log->size + 1);
    while (cur){
        for(size_t i = 0; i < cur->len;i++){
            buf[offset + i]=cur->editlog[i];
        }
        offset += cur->len;
        cur = cur->next;
    }
    buf[local_log->size] = '\0';
    return buf;
}
void append_log_to_all(char* log_line){
    versionlog* log = malloc(sizeof(versionlog));
    log->len = strlen(log_line);
    log->editlog = malloc(log->len+1);
    strncpy(log->editlog,log_line,log->len);
    log->editlog[log->len] ='\0';
    log->next = NULL;
    log->version = local_doc->version+1;
    if(local_log->head == NULL){
        local_log->head = log;
        local_log->tail = log;
    }else{
        local_log->tail->next = log;
        local_log->tail = log;
        
    }
    local_log->size += log->len;
}

void* receive_broadcast (void* arg){
    int* s2cfd = (int*)arg;
    //printf("client is receiving broadcast from server!\n");
    while(1){
        char temp[512];
        int size = read(*s2cfd, temp, 512);
        temp[size] ='\0';
        if (size > 0) {
            // current_verison_log = malloc(size+1);
            // strncpy(current_verison_log, temp, size);
            // current_verison_log[size] = '\0';
            //printf("receive:\n%ssize:%d\njiewei\n",temp,size);
            // lock  doc
            pthread_mutex_lock(&doc_mutex);
            //edit_local_doc(current_verison_log);
            edit_local_doc(temp);///////////////
            markdown_increment_version(local_doc);
            pthread_mutex_unlock(&doc_mutex);
            //lock log
            pthread_mutex_lock(&log_mutex);
           // append_log_to_all(current_verison_log);//////////
           append_log_to_all(temp);
            pthread_mutex_unlock(&log_mutex);
            free(current_verison_log);
            //printf("update client doc\n");
            //memset(temp, 0, sizeof(temp));
        }
    }
    return NULL;
}

int main (int argc, char** argv){
    // test_append_and_flatten_log();
    // test_insert_original_doc_data();    
    (void)argc, (void)argv;
    if (argc != 3){
        printf("please input 2 element!\n");
        return 1;
    }
    int serverpid = atoi(argv[1]);
    // initialize signal set
    /////////////////////int sig = -1;
    sigset_t set;
    struct timespec timeout = {1, 0};
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN+1);
    sigprocmask(SIG_BLOCK, &set, NULL);    
    // argv[1] is server pid, argv[2] is username
    kill(serverpid, SIGRTMIN);

    //wait 1 second for signal
    int x = sigtimedwait(&set, NULL, &timeout);
    if( x == -1){
        printf("sigtimedwait error!\n");
        return 1;
    }
    //open FIFO
    int clientpid = getpid();
    char name1[] = "FIFO_C2S_";
    char name2[] = "FIFO_S2C_";
    char c2s[100];
    char s2c[100];
    sprintf(c2s, "%s%d",name1,clientpid);
    sprintf(s2c, "%s%d",name2,clientpid);
    

    // pipe's file descriptor
    int c2sfd = open(c2s, O_WRONLY);
    int s2cfd = open(s2c, O_RDONLY);
    if(c2sfd == -1){
        printf("open FIFO_C2S error!\n");
        exit(1);
    }
    if(s2cfd == -1){
        printf("open FIFO_S2C error!\n");
        exit(1);
    } 
    //printf("client %d: open fifo\n", clientpid);
    //send username to server
    char username[256];
    sprintf(username, "%s\n", argv[2]);
    write(c2sfd, username, 256);
    char authorisation[256];
    read(s2cfd, authorisation, 256);
    //printf("receive: %s",authorisation);
    if(strncmp(authorisation, "read",4) == 0){
        
    }else if(strncmp(authorisation, "write",5) == 0){
        //printf("%s",authorisation);
    }else{
        //printf("%s\n",authorisation);
        printf("error of authorisation!\n");//debug
        return 1;
    }

    char bufdoc[512];
    read(s2cfd,bufdoc,512);
    local_doc = markdown_init();
    insert_original_doc_data(bufdoc);    
    
    //// initial the received doc
    offset_a = 0;
    local_log = log_init(); // linked list of log, a node store text of a version
    pthread_t broadcast_thread;
    pthread_create(&broadcast_thread, NULL, receive_broadcast, &s2cfd);

    char buf[256]; 
    while(1){
           
        if(fgets(buf, 256, stdin)){
            if(strncmp(buf,"DOC?\n",5) == 0){
                //lock
                pthread_mutex_lock(&doc_mutex);
                char* buf = markdown_flatten(local_doc);
                pthread_mutex_unlock(&doc_mutex);
                printf("%s",buf);
                free(buf);
            }else if(strncmp(buf, "PERM?\n",6)==0){
                printf("%s",authorisation);
            }else if(strncmp(buf, "LOG?\n",5) == 0) {
                pthread_mutex_lock(&log_mutex);
                editlog = log_flatten();
                pthread_mutex_unlock(&log_mutex);
                printf("%s\n", editlog);
                free(editlog);
            }else if(strncmp(buf, "DISCONNECT\n",11) == 0) {
                write(c2sfd,buf,256);
                close(c2sfd);
                close(s2cfd);
                unlink(c2s);
                unlink(s2c);
                break;
            }else{
                ssize_t size_written = write(c2sfd,buf,256);
                if(size_written > 0){
                    //printf("cilent side, send command successful\n");  
                }             
            }        
        }

    }
    pthread_mutex_lock(&doc_mutex);
    markdown_free(local_doc);
    pthread_mutex_unlock(&doc_mutex);

    pthread_mutex_lock(&log_mutex);
    log_free(local_log);
    pthread_mutex_unlock(&log_mutex);
    pthread_cancel(broadcast_thread);//////////
    pthread_join(broadcast_thread, NULL);

    return 0;
}