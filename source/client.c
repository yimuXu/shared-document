//TODO: client code that can send instructions to server.
#include "markdown.h"

document* doc;
char* editlog;

void* receive_broadcast (void* arg){
    int* s2cfd = (int*)arg;
    printf("client is receiving broadcast from server!\n");
    while(1){
        // int size = read(*s2cfd,editlog,256);
        // if(size > 0){
        //     // update the local documant;
        //     editlog = realloc(editlog,size);
        //     printf("update client doc\n");
        // }
        // }else if(size == 0){
        //     printf("close pipe!");
        //     break;
        // }
        char temp[256];
        int size = read(*s2cfd, temp, 256);
        if (size > 0) {
            // editlog = realloc(editlog, strlen(editlog) + size + 1);
            // strncat(editlog, temp, size);
            // editlog[strlen(editlog)] = '\0';
            printf("update client doc\n");
        }
    }
    return NULL;
}

int main (int argc, char** argv){
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
    printf("client %d: open fifo\n", clientpid);
    //send username to server
    char username[256];
    sprintf(username, "%s\n", argv[2]);
    write(c2sfd, username, 256);
    char authorisation[256];
    read(s2cfd, authorisation, 256);
    if(strcmp(authorisation, "read") == 0){
        printf("read!\n");
    }else if(strcmp(authorisation, "write") == 0){
        printf("write!\n");
        char buf[256];
        read(s2cfd,buf,256);
        printf("%s",buf);
        
    }else{
        printf("%s\n",authorisation);
        printf("error of authorisation!\n");//debug
        return 1;
    }
    doc = markdown_init();
    editlog = malloc(256);
    memset(editlog, 0, 256);
    //close(s2cfd);
    //int s2cfd2 = open(s2c,O_RDONLY | O_NONBLOCK);
    pthread_t broadcast_thread;
    pthread_create(&broadcast_thread, NULL, receive_broadcast, &s2cfd);
    char buf[256]; 
    while(1){
           
        if(fgets(buf, 256, stdin)){
            if(strncmp(buf,"DOC?\n",5) == 0){
                printf("%s",markdown_flatten(doc));
            }else if(strncmp(buf, "PERM?\n",6)==0){
                printf("%s",authorisation);
            }else if(strncmp(buf, "LOG?\n",5) == 0) {
                printf("%s\n", editlog);
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
                    printf("cilent side, send command successful\n");
                }else if (size_written<=0)
                
                {
                    printf("send command fail");    /* code */
                }
                
            }        
        }

    }
    free(editlog);
    exit(0);
    return 0;
}