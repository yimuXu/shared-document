//TODO: client code that can send instructions to server.
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>


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
    printf("client open fifo\n");

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
    //send username to server
    char username[256];
    sprintf(username, "%s\n", argv[2]);
    write(c2sfd, username, 256);
    char authorisation[256];
    read(s2cfd, authorisation, 256);
    if(strcmp(authorisation, "read\n") == 0){
        printf("read!\n");
    }else if(strcmp(authorisation, "write\n") == 0){
        printf("write!\n");
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // write to FIFO
            write(c2sfd, buffer, strlen(buffer)+1);
        }
    }else{
        printf("error!\n");//debug
        return 1;
    }
    return 0;



}