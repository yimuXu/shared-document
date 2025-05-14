// TODO: server code that manages the document and handles client instructions
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

int checkauthorisation(char* username, int c2sfd, int s2cfd){
    // check if username is valid
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

void* communication_thread(void* arg){
    // create FIFO for communication
    int* clientpid = (int*)arg;
    char name1[] = "FIFO_C2S_";
    char name2[] = "FIFO_S2C_";
    char c2s[100];
    char s2c[100];
    sprintf(c2s, "%s%d",name1,*clientpid);
    sprintf(s2c, "%s%d",name2,*clientpid);
    unlink(c2s);
    unlink(s2c);
    mode_t perm = 0666;
    
    int rt1 = mkfifo(c2s, perm);
    int rt2 = mkfifo(s2c, perm);
    if(rt1 == -1){
        printf("mkfifo error!\n");
        return NULL;
    }
    if(rt2 == -1){
        printf("mkfifo error!\n");
        return NULL;
    }else{
        printf("FIFO_C2S created!\n");
    }
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
    char* bufferdoc[256];//// need to be changed
    write(s2cfd,bufferdoc, 256);
    //loop for send command of editing
    while(1){
        char* command[256];
        read(c2sfd,command, 256);
        char* commandtype = strtok(*command, " ");
        if(strcmp(commandtype, "INSERT") == 0){
            // insert command
           
            int pos = atoi(strtok(NULL, " "));
            char* content = strtok(NULL, " ");
            // call the insert function //////////////////////
            printf("insert %s at %d\n", content, pos);
            // call insert function
        }
    }
    printf("success!\n");
    close(c2sfd);
    close(s2cfd);
    unlink(c2s);
    unlink(s2c);
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
    //////////////////////int time_interval = atoi(argv[1]);

    sigset_t set;
    siginfo_t info;
    sigemptyset(&set);
    sigaddset(&set, SIGRTMIN);
    sigprocmask(SIG_BLOCK, &set, NULL);
    
    // loop to accept the new client 
    pthread_t thread;
    int infopid = sigwaitinfo(&set, &info);
    if (infopid == -1){
        printf("sigwaitinfo error!\n");
        return 1;
    }
    int clientpid = info.si_pid;
    // initilaize thread
    pthread_create(&thread, NULL, communication_thread, &clientpid);      
    pthread_join(thread, NULL);  

    

    return 0;
    
}