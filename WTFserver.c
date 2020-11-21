#include <pthread.h>
#include <stdio.h> 
#include <stdlib.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <stdlib.h> 
#include <string.h> 
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/socket.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>

#define SA struct sockaddr
#define MESSAGE_LENGTH 128
#define PATH_LENGTH 1024

//Managing Multiple Client Connections:
typedef struct ClientNode{
    int sfd;
    struct ClientNode* prev;
    struct ClientNode* next;
    char ip[16];
}ClientList;

ClientList* newNode(int socket_fd, char* IP);
//handle_client() creates a new thread for a new client connection. Then whatever request is made by the client will be handled within this function (e.i. Request a file).
void handle_client(void* client);
//END client connections declarations

//GLOBAL VARIABLES: -------
//pthread_t tid[2]; 
//int counter; 
pthread_mutex_t lock;
ClientList* root;
ClientList* crnt;

int socket_fd =0;
int client_fd =0;
//END GLOBAL VARIABLES--------

//Catches SIGINT and exit() so atexit() can handle it.
void handle_sigint(int sig);
//atexit() will call cleanup() to close, free, etc. whatever is open. 
void cleanup(void);
void chat(int socket_fd);
void receive_message();
//Finds the requested file in the servers directory
int find_and_transfer(char* target, int sfd);
//Check if file is a regular file. USED IN find_and_transfer()
int isReg(char* path);
int isDirectory(char* path);
void send_file_data(char* filename, int sfd);
void* request_file(void* filename, int sfd);
void checkout(int sfd);
void send_project_rec(char* project_path, char* master_path, int sfd);
void create_project(int sfd);
void update_project(int sfd);
void upgrade_project(int sfd);
void commit_project(int sfd);
void push_project(int sfd);
//int unlnk(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
void delete_dir(char* dir_path, char* master_path);

int main(int argc, char* argv[]){

    if(pthread_mutex_init(&lock, NULL) == 0){
        printf("Mutex init successful...\n");
    }else{
        printf("\nMutex init failed...\n");
        exit(EXIT_FAILURE);
    }

    int exit_val = atexit(cleanup);
    //printf("atexit() = %d\n", exit_val);
    signal(SIGINT, handle_sigint);

    if(argc != 2){
        printf("Incorrect number of arguments : Please use 1 argument <PORT>...\n");
        exit(1);
    }
    int port = atoi(argv[1]); //Only one command line argument --> Port number

    if(port < 4096 || port > 65535){
        printf("Please use valid port:\nYour input: %d\nAccepted: 4096 - 65535\n", port);
        exit(1);
    }else{
        printf("PORT: %d\n", port);
    }

    //int socket_fd; 
    //int client_fd;
    int client_len;

    struct sockaddr_in serveraddr;
    struct sockaddr_in cli;

    //Create Socket:
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        printf("Failed to create socket...\n");
        exit(1);
    }else {
        printf("Socket Created...\n");
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if((bind(socket_fd, (SA*)&serveraddr, sizeof(serveraddr))) != 0){
        printf("Failed to bind socket...\n");
        perror("Socket Error");
        exit(1);
    }else{
        printf("Socket Binded...\n");
    }

    if((listen(socket_fd, 5)) != 0){
        printf("Listen Failed...\n");
        exit(1);
    }else{
        printf("Listening...\n");
    }
    client_len = sizeof(cli);

    root = newNode(socket_fd, inet_ntoa(serveraddr.sin_addr));
    crnt = root;

    //printf("Server IP: %s\n", root->ip);

    while(1){
        client_fd = accept(socket_fd, (SA*)&cli, &client_len);

        //Client IP:
        getpeername(client_fd, (SA*)&cli, &client_len);
        printf("Client Connected: %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        ClientList* c = newNode(client_fd, inet_ntoa(cli.sin_addr));
        c->prev = crnt;
        crnt->next = c;
        crnt = c;

        pthread_t id;
        if(pthread_create(&id, NULL,(void*)handle_client, (void*)c) != 0){
            perror("Failed to create thread...\n");
            exit(EXIT_FAILURE);
        }
        
        //TEST:
        //exit(1);
    }

    printf("Server shutting down...\n");
    pthread_mutex_destroy(&lock);
    close(socket_fd);
    close(client_fd);
}

ClientList* newNode(int socket_fd, char* IP){
    ClientList* client = (ClientList*) malloc(sizeof(ClientList));
    client->sfd = socket_fd;
    client->prev = NULL;
    client->next = NULL;
    strncpy(client->ip, IP, 16);
    return client;
}

void handle_client(void* client){

    if(pthread_mutex_lock(&lock) == 0){
        printf("Mutex locked...\n");
    }else{
        printf("Mutex lock failed...\n");
    }
    
    int leave = 0;
    int message_length = 128;
    char receive_buff[message_length];
    char send_buff[message_length];
    ClientList* c_list = (ClientList*) client;

    

    //Receive Requests from client
    //char* error_message = (char*) malloc(128 * sizeof(char));
    int error;
    memset(receive_buff, 0, message_length);
    int receive = recv(c_list->sfd, receive_buff, message_length,0);
    printf("From Client: %s\n", receive_buff);

    char* delim = ":";

    if(strcmp(receive_buff, "ping") == 0){
        send(c_list->sfd, "pong", 5, 0);
    }
    if(strncmp(receive_buff, "request", 7) == 0){// REQUEST SINGLE FILE
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        strtok(receive_buff, delim);
        char* fname = strtok(NULL, delim);
        printf("Searching for file: %s\n", fname);

        //bzero(receive_buff, sizeof(receive_buff));
        //recv(socket_fd, receive_buff,message_length,0);

        error = find_and_transfer(fname, c_list->sfd);
        
    }
    if(strncmp(receive_buff, "req_list", 8) == 0){// CLIENT REQUESTS A LIST OF FILES -- SERVER SENDS THOSE FILES
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        /* memset(receive_buff, 0, message_length);
        recv(c_list->sfd, receive_buff, 128, 0); */

        while(1){
            memset(receive_buff, 0, message_length);
            recv(c_list->sfd, receive_buff, 128, 0);
            

            if(strcmp(receive_buff, "end_list") ==0){
                printf("End of file list...\n");
                send(c_list->sfd, "end_list", 8,0);
                break;
            }else{
                printf("Filename: %s\n", receive_buff);
                send(c_list->sfd, "Searching...",12,0);
                find_and_transfer(receive_buff, c_list->sfd);
            }
        }
    }

    if(strncmp(receive_buff, "send_list", 9) == 0){ //CLIENT WILL SEND A LIST OF FILES -- SERVER WILL WRITE THEM
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        while(1){
            memset(receive_buff, 0, message_length);
            recv(c_list->sfd, receive_buff, 128, 0);
            

            if(strcmp(receive_buff, "end_list") ==0){
                printf("End of file list...\n");
                send(c_list->sfd, "end_list", 8,0);
                break;
            }else{
                printf("Filename: %s\n", receive_buff);
                send(c_list->sfd, "Acknowledged",12,0);
                request_file(receive_buff, c_list->sfd);
                //find_and_transfer(receive_buff, c_list->sfd);
            }
        }


    }

    if(strcmp(receive_buff, "checkout") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        checkout(c_list->sfd);

    }

    if(strcmp(receive_buff, "create") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        create_project(c_list->sfd);

    }

    if(strcmp(receive_buff, "update") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        update_project(c_list->sfd);

    }

    if(strcmp(receive_buff, "upgrade") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        upgrade_project(c_list->sfd);
        

    }

    if(strcmp(receive_buff, "commit") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        commit_project(c_list->sfd);
        

    }

    if(strcmp(receive_buff, "push") == 0){
        char* confirm = "Request_Received";
        printf("To Client: %s\n", confirm);
        send(c_list->sfd, confirm, 128, 0);

        push_project(c_list->sfd);

    }


    //free(error_message);


    //END Receive Requests from client


    int stop = 0;
    printf("Waiting for client to close...\n");
    while(1){
        memset(receive_buff, 0, message_length);
        recv(c_list->sfd, receive_buff, MESSAGE_LENGTH, 0);
        
        /* if(strcmp(receive_buff, "") != 0){
            printf("From Client: %s\n", receive_buff);
        } */
        //printf("From Client: %s\n", receive_buff);

        if(strncmp(receive_buff, "close", 5) == 0 /* || stop == 128 */){
            printf("Received command to close client...\n");
            break;
        }

        if(stop == 1000){
            printf("No 'close' response from client: Closing connection...\n");
            break;
        }

        stop++;
    }
    send(c_list->sfd, "close", 5, 0);
    //recv(socket_fd, )
    
    if(pthread_mutex_unlock(&lock) == 0){
        printf("Mutex Unlocked...\n");
    }else{
        printf("Mutex unlock failed...\n");
    }

    close(c_list->sfd);
    if(c_list == crnt){
        crnt = c_list->prev;
        crnt->next = NULL;
    }else{
        c_list->prev->next = c_list->next;
        c_list->next->prev = c_list->prev;
    }

    free(c_list);
    printf("Client Disconnected...\n");
}

void handle_sigint(int sig){
    /* ClientList* ptr;
    while(root != NULL){
        printf("\nClosing Socket: %d\n", root->sfd);
        close(root->sfd);
        ptr = root;
        root = root->next;
        free(ptr);
    } */
    //close(client_fd);
    //close(socket_fd);
    //printf("\nServer Shutting down...\n");
    exit(EXIT_SUCCESS);
}

void cleanup(void){
    //signal(SIGINT, handle_sigint);
    printf("\nCleaning Up...\n");
    //handle_sigint(SIGINT);
    ClientList* ptr;
    while(root != NULL){
        printf("Closing Socket: %d\n", root->sfd);
        close(root->sfd);
        ptr = root;
        root = root->next;
        free(ptr);
    }

    if(pthread_mutex_destroy(&lock)==0){
        printf("Mutex Destroyed...\n");
    }else{
        printf("Failed to Destroy Mutex...\n");
    }

    printf("\nServer Shutting down...\n");
}

//SINGLE CLIENT CHAT
void chat(int socket_fd){
    printf("ENTERED CHAT\n");
    int leave = 0;
    char recv_buff[128];
    char send_buff[128];

    while(1){
        if(leave){
            printf("Exiting...\n");
            break;
        }
        int receive = recv(client_fd, recv_buff, 128, 0);
        if(receive > 0){
            if(strlen(recv_buff) == 0){
                continue;
            }

            if(strcmp(recv_buff,"exit") == 0){
                sprintf(send_buff, "exit");
                leave = 1;
                send(client_fd, send_buff, 128, 0);
                break;
            }

            sprintf(send_buff, "Message Recieved.");
            printf("Client: %s\n", recv_buff);
        }else if(receive == 0){
            sprintf(send_buff, "exit");
            //printf("Exiting...\n");
            leave = 1;
            //break;
        }else{
            printf("Error...\n");
            leave = 1;
        }

        send(client_fd, send_buff, 128, 0);
    }
    return;
}

void receive_message(){
    char receieveMessage[128];
    while(1){
        int recieve;
    }
}

int find_and_transfer(char* target, int sfd){
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d){
        while((dir = readdir(d)) != NULL){
            int type = isReg(dir->d_name);
            if(type != 0){
                //printf("%s\n", dir->d_name);

                if(strcmp(dir->d_name, target) == 0){
                    printf("%s FOUND...\n", dir->d_name);
                    char* success_message = "File found!";
                    send(sfd, success_message, 128, 0);
                    printf("To Client: %s\n", success_message);
                    

                    send_file_data(dir->d_name, sfd);
                    
                    //transferDataToFile(dir->d_name, "write_temp.txt");
                    closedir(d);
                    return 0;
                }

            }
        }
        printf("FILE: '%s' not found...\n", target);
        closedir(d);
    }else{
        printf("Error Number %d\n", errno);  
            perror("Program");
            //exit(1);
    }

    char* error_message = "File could not be found...";
    //printf("File could not be found...\n");
    send(sfd, error_message, 128, 0);
    printf("To Client: %s\n", error_message);

    return -1;
}

int isReg(char* path){
    struct stat statbuf;
    stat(path, &statbuf);
    return S_ISREG(statbuf.st_mode);
}

void send_file_data(char* filename, int sfd){
    char* receive_buf = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    int fd_read = open(filename, O_RDONLY);

    if(fd_read == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    char* read_buf = (char*) malloc(1024* sizeof(char));
    int r;
    int w;


    printf("Sending file Data...\n");
    //recv(socket_fd, "-status-", 8,0);

    int file_bytes = 0;
    int res;
    while((res = read(fd_read, read_buf, 1)) > 0){
        //printf("%s",read_buf);
        file_bytes+=res;
        //write(sfd, read_buf, res);
    }

    close(fd_read);
    fd_read = open(filename, O_RDONLY);
    if(fd_read == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    printf("\nBytes in the file: %d\n", file_bytes);
    char* file_contents = (char*) malloc(file_bytes * sizeof(char));
    read(fd_read, file_contents, file_bytes);
    //printf("File Contents:\n%s\n", file_contents);
    char send_amount[128];
    memset(send_amount, 0, 128);
    sprintf(send_amount, "%d", file_bytes);
    //printf("%s\n", send_amount);

    //send(sfd, send_amount, file_bytes,0);
    printf("Waiting to send file size...\n");
    while(1){
        memset(receive_buf, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buf, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buf, "send it") == 0){
            break;
        }
    }


    printf("Sending file size...\n");
    if(file_bytes == 0){
        send(sfd, "0", 1, 0);
    }else{
        send(sfd, send_amount, 128,0);
    }
    

    printf("Waiting to send data...\n");
    while(1){
        memset(receive_buf, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buf, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buf, "send it") == 0){
            break;
        }
    }

    printf("Sending file data...\n");
    if(file_bytes == 0){
        send(sfd, "", 1, 0);
    }else{
        send(sfd, file_contents, file_bytes, 0);
    }
    

    printf("Waiting for client to finish...\n");
    while(1){
        memset(receive_buf, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buf, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buf, "finished") == 0){
            break;
        }
    }

    //send(socket_fd, "finished", 8, 0);

    free(file_contents);
    free(read_buf);
    free(receive_buf);

    printf("--- END send_file_data() ---\n");
    close(fd_read);
}

void* request_file(void* filename, int sfd){
    char receive_buff[MESSAGE_LENGTH];
    char* file_name = (char*) filename;

    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    printf("From client: %s\n", receive_buff);

    if(strcmp(receive_buff, "File could not be found...") == 0){
        printf("Exiting...\n");
        //send(socket_fd, "", 1,0);
        return;
    }
    
    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);

    send(sfd, "send it", 7, 0);

    int bytes = atoi(receive_buff);
    printf("Number of bytes: %d\n", bytes);

    memset(receive_buff, 0, MESSAGE_LENGTH);
    char* file_data = (char*) malloc(bytes * sizeof(char));
    recv(sfd, file_data, bytes, 0);
    

    //WRITING TO FILE:
    int overw;
    char option[128];
    int fd_write = open(file_name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd_write == -1 && errno == 2){
        printf("Creating file...\n");
        //perror("Program");
        creat(file_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        //printf("Creating File...\n");
        fd_write = open(file_name, O_RDWR);
        write(fd_write, file_data, bytes);
    }else if(fd_write == -1 && errno != 2){
        perror("File Error");
    }else /* if(fd_write == 1) */{
        printf("File already exits! Overwrite? (yes/no)\n");
        
        //scanf("%s", option);
        while(strcmp(option, "yes") != 0 && strcmp(option, "no") != 0){
            scanf("%s", option);
            
            if(strcmp(option, "yes") == 0){
                overw = 1;
                break;
            }else if(strcmp(option, "no") == 0){
                overw = 0;
                break;
            }
            printf("Please enter 'yes' or 'no':\n");
        }
    }

    memset(option, 0, 128);

    if(overw == 0){
        printf("No overwrite: File unchanged...\n");
    }else if(overw == 1){
        printf("Overwriting file...\n");
        write(fd_write, file_data, bytes);
    }

    send(sfd, "finished", 8, 0);

    printf("FILE WRITING COMPLETE\n");
    free(file_data);
}

void checkout(int sfd){

    char* receive_buff = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    //WAIT FOR CLIENT TO SAY STOP OR CONTINUE
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            return;
        }
        if(strcmp(receive_buff, "CONT*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }

    char* success_message = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(success_message, 0, MESSAGE_LENGTH);
    strcpy(success_message, "checkout_complete");

    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);

    recv(sfd, project_name, MESSAGE_LENGTH, 0);

    printf("Project name: %s\n", project_name);

    DIR *d = opendir(project_name);
    if(!d){ //PROJECT FOLDER DOES NOT EXITS
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "project_dne");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        free(success_message);
        free(project_name);
        free(no_project);
        closedir(d);
        return;
    }
    closedir(d);

    //RECURSIVELY SEND NAMES OF DIRECTORIES AND FILES:
    send(sfd, project_name, MESSAGE_LENGTH, 0); //SEND THE NAME OF PROJECT DIRECTORY
    send_project_rec(project_name, project_name, sfd);

    send(sfd, "end_list", 8,0);


    //SEND MESSAGE: ALLOW CLIENT TO FINISH
    send(sfd, success_message, MESSAGE_LENGTH, 0);

    //WAIT FOR CLIENT FINISH:
    /* while(1){
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);

        if(strcmp(receive_buff, success_message) == 0){
            printf("Checkout Complete...\n");
            break;
        }
    } */

    free(success_message);
    free(project_name);
}

void send_project_rec(char* project_path, char* master_path, int sfd){

    char* end_list = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(end_list, 0, MESSAGE_LENGTH);
    strcpy(end_list, "end_list");

    char* path = (char*) malloc(PATH_LENGTH * sizeof(char));
    memset(path, 0, PATH_LENGTH);
    struct dirent* dp;
    DIR *d = opendir(project_path);

    if(!d){
        //perror("Send Project");
        return;
    }

    //printf("Searching Project Dir: %s...\n", project_path);

    char* dir_message = (char*) malloc(PATH_LENGTH* sizeof(char));
    memset(dir_message, 0, PATH_LENGTH);
    strcpy(dir_message, "directory:");    

    char* file_message = (char*) malloc(PATH_LENGTH* sizeof(char));
    memset(file_message, 0, PATH_LENGTH);
    strcpy(file_message, "file:");    

    while((dp = readdir(d)) != NULL){
        if(strcmp(dp->d_name,"..") != 0 && strcmp(dp->d_name,".") != 0){
            
            strcpy(path,project_path);
            strcat(path,"/");
            strcat(path, dp->d_name);
            //printf("%s\n", path);

            if(isDirectory(path)){
                strcat(dir_message, path);
                printf("%s\n", dir_message);
                send(sfd, dir_message, PATH_LENGTH,0);
                strcpy(dir_message, "directory:");
            }else if(isReg(path)){
                strcat(file_message, path);
                printf("%s\n", file_message);
                send(sfd, file_message, PATH_LENGTH,0);
                strcpy(file_message, "file:"); 
                send_file_data(path, sfd);
            }

            

            send_project_rec(path, master_path, sfd);
        }
    }

    //send(sfd, end_list, MESSAGE_LENGTH, 0);
    //printf("End Search...\n");
    closedir(d);
    free(path);
    free(dir_message);
    free(end_list);
    free(file_message);
}

int isDirectory(char* path){
    struct stat statbuf;
    stat(path, &statbuf);
    return S_ISDIR(statbuf.st_mode);
}

void create_project(int sfd){
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    /* recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    printf("Project name: %s\n", receive_buff); */

    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);
    recv(sfd, project_name, MESSAGE_LENGTH, 0);
    printf("Project name: %s\n", project_name);

    //STOP IF PROJECT ALREADY EXISTS
    DIR* d = opendir(project_name);
    if(d){
        printf("'%s' Already exists...\nExiting...\n", project_name);
        send(sfd, "STOP*", 5, 0);
        closedir(d);
        free(project_name);
        free(receive_buff);
        return;
    }else{
        send(sfd, "CONT*", 5, 0);

        //CREATE PROJECT FOLDER & .Manifest FILE INSIDE
        printf("Creating Project...\n");

        char* mani_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
        strcpy(mani_path, project_name);
        strcat(mani_path, "/.Manifest");
        printf("Manifest path: %s\n", mani_path);

        int error = mkdir(project_name, S_IRWXU | S_IRWXG | S_IRWXO);
            if(error){
                perror("Error creating directory");
            }

        //CREATE MANIFEST
        printf("Creating .Manifest...\n");
        creat(mani_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        int mani_fd = open(mani_path, O_WRONLY);
        if(mani_fd == -1){
            perror("Manifest create error");
        }
        int w = write(mani_fd, "version:0", 9);
        w = write(mani_fd, "\n", 1);
        if(w == -1){
            perror("Manifest Write error");
        }

        printf("Project creation complete...\n");
        send(sfd, "create_complete", 15, 0);

        //CLIENT WILL PERFORM checkout() on project:
        checkout(sfd);

        closedir(d);
        free(mani_path);
        free(project_name);
        free(receive_buff);
        return;
    }



    /* printf("Project creation complete...\n");
    send(sfd, "create_complete", 15, 0);
    
    closedir(d);
    free(project_name);
    free(receive_buff); */
}

void update_project(int sfd){
    printf("Starting Update...\n");

    char* receive_buff = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);
    recv(sfd, project_name, MESSAGE_LENGTH, 0);

    printf("Project name: %s\n", project_name);

    DIR *d = opendir(project_name);
    if(!d){ //PROJECT FOLDER DOES NOT EXITS
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "project_dne");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        //free(success_message);
        free(project_name);
        free(no_project);
        closedir(d);
        free(receive_buff);
        return;
    }
    closedir(d);


    //SEND CLIENT LINES FROM SERVER .Manifest:
    char* mani_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(mani_path, project_name);
    strcat(mani_path, "/.Manifest");
    printf("Manifest path: %s\n", mani_path);

    char* line = (char*) malloc(256 * sizeof(char));
    memset(line, 0, 256);


    int mani_fd = open(mani_path, O_RDONLY);    
    if(mani_fd == -1){
        perror("Failed to open manifest");
    }

    //TELL CLIENT TO START
    send(sfd, "SEND*", 5,0);

    char* read_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(read_buff, 0, MESSAGE_LENGTH);
    int r;
    //APPEND BUFFER TO LINE UNTIL END OF LINE
    int eof = 0;
    printf("Reading Lines...\n");
    while((r = read(mani_fd, read_buff, 1))>= 0){

        if(r == 0){
            printf("END OF FILE\n");
            eof = 1;
        }

        if(strcmp(read_buff, "\n") != 0){
            strcat(line, read_buff);
        }else if(strcmp(read_buff, "\n") == 0 || eof == 1){

            if(eof == 1){
                strcpy(line, "LAST*");
            }

            printf("Line: %s\n", line); 
            //send(sfd, line, 256, 0);
            
            //WAIT FOR CLIENT TO REQUEST LINE
            printf("Waiting to send line...\n");
            while(1){

                if(strcmp(receive_buff, "SEND*") == 0){
                    printf("SENDING LINE...\n");
                    send(sfd, line, 256, 0);
                    break;
                }

                memset(receive_buff, 0, MESSAGE_LENGTH);
                recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
            }

            memset(line, 0, 256);
        }
        



        memset(read_buff, 0, 1);
        if(eof == 1){
            break;
        }
    }

    

    //NOTIFY CLIENT WHEN LIST ENDS
    printf("END OF LIST\n");
    send(sfd, "end_list", 8, 0);

    //memset(receive_buff, 0, MESSAGE_LENGTH);
    //recv(sfd, receive_buff, MESSAGE_LENGTH, 0);

    //WAIT FOR CLIENT TO FINISH
    printf("Client: %s\n", receive_buff);
    while(1){
        //printf("Client: %s\n", receive_buff);
        if(strcmp(receive_buff, "end_list") == 0){
            printf("Client: %s\n", receive_buff);
            memset(receive_buff, 0, MESSAGE_LENGTH);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }

    printf("Update Completed...\n");
    while(strcmp(receive_buff, "end_list") == 0){
        send(sfd, "update_complete", 15, 0);
        memset(receive_buff, 0, MESSAGE_LENGTH);
    }
    send(sfd, "update_complete", 15, 0);

    memset(receive_buff, 0, MESSAGE_LENGTH);
    free(line);
    free(read_buff);
    free(mani_path);
    free(project_name);
    free(receive_buff);
    return;
}

void upgrade_project(int sfd){

    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    //WAIT FOR CLIENT TO SAY STOP OR CONTINUE
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            return;
        }
        if(strcmp(receive_buff, "CONT*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }



    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);

    recv(sfd, project_name, MESSAGE_LENGTH, 0);

    printf("Project name: %s\n", project_name);

    DIR *d = opendir(project_name);
    if(!d){ //PROJECT FOLDER DOES NOT EXITS
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "project_dne");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        //free(success_message);
        free(project_name);
        free(no_project);
        closedir(d);
        return;
    }
    closedir(d);

    printf("TO CLIENT: GO\n");
    send(sfd, "GO*", 3, 0);
    //WAIT FOR CLIENT TO REQUEST FOR FILE UNTIL STOP COMMAND IS RECIEVED:

    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    printf("CLIENT: %s\n", receive_buff);

    printf("SENDING...\n");
    //recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            //printf("CLIENT: %s\n", receive_buff);
            break;
        }

        if(strcmp(receive_buff, "") !=0){
            char* filename = (char*) malloc(MESSAGE_LENGTH*sizeof(char));
            strcpy(filename, receive_buff);
            //memset(receive_buff, 0, MESSAGE_LENGTH);
            //recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
            printf("Filename: %s\n", filename);
            send(sfd, "ACK", 3, 0);
            send_file_data(filename, sfd);
        }


        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }
    printf("CLIENT: %s\n", receive_buff);



    printf("Upgrade Complete...\n");
    send(sfd, "upgrade_complete", 16,0);

    free(receive_buff);
}

void commit_project(int sfd){
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    //WAIT FOR CLIENT TO SAY STOP OR CONTINUE
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            return;
        }
        if(strcmp(receive_buff, "CONT*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }



    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);

    recv(sfd, project_name, MESSAGE_LENGTH, 0);

    printf("Project name: %s\n", project_name);

    DIR *d = opendir(project_name);
    if(!d){ //PROJECT FOLDER DOES NOT EXITS
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "project_dne");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        //free(success_message);
        free(receive_buff);
        free(project_name);
        free(no_project);
        closedir(d);
        return;
    }
    closedir(d);

    //CHECK IF MANIFEST EXISTS
    char* mani_path = (char*) malloc(MESSAGE_LENGTH*sizeof(char));
    strcpy(mani_path, project_name);
    strcat(mani_path, "/.Manifest");
    int mani_fd = open(mani_path, O_RDONLY);
    if(mani_fd == -1){
        printf("Project .Manifest doesn't exists...\n");
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "no_manifest");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        //free(success_message);
        free(receive_buff);
        free(project_name);
        free(no_project);
        closedir(d);
        return;
    }

    send(sfd, "bytes", 5, 0);
    //SEND MANIFEST INFO
    //read how many bytes manifest has, send to client
    //make string hold that many bytes from manifest
    //send it to client
    int r;
    int bytes;
    char* read_buff = (char*) malloc(MESSAGE_LENGTH*sizeof(char));
    
    while((r = read(mani_fd, read_buff, 1))>0){
        bytes+=r;
        
    }
    printf("Bytes in Manifest: %d\n", bytes);
    close(mani_fd);
    mani_fd = open(mani_path, O_RDONLY);
    char* mani_contents = (char*) malloc(bytes);
    read(mani_fd, mani_contents, bytes);
    printf("Manifest contents:\n%s\n", mani_contents);
    char mani_size[MESSAGE_LENGTH];
    sprintf(mani_size, "%d", bytes);
    //printf("mani_size: %s\n", mani_size);
    send(sfd, mani_size, MESSAGE_LENGTH,0);
    send(sfd, mani_contents, bytes, 0);


    //GET .Commit from Client  
    //printf("client: %s\n", receive_buff);
    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(sfd, receive_buff, MESSAGE_LENGTH,0 );
    //printf("client: %s\n", receive_buff);
    char commit_path[MESSAGE_LENGTH];
    strcpy(commit_path, receive_buff);
    printf("commit_path: %s\n", receive_buff);
    //request_file(commit_path, sfd);
    int commit_fd = open(commit_path, O_RDWR);
    if(commit_fd != -1){
        printf("Commit exists.\n");
        //close(commit_fd);
        //unlink(commit_path);
        //remove(commit_path);
        //commit_fd = open(commit_path, O_RDWR);
    }
    if(commit_fd == -1 && errno == 2){
        printf("Creating .Commit file...\n");
        creat(commit_path, S_IRUSR | S_IWUSR | S_IXUSR);
        commit_fd = open(commit_path, O_RDWR | O_APPEND);
    }

    //send(sfd, "send it", 7, 0);
    bytes = 0;
    char commit_size[MESSAGE_LENGTH];
    memset(commit_size, 0, MESSAGE_LENGTH);
    recv(sfd, commit_size, MESSAGE_LENGTH, 0);
    //printf("Commit size: %s\n", commit_size);
    bytes = atoi(commit_size);
    printf("Commit bytes: %d\n", bytes);
    char* commit_contents = (char*) malloc(bytes);
    
    recv(sfd, commit_contents, bytes, 0);
    printf("Commit Contents:\n'%s'\n", commit_contents);
    //send(sfd, "finished", 8,0);

    //write to .Commit:
    write(commit_fd, commit_contents, bytes);


    printf("Commit Complete...\n");
    send(sfd, "commit_complete", 16,0);

    free(receive_buff);
    free(project_name);
    //free(no_project);
}

void push_project(int sfd){
    char* receive_buff = (char*) malloc(PATH_LENGTH * sizeof(char));
    memset(receive_buff, 0, PATH_LENGTH);

    //WAIT FOR CLIENT TO SAY STOP OR CONTINUE
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            return;
        }
        if(strcmp(receive_buff, "CONT*") == 0){
            printf("CLIENT: %s\n", receive_buff);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    }

    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* project_name = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(project_name, 0, MESSAGE_LENGTH);

    recv(sfd, project_name, MESSAGE_LENGTH, 0);

    printf("Project name: %s\n", project_name);

    DIR *d = opendir(project_name);
    if(!d){ //PROJECT FOLDER DOES NOT EXITS
        char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
        memset(no_project, 0, MESSAGE_LENGTH);
        strcpy(no_project, "project_dne");

        perror("Project");
        printf("Exiting...\n");
        send(sfd, no_project, MESSAGE_LENGTH, 0);
        //free(success_message);
        free(receive_buff);
        free(project_name);
        free(no_project);
        closedir(d);
        return;
    }
    closedir(d);

    send(sfd, "GO*", 3, 0);

    
    memset(receive_buff, 0, MESSAGE_LENGTH);
    char commit_path[MESSAGE_LENGTH];
    strcpy(commit_path, project_name);
    strcat(commit_path, "/.Commit");
    int commit_fd = open(commit_path, O_RDWR);
    if(commit_fd == -1 && errno == 2){
        printf("Creating .Commit file...\n");
        creat(commit_path, S_IRUSR | S_IWUSR | S_IXUSR);
        commit_fd = open(commit_path, O_RDWR | O_APPEND);
    }

    //memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("clIEnt: %s\n", receive_buff);


    //GET .Commit from Client  
    //send(sfd, "send it", 7, 0);
    int bytes = 0;
    char commit_size[MESSAGE_LENGTH];
    strcpy(commit_size, receive_buff);
    //recv(sfd, commit_size, MESSAGE_LENGTH, 0);
    //printf("Commit size: %s\n", commit_size);
    bytes = atoi(commit_size);
    printf("Commit bytes: %d\n", bytes);
    char* commit_contents = (char*) malloc(bytes);
    
    recv(sfd, commit_contents, bytes, 0);
    printf("Commit Contents:\n'%s'\n", commit_contents);
    //send(sfd, "finished", 8,0);
    
    //MAKE SERVER'S COMMIT A STRING:
    char* serv_commit_str = (char*) malloc(bytes);
    read(commit_fd, serv_commit_str, bytes);
    printf("SERVER .Commit CONTENTS:\n%s\n", serv_commit_str);

    char copy_path[MESSAGE_LENGTH];
    strcpy(copy_path, "Copy_");
    strcat(copy_path, project_name);

    //CHECK IF THEY ARE THE SAME
    if(strcmp(serv_commit_str, commit_contents) == 0){
        printf("CLIENT AND SERVER HAVE SAME COMMIT\n");

        //CREATE COPY_<PROJECT NAME> DIRECTORY
        /* char copy_path[MESSAGE_LENGTH];
        strcpy(copy_path, "Copy_");
        strcat(copy_path, project_name); */
        printf("Copy of Project: %s\n", copy_path);
        mkdir(copy_path, S_IRWXU | S_IRWXG | S_IRWXO);

        //RECEIVE FILENAMES UNTIL end_list:
        memset(receive_buff, 0, MESSAGE_LENGTH);
        //recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
        
        while(1){
            //send(sfd, "GO*", 3, 0);
            if(strcmp(receive_buff, "end_list")== 0){
                printf("End of List\n");
                break;
            }else if(strcmp(receive_buff, "") != 0){
                //printf("filepath: %s\n", receive_buff);
                //TOKENIZE TO GET FILEPATH:
                char filepath[MESSAGE_LENGTH];
                strcpy(filepath, receive_buff);
                char* mode = strtok(filepath, ":");
                char* fp = strtok(NULL, ":");

                //CREATE FILE OR SUBDIR
                if(strcmp(mode, "file") == 0){
                    send(sfd, "GO*", 3, 0);
                    printf("Creating file: %s\n", fp);
                    //creat(fp, O_RDWR);
                    /* int file_fd = open(fp, O_RDWR);
                    if(file_fd == -1 && errno == 2){
                        creat(fp, O_RDWR);
                        file_fd = open(fp, O_RDWR);
                    } */
                    //GET FILE DATA FROM CLIENT
                    bytes = 0;
                    char file_size[PATH_LENGTH];
                    //memset(receive_buff, 0, PATH_LENGTH);
                    recv(sfd, receive_buff, PATH_LENGTH, 0);
                    printf("Rec_buf: %s\n", receive_buff);
                    recv(sfd, file_size, PATH_LENGTH, 0);
                    //recv(sfd, receive_buff, MESSAGE_LENGTH, 0);

                    //memset(receive_buff, 0, PATH_LENGTH);
                    //recv(sfd, receive_buff, bytes, 0);

                    //printf("File size: %s\n", file_size);
                    bytes = atoi(file_size);
                    printf("File size: %d\n", bytes);
                    char* file_contents = (char*) malloc(bytes);

                    memset(receive_buff, 0, PATH_LENGTH);
                    recv(sfd, receive_buff, bytes, 0);

                    //recv(sfd, file_contents, bytes, 0);
                    if(bytes == 0){
                        file_contents = (char*) malloc(1);
                        file_contents = "";
                    }
                    printf("File contents: '%s'\n", receive_buff);

                    int file_fd = open(fp, O_RDWR);
                    if(file_fd == -1 && errno == 2){
                        printf("Creating File...\n");
                        //close(file_fd);
                        creat(fp, S_IRUSR | S_IWUSR | S_IXUSR);
                        file_fd = open(fp, O_RDWR);
                    }


                    write(file_fd, receive_buff, bytes);
                    close(file_fd);
                }else if(strcmp(mode, "directory") == 0){
                    printf("Creating directory: %s\n", fp);
                    mkdir(fp, S_IRWXU | S_IRWXG | S_IRWXO);
                }
            }
            
            memset(receive_buff, 0, MESSAGE_LENGTH);
            recv(sfd, receive_buff, MESSAGE_LENGTH, 0);
        }


        //SCAN COMMIT FOR CHANGES TO BE MADE:
            //CHANGE MANIFEST BASED ON THESE CHANGES
                //--> INCREMENT MANIFEST VERSION
                //--> COPY ALL NON D: TAGGED LINES
            //REMOVE ANY FILES WITH D: TAG
        
        //get current mani version
        char mani_path[MESSAGE_LENGTH];
        strcpy(mani_path, copy_path);
        strcat(mani_path, "/.Manifest");
        //printf("mani_path: %s\n", mani_path);
        int mani_fd = open(mani_path, O_RDONLY);
        if(mani_fd == -1){
            printf("Cant open manifest...\n");
        }
        char read_buff[10];
        int r;
        char version_str[10];
        while((r = read(mani_fd, read_buff, 1)) > 0){
            
            if(strcmp(read_buff, "\n") == 0){
                break;
            }

            if(strcmp(read_buff, "\n") != 0){
                strcat(version_str, read_buff);
            }
            memset(read_buff, 0, 10);
        }
        //printf("Mani version: %s\n", version_str);
        strtok(version_str, ":");
        char* vers_str = (char*) malloc(1); 
        vers_str = strtok(NULL, ":");
        int version = atoi(vers_str);
        version++;
        //printf("new version: %d\n", version);
        sprintf(vers_str, "%d", version);
        //printf("'%s'\n", vers_str);
        //memset(version_str, 0, 10);
        //strcpy(version_str, "version:");
        //strcat(version_str, vers_str);
        char new_version_str[11];
        strcpy(new_version_str, "version:"); 
        strcat(new_version_str, vers_str);
        printf("new- '%s'\n", new_version_str);

        //replace .Manifest with new version and then commit contents.
        close(mani_fd);
        //int unlink_status = unlink(mani_path);
        /* if(unlink_status == EBUSY){
            perror("Manifest unlink error");
        } */
        //close(mani_fd);
        //remove(mani_path);
        mani_fd = open(mani_path, O_WRONLY | O_TRUNC);
        write(mani_fd, "", 1);
        close(mani_fd);
        mani_fd = open(mani_path, O_WRONLY);
        if(mani_fd == -1 && errno == 2){
            //printf("Creating File...\n");
            //close(mani_fd);
            creat(mani_path, S_IRUSR | S_IWUSR | S_IXUSR);
            mani_fd = open(mani_path, O_WRONLY);
        }
        write(mani_fd, new_version_str, strlen(new_version_str));
        write(mani_fd,"\n", 1);
        close(mani_fd);
        mani_fd = open(mani_path, O_RDWR | O_APPEND);
        //Write contents of commit without first tag (w/o first two characters)

        int comm_lines = 0;
        int i;
        for(i = 0; i < strlen(commit_contents); i++){
            if(commit_contents[i] == '\n'){
                comm_lines++;
            }
        }
        printf("Number of lines in commit: %d\n", comm_lines);

        //char line[MESSAGE_LENGTH];
       // memset(line, 0, MESSAGE_LENGTH);
        //char* serv_mani_contents_copy = (char*) malloc(bytes);
        //strcpy(serv_mani_contents_copy, serv_mani_contents);
        char* commit_array[comm_lines];
        int index = 0;
        char* token = strtok(commit_contents, "\n");
        //char* token_array[4];
        commit_array[index] = token;
        while(token != NULL){
            //printf("TOKEN: %s\n", token);
            //memset(token_array[index], 0, MESSAGE_LENGTH);
            token = strtok(NULL, "\n");
            index++;
            commit_array[index] = token;
        }
        //WRITE LINES WITHOUT FIRST TAG:
        index = 0;
        for(index = 0; index < comm_lines; index++){
            char* mode = strtok(commit_array[index], ":");
            char* line = strtok(NULL, "");
            char line_copy[MESSAGE_LENGTH];
            strcpy(line_copy, line);

            if(strcmp(mode, "D") != 0){
                printf("write: %s\n", line);
                write(mani_fd, line, strlen(line));
                write(mani_fd, "\n", 1);
            }else{
                memset(copy_path, 0, MESSAGE_LENGTH);
                strcpy(copy_path, "Copy_");
                strtok(line_copy, ":");
                char* remove_path = strtok(NULL, ":");
                strcat(copy_path, remove_path);

                printf("removing %s\n", copy_path);
                int cp_fd = open(copy_path, O_RDWR | O_TRUNC);
                close(cp_fd);
                unlink(copy_path);
                //remove(copy_path);
            }
        }
        close(mani_fd);
        close(commit_fd);

        //REMOVE OLD PROJECT DIR
        //RENAME PROJECT COPY
        //ftw
        printf("\nRemoving old project directory...\n");
        strcpy(copy_path, "Copy_");
        strcat(copy_path, project_name);
        delete_dir(project_name, project_name);
        //ftw(project_name, remove, 20);
        //perror("FTW");
        rename(copy_path, project_name);
        //nftw(copy_path, )


    }else{
        printf("Commits are different. Finshed.\n");
    }






    printf("Push Complete...\n");
    send(sfd, "push_complete", 13,0);

    close(commit_fd);
    free(receive_buff);
    free(project_name);
}

void delete_dir(char* dir_path, char* master_path){

    char* path = (char*) malloc(PATH_LENGTH * sizeof(char));
    memset(path, 0, PATH_LENGTH);

    struct dirent* dp;
    DIR *d = opendir(dir_path);

    if(!d){
        //perror("Send Project");
        return;
    }

    //printf("Searching Project Dir: %s...\n", project_path);

    /* char* dir_message = (char*) malloc(PATH_LENGTH* sizeof(char));
    memset(dir_message, 0, PATH_LENGTH);
    strcpy(dir_message, "directory:");    

    char* file_message = (char*) malloc(PATH_LENGTH* sizeof(char));
    memset(file_message, 0, PATH_LENGTH);
    strcpy(file_message, "file:");     */

    while((dp = readdir(d)) != NULL){
        if(strcmp(dp->d_name,"..") != 0 && strcmp(dp->d_name,".") != 0){
            
            strcpy(path,dir_path);
            strcat(path,"/");
            strcat(path, dp->d_name);
            printf("%s\n", path);

            if(isDirectory(path)){
                //strcat(dir_message, path);
                //printf("%s\n", dir_message);
                //send(sfd, dir_message, PATH_LENGTH,0);
                //strcpy(dir_message, "directory:");
            }else if(isReg(path)){
                
                unlink(path);
                //remove(path);
                perror("remove");
                //strcat(file_message, path);
                //printf("%s\n", file_message);
                //send(sfd, file_message, PATH_LENGTH,0);
                //strcpy(file_message, "file:"); 
                //send_file_data(path, sfd);
            }

            
            delete_dir(path, master_path);
            //send_project_rec(path, master_path, sfd);
        }
    }
    closedir(d);
    rmdir(dir_path);
}