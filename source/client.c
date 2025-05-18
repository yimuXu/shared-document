//TODO: client code that can send instructions to server.
#include "markdown.h"

document* doc;
char* editlog;

void* receive_broadcast (void* arg){
    int* s2cfd = (int*)arg;
    printf("client is receiving broadcast from server!\n");
    while(1){
        int size = read(*s2cfd,editlog,256);
        if(size > 0){
            // update the local documant;

        }else if(size == 0){
            printf("close pipe!");
            break;
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
    // argv[1] is server pid, argv[2] is username
    kill(serverpid, SIGRTMIN);
    // initialize signal set
    /////////////////////int sig = -1;
    sigset_t set;
    struct timespec timeout = {1, 0};
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN+1);
    sigprocmask(SIG_BLOCK, &set, NULL);
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
    }else{
        printf("%s\n",authorisation);
        printf("error of authorisation!\n");//debug
        return 1;
    }
    close(s2cfd);
    int s2cfd2 = open(s2c,O_RDONLY | O_NONBLOCK);
    pthread_t broadcast_thread;
    pthread_create(&broadcast_thread, NULL, receive_broadcast, &s2cfd2);
    
    editlog = "";
    while(1){
    char buf[256];    
    if(fgets(buf, 256, stdin)){
        if(strncmp(buf,"DOC?\n",256) == 0){
            printf("%s",markdown_flatten(doc));
        }else if(strncmp(buf, "PERM?\n",256)==0){
            printf("%s",authorisation);
        }else if(strncmp(buf, "LOG?\n",256)) {
            printf("%s\n", editlog);
        }else if(strncmp(buf, "DISCONNECT\n",256) == 0) {
            write(c2sfd,buf,256);
            close(c2sfd);
            close(s2cfd);
            unlink(c2s);
            unlink(s2c);
            break;
        }else{
            write(c2sfd,buf,256);
        }        
    }

    }
    return 0;



}