#include <pthread.h>
#include <netdb.h> 
#include <stdio.h> 
#include <openssl/opensslconf.h>
#include <stdlib.h> 
#include <string.h> 
#include <strings.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/opensslv.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>


#define SA struct sockaddr 
#define MESSAGE_LENGTH 128
#define PATH_LENGTH 1024

int socket_fd =0;
int client_fd =0;

void cleanup(void);
void configure(char* IP, char* port);
//void sendMessage(int socket_fd);
void delay(int seconds);
void handle_sigint(int sig);
void chat(int socket_fd);
void* request_file(void* filename);
void* request_list(void* filename);
void* send_list(void* filename);
void send_file_data(char* filename);
int isReg(char* path);
void* find_and_transfer(void* target);
void file_create_hash(char* filename, unsigned char* hash);
void hash_to_string(unsigned char* hash, char* string_ptr);
char* find_file_path(char* project_name, char* masterPath, char* filename, char* path);
char* manifest_create_entry(char* project_name, char* masterPath, char* filename, char* entry);
void manifest_remove_entry(char* project_name, char* filename);
void checkout(char* project_name);
void create_project(char* project_name);
void update_project(char* project_name);
void strtok_to_array(char* original, char* str_arr[128]);
void string_array_get_element(char* string_arr[], int index, char* output);
void upgrade_project(char* project_name);
void commit_project(char* project_name);
void push_project(char* project_name);
void send_project_rec(char* project_path, char* master_path, int is_copy);
int isDirectory(char* path);
void send_f_data(char* filepath);


int main(int argc, char* argv[]){

    signal(SIGINT, handle_sigint);
    atexit(cleanup);

    if(argc < 2){
        printf("Missing arguments: Please specifiy a function and include its required parameters...\n");
        exit(1);
    }

    //**** CONFIGURE *****
    if(strcmp(argv[1], "configure") == 0 ){ 
        if(argv[2] == NULL || argv[3] == NULL){
            printf("Arguments missing : Please include arguments <IP> <PORT>, in that order...\n");
            exit(1);
        }
        configure(argv[2], argv[3]);
        return 0;
    }
    //**** END CONFIGURE *****


    //**** ADD ****
    if(strcmp(argv[1], "add") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project directory name...");
            exit(EXIT_FAILURE);
        }
        if(argv[3] == NULL){
            printf("Error: Third argument must be a file name...");
            exit(EXIT_FAILURE);
        }

        char* entry = (char*) malloc(256 * sizeof(char));
        memset(entry, 0, 256);

        manifest_create_entry(argv[2], argv[2], argv[3], entry);

        
        free(entry);
        return 0;
    }
    //**** END ADD ****


    //**** REMOVE *****
    if(strcmp(argv[1], "remove") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project directory name...");
            exit(EXIT_FAILURE);
        }
        if(argv[3] == NULL){
            printf("Error: Third argument must be a file name...");
            exit(EXIT_FAILURE);
        }

        manifest_remove_entry(argv[2], argv[3]);
        return 0;
    }
    //**** END REMOVE *****

     if(strcmp(argv[1], "rollback") == 0){
         printf("Rollback not implemented...\n");
         return 0;
     }

     if(strcmp(argv[1], "history") == 0){
         printf("History not implemented...\n");
         return 0;
     }

    if(strcmp(argv[1], "currentversion") == 0){
         printf("Current Version not implemented...\n");
         return 0;
     }

    if(strcmp(argv[1], "Destroy") == 0){
         printf("Destroy not implemented...\n");
         return 0;
     }


    //********** GET .CONFIGURE INFORMATION *********
    int configure_fd = open(".configure", O_RDONLY);
    //atexit();
    if(configure_fd == -1){
        printf(".configure file is missing, cannot continue...\n");
        exit(1);
    }

    int r;
    char* config_info = (char*) malloc(128 * sizeof(char));
    memset(config_info, 0, 128);
    char* read_buf = (char*) malloc(128 *sizeof(char));
    memset(read_buf, 0, 128);
    
    while((r = read(configure_fd, read_buf, 1)) > 0){
        strcat(config_info, read_buf);
    }
    
    free(read_buf);
    close(configure_fd);
    printf("Config info: '%s'\n", config_info);
    char* delim = ":";
    char* IP = strtok(config_info, delim);
    char* PORT = strtok(NULL, delim);
    printf("IP: '%s'\nPORT: '%s'\n", IP, PORT);
    //free(config_info);
    //********** END GET .CONFIGURE INFORMATION *********



    //******* CONNECT TO SERVER *********
    //int socket_fd; 
    //int client_fd;

    struct sockaddr_in serveraddr;
    struct sockaddr_in cli;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        printf("Failed to create socket...\n");
        exit(1);
    }else {
        printf("Socket Created...\n");
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(IP);
    serveraddr.sin_port = htons(atoi(PORT));

    int stop = 0;
    if(connect(socket_fd, (SA*)&serveraddr, sizeof(serveraddr)) != 0){
        printf("Failed to connect to server...\n");
        printf("Retrying connection...\n");
        do{
            int i; 
            for (i = 0; i < 10; i++) { 
                // delay of one second 
                delay(1); 
                //printf("%d seconds have passed\n", i + 1); 
                stop++;
                if(stop == 300){
                    printf("Server took too long to respond...\nExiting");
                    exit(1);
                }
            } 
            
        }while(connect(socket_fd, (SA*)&serveraddr, sizeof(serveraddr)) != 0);

        printf("Connected to server...\n");
        //exit(1);
    }else{
        printf("Connected to server...\n");
    }
    free(config_info);
    //******* END CONNECT TO SERVER *********

    //TESTING:
    //chat(socket_fd);

    //REQUESTS FOR SERVER:
    int message_length = 128;
    char receive_buff[message_length];
    char send_buff[message_length];
    
    if(strcmp(argv[1], "ping") == 0){//Pings the server. Server responds with "pong."
        printf("Sending ping...\n");
        send(socket_fd, "ping", 5, 0);
        recv(socket_fd, receive_buff, message_length, 0);
        //printf("From server: %s\n", receive_buff);
    }else if(strcmp(argv[1], "request") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a file name...");
            exit(EXIT_FAILURE);
        }

        char* filename = argv[2];
        char* action = (char*) malloc(128*sizeof(char));
        memcpy(action, "request", 7);
        strcat(action, ":");
        strcat(action, filename);
        //printf("Requesting: %s\n", action);
        send(socket_fd, action, 128, 0);

        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        //printf("From server: %s\n", receive_buff);

        request_file(filename);
        free(action);
    }else if(strcmp(argv[1], "request_list") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a file name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memcpy(action, "req_list", 8);

        printf("Requesting: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        request_list(argv[2]);
        free(action);
    }else if(strcmp(argv[1], "send_list") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a file name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memcpy(action, "send_list", 9);

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        send_list(argv[2]);

        //request_list(argv[2]);
        free(action);
    }else if(strcmp(argv[1], "checkout") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "checkout");

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        checkout(argv[2]);

        
        free(action);
    }else if(strcmp(argv[1], "create") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "create");

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        //CREATE
        create_project(argv[2]);
        
        free(action);
    }else if(strcmp(argv[1], "update") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "update");

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        
        update_project(argv[2]);
        
        free(action);
    }else if(strcmp(argv[1], "upgrade") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "upgrade");

        //printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        upgrade_project(argv[2]);
        //manifest_remove_entry(argv[2], "test.txt");
        
        free(action);
    }else if(strcmp(argv[1], "commit") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "commit");

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        commit_project(argv[2]);
        
        free(action);
    }else if(strcmp(argv[1], "push") == 0){
        if(argv[2] == NULL){
            printf("Error: Second argument must be a project name...");
            exit(EXIT_FAILURE);
        }

        char* action = (char*) malloc(128*sizeof(char));
        memset(action, 0, 128);
        strcpy(action, "push");

        printf("Sending: %s\n", action);
        send(socket_fd, action, 128, 0);
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
        //printf("From Server: %s\n", receive_buff);

        push_project(argv[2]);
        
        free(action);
    }else{
        printf("INVALID COMMAND");
        exit(1);
    }

    

    //END REQUESTS FOR SERVER:

    memset(receive_buff, 0, MESSAGE_LENGTH);
    send(socket_fd, "close", 5, 0);
    recv(socket_fd, receive_buff, message_length,0);
    //printf("Server close: %s\n", receive_buff);
    printf("Disconnecting from server...\n");
    //close(socket_fd); //Disconnect from server
    //close(client_fd);
    exit(EXIT_SUCCESS);
}

void cleanup(void){
    printf("\nCleaning Up...\n");
    
    close(client_fd);
    close(socket_fd);
}

void configure(char* IP, char* port){

    int port_num = atoi(port);
    if(port_num < 0){
        printf("Error: Port cannot be negative...\n");
        return;
    }
    if(port_num > 65535 || port_num == 0 || port_num == 1023){
        printf("Error: Invalid port...\n");
        return;
    }
    

    int fd = open(".configure", O_WRONLY);

    if(fd == -1 && errno == 2){
        //perror("Program");
        creat(".configure", S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        //printf("Creating File...\n");
        fd = open(".configure", O_WRONLY);
    }

    printf("Configuring...\nIP: %s\nPORT: %s\n", IP, port);

    int w = write(fd, IP, strlen(IP));
    w = write(fd, ":", 1);
    w = write(fd, port, strlen(port));
    
    printf("Configure Complete...\n");

    close(fd);
}

void delay(int seconds){
    int milli_seconds = 1000 * seconds;
    clock_t start = clock();
    while (clock() < start + milli_seconds){}
}

void handle_sigint(int sig){
    printf("Caught signal %d: Exiting...\n", sig); 
    exit(1);
}

void chat(int socket_fd){
    int leave = 0;
    char recieveMessage[128];
    char sendMessage[128];

    while(1){
        if(leave == 1){
            printf("Exiting chat...\n");
            break;
        }
        //printf("Enter String: \n");
        int n = 0;
        while((sendMessage[n++] = getchar()) != '\n');
        //write(socket_fd, sendMessage, sizeof(sendMessage));
        send(socket_fd, sendMessage, 128, 0);

        int recieve = recv(socket_fd, recieveMessage, 128, 0);
        if(recieve > 0){
            //printf("From server: %s\n", recieveMessage);
        }else if(recieve == 0 || strcmp(recieveMessage, "exit") == 0){
            //printf("From server: %s\n", recieveMessage);
            leave = 1;
            //break;
        }

        if(strcmp(sendMessage, "exit\n") == 0){
            printf("Exiting chat...\n");
            leave = 1;
            //break;
        } 
    }
    return;
}

void* request_file(void* filename){
    
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* file_name = (char*) filename;

    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("From server: %s\n", receive_buff);

    if(strcmp(receive_buff, "File could not be found...") == 0){
        printf("Exiting...\n");
        //send(socket_fd, "", 1,0);
        free(receive_buff);
        //free(file_name);
        return;
    }

    //SERVER WILL SEND DATA STREAM, CLIENT WRITES IT
    //send(socket_fd, "", 1,0);
    
    /* memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);

    printf("Bytes: %s\n", receive_buff); */

    int bytes;
    
    send(socket_fd, "send it", 7, 0);
    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("Recieve_buf: %s\n", receive_buff);
    int stop = 0;
    while(1){
        //printf("Server: %d\n", receive_buff);
        if(strcmp(receive_buff, "") != 0 || stop == 256){
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        stop++;
    }
    /* memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0); */

    //printf("Bytes: %s\n", receive_buff);

    /* if(receive_buff != NULL && strcmp(receive_buff, "") != 0){
        bytes = atoi(receive_buff);
    } */
    bytes = atoi(receive_buff);
    //printf("Number of bytes: %d\n", bytes);

    send(socket_fd, "send it", 7, 0);

    memset(receive_buff, 0, MESSAGE_LENGTH);
    char* file_data = (char*) malloc(bytes);
    if(bytes != 0){
        //file_data = (char*) malloc(1000);
        memset(file_data, 0, bytes);
        recv(socket_fd, file_data, bytes, 0);
    }else{
        //file_data = (char*) malloc(bytes * sizeof(int));
        memset(file_data, 0, bytes);
        recv(socket_fd, file_data, bytes, 0);
    }
    
    
    //send(socket_fd, "-status-", 8,0);
    //printf("FILE CONTENTS:\n%s\n", file_data);
    //perror("Issue");
    //send(socket_fd, "", 1,0);

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

    send(socket_fd, "finished", 8, 0);

    /* printf("Waiting for server finish confirmation...\n");
    while(1){
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buff, "finished") == 0){
            break;
        }
    } */

    printf("FILE WRITING COMPLETE\n");
    free(file_data);
    free(receive_buff);
}

void* request_list(void* filename){
    char receive_buff[MESSAGE_LENGTH];
    char* file_name = (char*) filename;

    //char* file_name = (char*) filename;
    int fd_read = open(file_name, O_RDONLY);
    if(fd_read == -1){
        perror("Program"); 
        return;
    }
    int r;
    char* read_buf = (char*) malloc(sizeof(char));
    char fn[128];
    while((r = read(fd_read, read_buf, 1)) > 0){
        if(r == 0){
            break;
        }
        if(strcmp(read_buf, "\n") != 0){
            strcat(fn, read_buf);
        }else{
            //printf("Filename: %s\n", fn);

            if(strcmp(fn, file_name) == 0){
                //printf("Avoiding %s\n", fn);
                memset(fn, 0, 128);
                continue;
            }

            send(socket_fd, fn, 128,0);
            memset(receive_buff, 0,MESSAGE_LENGTH);
            recv(socket_fd, receive_buff, 128,0);
            //printf("Server: %s\n", receive_buff);

            request_file(fn);

            memset(fn, 0, 128);
        }
    }
    send(socket_fd, "end_list", 8,0);
    //printf("To Server: end_list\n");

    while(1){
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buff, "end_list") == 0){
            break;
        }
    }
    
    
    free(read_buf);
    //free(action);
}

void* send_list(void* filename){
    char receive_buff[MESSAGE_LENGTH];
    char* file_name = (char*) filename;

    //char* file_name = (char*) filename;
    int fd_read = open(file_name, O_RDONLY);
    if(fd_read == -1){
        perror("Program"); 
        return;
    }
    int r;
    char* read_buf = (char*) malloc(sizeof(char));
    char fn[128];
    while((r = read(fd_read, read_buf, 1)) > 0){
        if(r == 0){
            break;
        }
        if(strcmp(read_buf, "\n") != 0){
            strcat(fn, read_buf);
        }else{
            //printf("Filename: %s\n", fn);

            if(strcmp(fn, file_name) == 0){
                //printf("Avoiding %s\n", fn);
                memset(fn, 0, 128);
                continue;
            }

            send(socket_fd, fn, 128,0);
            memset(receive_buff, 0,MESSAGE_LENGTH);
            recv(socket_fd, receive_buff, 128,0);
            //printf("Server: %s\n", receive_buff);

            find_and_transfer(fn);
            //request_file(fn);

            memset(fn, 0, 128);
        }
    }
    send(socket_fd, "end_list", 8,0);
    //printf("To Server: end_list\n");

    while(1){
        memset(receive_buff, 0,MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buff, "end_list") == 0){
            break;
        }
    }
    
    
    free(read_buf);
}

void send_file_data(char* filename){
    char* receive_buf = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    int fd_read = open(filename, O_RDONLY);

    if(fd_read == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    char* read_buf = (char*) malloc(1024* sizeof(char));
    int r;
    int w;


    //printf("Sending file Data...\n");
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

    //printf("\nBytes in the file: %d\n", file_bytes);
    char* file_contents = (char*) malloc(file_bytes * sizeof(char));
    read(fd_read, file_contents, file_bytes);
    //printf("File Contents:\n%s\n", file_contents);
    char send_amount[128];
    sprintf(send_amount, "%d", file_bytes);
    //printf("%s\n", send_amount);

    send(socket_fd, send_amount, file_bytes,0);
    //printf("Waiting to send data...\n");
    while(1){
        memset(receive_buf, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buf, MESSAGE_LENGTH, 0);
        if(strcmp(receive_buf, "send it") == 0){
            break;
        }
    }

    //printf("Sending...\n");
    send(socket_fd, file_contents, file_bytes, 0);

    //printf("Waiting for client to finish...\n");
    while(1){
        memset(receive_buf, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buf, MESSAGE_LENGTH, 0);
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

void* find_and_transfer(void* target){
    char* tar = (char*) target;
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if(d){
        while((dir = readdir(d)) != NULL){
            int type = isReg(dir->d_name);
            if(type != 0){
                //printf("%s\n", dir->d_name);

                if(strcmp(dir->d_name, tar) == 0){
                   // printf("%s FOUND...\n", dir->d_name);
                    char* success_message = "File found!";
                    send(socket_fd, success_message, 128, 0);
                    //printf("To Client: %s\n", success_message);
                    

                    send_file_data(dir->d_name);
                    
                    //transferDataToFile(dir->d_name, "write_temp.txt");
                    closedir(d);
                    return;
                }

            }
        }
        //printf("FILE: '%s' not found...\n", tar);
        closedir(d);
    }else{
        printf("Error Number %d\n", errno);  
            perror("Program");
            //exit(1);
    }

    char* error_message = "File could not be found...";
    //printf("File could not be found...\n");
    send(socket_fd, error_message, 128, 0);
    //printf("To Client: %s\n", error_message);

    //return -1;
}

int isReg(char* path){
    struct stat statbuf;
    stat(path, &statbuf);
    return S_ISREG(statbuf.st_mode);
}

void file_create_hash(char* filename, unsigned char* hash){
    int fd = open(filename, O_RDONLY);
    if(fd == -1){
        perror("File");
        return;
    }

    int r;
    int bytes =0;

    /* do{
        char* read_buf = (char*) malloc(1 * sizeof(char));
        r = read(fd, read_buf, 1);
        bytes+=r;
        free(read_buf);
    }while (r != 0); */

    char* read_buf = (char*) malloc(1024 * sizeof(char));
    memset(read_buf, 0, 1024);
    while((r = read(fd, read_buf, 1)) > 0){
        //printf("%s", read_buf);
        bytes += r;
        memset(read_buf, 0, 1024);
    }
    //printf("bytes: %d\n", bytes);
    free(read_buf);

    close(fd);
    fd = open(filename, O_RDONLY);
    

    char* file_contents = (char*) malloc(bytes * sizeof(char));
    
    read(fd, file_contents, bytes);
    //printf("Last char: %c\n", file_contents[strlen(file_contents)-1]);
    /* int w = write(fd, file_contents, bytes);
    if(w == -1){
        perror("Write");
    } */
    //printf("File Contents: %s\n", file_contents);
    //char hash[20];
    
    hash = SHA256(file_contents, strlen(file_contents), hash);
    /* char h_string[65];
    hash_to_string(hash, h_string);
    printf("%s\n", h_string); */
    

    free(file_contents);
    close(fd);
    return;
}

void hash_to_string(unsigned char hash[SHA256_DIGEST_LENGTH], char string_ptr[65]){
    int i;
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++){
        sprintf(string_ptr + (i*2), "%02x", hash[i]);
    }
    //string_ptr[65] = '\0';
}

char* manifest_create_entry(char* project_name, char* masterPath, char* filename, char* entry){
    int located = 0;
    char path[1000];
    memset(path, 0, 1000);
    struct dirent* dp;
    DIR *d = opendir(project_name);

    if(!d){
        printf("Project does not exist...\n");
        perror("Directory");
        return "";
    }
    //printf("Opening Project: '%s'\n", project_name);
    //strcat(path, project_name);
    //printf("path: %s\n", path);

    int mani_fd;
    find_file_path(project_name, project_name, ".Manifest", path);
    //printf("Manifest path: %s\n", path);
    mani_fd = open(path, O_RDWR | O_APPEND);
    if(mani_fd == -1){
        perror(".Manifest");
        return "";
    }

    char* mani_path = (char*) malloc(256* sizeof(char));
    strcpy(mani_path, path);

    memset(path, 0, 1000);
    //int found = 0;
    find_file_path(project_name, project_name, filename, path);
    //printf("File path: %s\n", path);
    if(strcmp(path, "") == 0){
        printf("File Not Found...\n");
        exit(EXIT_FAILURE);
    }

    int file_fd = open(path, O_RDONLY);
    if(file_fd == -1){
        perror("File");
        exit(EXIT_FAILURE);
    }

    //CHECK FOR ENTRY VERSION
    int line_number = 0;
    char* delim = ":";
    char* version = (char*) malloc(128 * sizeof(char));
    memset(version, 0, 128);
    strcpy(version, "0");
    char* read_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(read_buff, 0, MESSAGE_LENGTH);
    char* line = (char*) malloc(256 * sizeof(char));
    memset(line, 0, 256);
    int r;
    int bytes;

    // READ LINE-BY-LINE, THEN TOKENIZE FOR COMPARISONS
    while((r = read(mani_fd, read_buff, 1)) > 0){
        //printf("%s", read_buff);

        //End of line
        if(strcmp(read_buff, "\n") == 0 && line_number > 0){
            //printf("Line: '%s'\n", line);

            /* if(strcmp(line, "") == 0){
                break;
            } */

            if(strcmp(line, "\n") == 0){ // SKIP EMPTY LINES
                line_number++;
                strcat(line, read_buff);
                memset(read_buff, 0, 1);
                continue;
            }

            //TOKENIZE
            char* temp_ver = strtok(line, delim);
            char* temp_path = strtok(NULL, delim);
            //printf("temp_ver: %s\n", temp_ver);
            int ver = atoi(temp_ver);
            //printf("Old Version: %d\n", ver);
            
            //printf("Found path: %s\n", temp_path);

            if(strcmp(temp_path, path) == 0){
                ver++;
                //printf("New Version: %d\n", ver);
                sprintf(version, "%d", ver);
                //printf("New Version: %s\n", version);
                //manifest_remove_entry(project_name, temp_path);
                located = 1;
            }

            memset(line, 0, 256);
        }
        
        line_number++;
        /* if(strcmp(read_buff, "\n") != 0){
            strcat(line, read_buff);
        } */
        strcat(line, read_buff);
        memset(read_buff, 0, 1);
    }

    //memset(line, 0, 256);

    if(located == 1){
        manifest_remove_entry(project_name, filename);
    }
    close(mani_fd);

    mani_fd = open(mani_path, O_RDWR | O_APPEND);
    if(mani_fd == -1){
        perror(".Manifest");
        return "";
    }
    
    unsigned char* hash = (char*) malloc(SHA256_DIGEST_LENGTH);
    char hash_string[65];
    memset(hash_string, 0, 65);
    file_create_hash(path, hash);
    hash_to_string(hash, hash_string);
    //printf("File hash: %s\n", hash_string);
    //printf("Version: %s\n", version);

    strcat(entry, version);
    strcat(entry, delim);
    strcat(entry, path);
    strcat(entry, delim);
    strcat(entry, hash_string);

    printf("\nENTRY: %s\n", entry);

    int w; 
    //w = write(mani_fd, "\n", 1);
    w = write(mani_fd, entry, strlen(entry));
    w = write(mani_fd, "\n", 1);

    if(w == -1){
        perror("Write Error");
        exit(EXIT_FAILURE);
    }

    free(version);
    free(line);
    free(read_buff);
    free(hash);
    close(file_fd);
    close(mani_fd);
    closedir(d);
    return entry;
}

char* find_file_path(char* project_name, char* masterPath, char* filename, char* path){
    char search_path[1000];
    struct dirent* dp;
    DIR *d = opendir(project_name);

    if(!d){
        //perror("Find File Path");
        return "";
    }

    while((dp = readdir(d)) != NULL){
        if(strcmp(dp->d_name,"..") != 0 && strcmp(dp->d_name,".") != 0){
            
            strcpy(search_path,project_name);
            strcat(search_path,"/");
            strcat(search_path, dp->d_name);
            //printf("%s\n", search_path);
           // printf("%s\n", dp->d_name);

            int type = isReg(search_path);
            if(strcmp(dp->d_name, filename) == 0 && type){
                memcpy(path, search_path, sizeof(search_path));
                //printf("Found file!\nPATH: %s\n", path);
                closedir(d);
                //found = 1;
                return path;
            }
            

            find_file_path(search_path, masterPath, filename, path);
        }
    }

    /* if(found != 1){
        printf("File Not Found...\n");
        closedir(d);
        memset(path, 0, 1000);
        return path;
    } */

    closedir(d);
    return path;
}

//Create temp file
//Copy everything except data you want removed from original file
//Replace original .Manifest with temp file 
//  --> Delete Original, Rename temp to .Manifest
void manifest_remove_entry(char* project_name, char* filename){
    int located = 0;
    char path[1000];
    memset(path, 0, 1000);
    struct dirent* dp;
    DIR *d = opendir(project_name);

    if(!d){
        printf("Project does not exist...\n");
        perror("Directory");
        return;
    }

    int mani_fd;
    find_file_path(project_name, project_name, ".Manifest", path);
    //printf("Manifest path: %s\n", path);
    mani_fd = open(path, O_RDWR | O_APPEND);
    if(mani_fd == -1){
        perror(".Manifest");
        return;
    }
    //memset(path, 0, 1000);

    char* mani_path = (char*) malloc(256 * sizeof(char));
    strcpy(mani_path, path);

    //printf("Path: %s\n", path);
    char* temp_path = (char*) malloc(256 * sizeof(char));
    memset(temp_path, 0, 256);
    //strcpy(temp_path, "./");
    strcpy(temp_path, project_name);
    strcat(temp_path, "/Manifest_temp.txt");
    //printf("Temp Path: %s\n", temp_path);

    //creat(temp_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int temp_fd = open(temp_path, O_WRONLY/*  | O_APPEND */, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(temp_fd == -1 && errno == 2){
        creat(temp_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        temp_fd = open(temp_path, O_WRONLY /* | O_APPEND */);
        /* perror("Temp file");
        return; */
    }

    if (temp_fd == -1) { 
        printf("Error Number %d\n", errno);  
        perror("Program");   
        return;               
    } 

    //COPY DATA FROM .Manifest to Manifest_temp.txt:
    //Read in line by line
    //Check the file path and compare to file to be removed
    //If they match, don't write to temp file
    memset(path, 0, 1000);// --> RESETS PATH: NOW USED FOR ENTRY FILEPATH DATA
    char* delete_path = (char*) malloc(256 * sizeof(char));
    //printf("Remove File: %s\n", filename);
    find_file_path(project_name, project_name, filename, path);
    //printf("File entry to remove: '%s'\n", path);
    char* delim = ":";
    char* read_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(read_buff, 0, MESSAGE_LENGTH);
    char* line = (char*) malloc(256 * sizeof(char));
    char* line_copy = (char*) malloc(256 * sizeof(char));
    memset(line_copy, 0, 256);
    memset(line, 0, 256);
    int r;
    int w;
    while((r = read(mani_fd, read_buff, 1)) > 0){
        if(strcmp(read_buff, "\n") == 0){
            

            /* if(strcmp(line, "\n") == 0){ // SKIP EMPTY LINES
                strcat(line, read_buff);
                memset(read_buff, 0, 1);
                continue;
            } */

            //printf("Line: '%s'\n", line);
            if(strncmp(line, "\n", 1) ==0){
                strncpy(line_copy, line + 1, strlen(line) - 1);
            }else{
                strcpy(line_copy, line);
            }
            //printf("LineCopy: '%s'\n", line_copy);
            if(strcmp(line_copy, "\n") == 0 || strcmp(line_copy, "") == 0 || strcmp(line_copy, "\n\n") == 0){
                //printf("Skipping line...\n");
                memset(read_buff, 0, MESSAGE_LENGTH);
                memset(line, 0, 256);
                continue;
            }

            //TOKENIZE
            char* temp_ver = strtok(line, delim);
            char* path_data = strtok(NULL, delim);

            if(path_data == NULL){
                //printf("Path data is null\n");
                memset(read_buff, 0, MESSAGE_LENGTH);
                memset(line, 0, 256);
                continue;
            }
            //int ver = atoi(temp_ver);
            //printf("Old Version: %d\n", ver);
            
            //printf("Found path: %s\n", path_data);
            if(strcmp(path_data, path) != 0){
                //printf("Writing: '%s'\n", line_copy);
                
                w = write(temp_fd, line_copy, strlen(line_copy));
                w = write(temp_fd, "\n", 1);
                if(w == -1){
                    perror("Write to temp");
                    exit(EXIT_FAILURE);
                }

            }else{
                
                //printf("Removing %s\n", path);

                if(located == 0){
                    located = 1;
                    /* char* removed = (char*) malloc(256 * sizeof(char));
                    memset(removed, 0, 256);
                    strcpy(removed, "0:");
                    strcat(removed, path);
                    strcat(removed, ":removed");

                    //printf("Removed: %s\n", removed);
                
                    write(temp_fd, removed, strlen(removed));
                    write(temp_fd, "\n", 1);

                    free(removed); */
                }
                

                //strcat(line, read_buff);
                memset(read_buff, 0, MESSAGE_LENGTH);
                memset(line, 0, 256);
                continue;
            }
            memset(line, 0, 256);
        }
        
        strcat(line, read_buff);
        memset(read_buff, 0, MESSAGE_LENGTH);
    }

    //printf("COMPLETED\n");

    /* if(located == 1){

        //DELETE AND RENAME:
        //printf("mani path: %s\n", mani_path);
        //printf("temp mani path: %s\n", temp_path);
        remove(mani_path);
        rename(temp_path, mani_path);
    }else{
        close(mani_fd);
        close(temp_fd);
    } */

    close(mani_fd);
    close(temp_fd);

    remove(mani_path);
    rename(temp_path, mani_path);
    
    free(mani_path);
    free(delete_path);
    free(temp_path);
    free(line);
    free(line_copy);
    
}

void checkout(char* project_name){

    DIR* d = opendir(project_name);
    if(d){
        printf("'%s' Already exists on client...\n", project_name);
        send(socket_fd, "STOP*", 5, 0);
        return;
    }
    send(socket_fd, "CONT*", 5, 0);

    //printf("CHECKING OUT...\n");
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* success_message = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(success_message, 0, MESSAGE_LENGTH);
    strcpy(success_message, "checkout_complete");

    char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(no_project, 0, MESSAGE_LENGTH);
    strcpy(no_project, "project_dne");

    char* end_list = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(end_list, 0, MESSAGE_LENGTH);
    strcpy(end_list, "end_list");

    /* char* exists = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    //find_file_path(".", ".", "list.txt", exists);
    printf("EXISTS: %s\n", exists); */



    send(socket_fd, project_name, strlen(project_name), 0);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("From server: %s\n", receive_buff);
    if(strcmp(receive_buff, no_project) == 0){
        printf("Project does not exit...\n");
        free(receive_buff);
        free(success_message);
        free(no_project);
        return;
    }

    if(strcmp(receive_buff, project_name) == 0){
        //printf("Creating Project folder...\n");
        //printf("Creating Directory: %s\n", project_name);
        int error = mkdir(project_name, S_IRWXU | S_IRWXG | S_IRWXO);
        if(error){
             perror("Error creating directory");
        }
    }
    
    //GET LIST OF FILES AND FOLDERS FROM SERVER: CREATE THEM LOCALLY
    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    while(1){
        if(strcmp(receive_buff, "") != 0 && strcmp(receive_buff, end_list) != 0){
            //printf("SERVER file: %s\n", receive_buff);
            
            //CREATE FILE/DIRECTORY
            char* delim = ":";
            char* mode = strtok(receive_buff, delim);
            char* file_path = strtok(NULL, delim);

            if(strcmp(mode, "file") == 0){
                //printf("Creating file: %s\n", file_path);
                request_file(file_path);
                
            }else if(strcmp(mode, "directory") == 0){
               // printf("Creating Directory: %s\n", file_path);
                int error = mkdir(file_path, S_IRWXU | S_IRWXG | S_IRWXO);
                if(error){
                    perror("Error creating directory");
                }
            }

        }
        
        if(strcmp(receive_buff, end_list) == 0){
            //printf("End of file list...\n");
            //send(socket_fd, success_message, MESSAGE_LENGTH, 0);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }



    //WAIT FOR SERVER CLOSE CONFIRMATION
    while(1){
        
        //printf("SERVER: %s\n", receive_buff);
        if(strcmp(receive_buff, success_message) == 0){
            printf("Checkout Complete...\n");
            //send(socket_fd, success_message, MESSAGE_LENGTH, 0);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }

    //free(exists);
    free(receive_buff);
    free(success_message);
}

void create_project(char* project_name){
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);
    

    //SEND PROJECT NAME TO SERVER
    send(socket_fd, project_name, MESSAGE_LENGTH, 0);

    //WAIT FOR SERVER TO SAY STOP OR CONTINUE
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "STOP*") == 0){
            printf("SERVER: %s\nProject already exists on server...\n", receive_buff);
            return;
        }
        if(strcmp(receive_buff, "CONT*") == 0){
            printf("SERVER: %s\n", receive_buff);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    }

    //WAIT FOR SERVER TO FINISH
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    while(1){
        if(strcmp(receive_buff, "create_complete") == 0){
            printf("Project Creation Completed...\n");
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    }

    //CHECKOUT PROJECT THAT WAS CREATED
    checkout(project_name);

    free(receive_buff);
}

void update_project(char* project_name){

    int break_loop = 0;
    char* delim = ":";
    
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(no_project, 0, MESSAGE_LENGTH);
    strcpy(no_project, "project_dne");


    send(socket_fd, project_name, strlen(project_name), 0);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("SERVER: %s\n", receive_buff);
    //EXIT IF THE PROJECT DOES NOT EXIST ON THE SERVER
    if(strcmp(receive_buff, no_project) == 0){
        printf("Project does not exit...\n");
        free(receive_buff);
        //free(success_message);
        free(no_project);
        return;
    }


    //WAIT FOR SERVER TO SIGNAL START:
    while(1){
        //STOP WHEN END OF LIST MESSAGE
        if(strcmp(receive_buff, "SEND*") == 0){
            //printf("List Starting...\n");
            break;
        }

        //send(socket_fd, "SEND*", 5,0);

        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        
    }   


    send(socket_fd, "SEND*", 5,0);
    memset(receive_buff, 0, MESSAGE_LENGTH);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);

    /* //OPEN CLIENT MANIFEST:
    char* mani_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(mani_path, project_name);
    strcat(mani_path, "/.Manifest");
    printf("Manifest path: %s\n", mani_path);
    int mani_fd_cli = open(mani_path, O_RDWR);
    if(mani_fd_cli == -1){
        perror("Manifest Open Error");
    } */
    char* mani_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(mani_path, 0, MESSAGE_LENGTH);
    strcpy(mani_path, project_name);
    strcat(mani_path, "/.Manifest");
    //printf("Manifest path: %s\n", mani_path);
    //GET SERVER MANIFEST: SERVER SENDS DATA LINE-BY-LINE
    //GET CLIENT ENTRIES
    //COMPARE ENTRIES OF CLIENT AND SERVER

    char* cli_version = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* cli_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    //memset(cli_path, 0, MESSAGE_LENGTH);
    char* cli_hash = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* cli_mani_version = (char*) malloc(MESSAGE_LENGTH * sizeof(char));

    char* serv_version = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* serv_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* serv_hash = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* serv_mani_version = (char*) malloc(MESSAGE_LENGTH * sizeof(char));

    unsigned char* live_hash = (char*) malloc(SHA256_DIGEST_LENGTH);
    char* live_hash_string = (char*) malloc( 65 * sizeof(char));

    char* serv_mani_string = (char*) malloc(2048);
    memset(serv_mani_string, 0, 2048);

    int client_has_file = 0;
    int line_number = 0;
    int end_list = 0;
    int same_ver = 0;
    int up_to_date = 0;
    while(1){
        
        //client_has_file =0;

        if(break_loop == 1){
            break;
        }
        
        //STOP WHEN END OF LIST MESSAGE
        if(strcmp(receive_buff, "end_list") == 0){
            //printf("\nSERVERS MANIFEST:\n%s\n\n", serv_mani_string);

            //printf("End of list...\n");
            //break_loop = 1;
            //end_list =1;
            //send(socket_fd, "end_list", 8, 0);
            break;
        }else if(strcmp(receive_buff, "") != 0){
            //printf("Server Line: %s\n", receive_buff);

            char* serv_line_copy = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
            strcpy(serv_line_copy, receive_buff);
            //printf("Server_Line_Copy: %s\n", serv_line_copy); 
            strcat(serv_mani_string, serv_line_copy);
            line_number++;
            //client_has_file = 0;

            //(SERVER) store tokens in array:
            int index = 0;
            char* serv_token = strtok(serv_line_copy, delim);
            char* serv_token_array[4];
            serv_token_array[index] = serv_token;
            while(serv_token != NULL){
                //printf("SERVERTOKEN: %s\n", serv_token);
                //memset(token_array[index], 0, MESSAGE_LENGTH);
                serv_token = strtok(NULL, delim);
                index++;
                serv_token_array[index] = serv_token;
            }

            memset(serv_version, 0, MESSAGE_LENGTH);
            string_array_get_element(serv_token_array, 0, serv_version);
            //printf("cli_version: %s\n", cli_version);

            memset(serv_path, 0, MESSAGE_LENGTH);
            string_array_get_element(serv_token_array, 1, serv_path);
            //printf("cli_path: %s\n", cli_path);

            if(strcmp(serv_version, "version") != 0){
                memset(serv_hash, 0, MESSAGE_LENGTH);
                string_array_get_element(serv_token_array, 2, serv_hash);
                //printf("Client stored hash: %s\n", cli_hash);
            }else{
                //cli_mani_version = strtok(NULL, delim);
                //printf("Token_array[1]: %s\n", token_array[1]);
                memset(serv_mani_version, 0, MESSAGE_LENGTH);
                string_array_get_element(serv_token_array, 1, serv_mani_version);
            }




            //SCAN THROUGH CLIENT MANIFEST AND COMPARE LINES
            //IF MANIFEST VERSIONS MATCH: SUCCESS --> WRITE EMTPY .Update, Delete any .Conflict
            //IF DIFFERENT MANIFEST VERSIONS: FAILURE
            //IF STORED HASH (client) IS DIFFERENT FROM LIVE HASH AND SERVER: FAILURE
            
            //OPEN CLIENT MANIFEST:            
            int mani_fd_cli = open(mani_path, O_RDWR);
            if(mani_fd_cli == -1){
                perror("Manifest Open Error");
            }
            char* cli_line = (char*) malloc(256 * sizeof(char));
            memset(cli_line, 0, 256);
            char* read_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
            memset(read_buff, 0, MESSAGE_LENGTH);
            int r;
            
            while((r = read(mani_fd_cli, read_buff, 1)) > 0){

                if(break_loop == 1){
                    break;
                }  

                if(strcmp(read_buff, "\n") != 0){
                    strcat(cli_line, read_buff);
                }else{
                    //printf("\tClient Line: %s\n", cli_line);
                    char* cli_line_copy = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                    strcpy(cli_line_copy, cli_line);
                    //printf("\tClient Line_Copy: %s\n", cli_line_copy);

                    
                    
                    //store tokens in array (CLIENT)
                    index = 0;
                    char* token = strtok(cli_line_copy, delim);
                    char* token_array[4];
                    token_array[index] = token;
                    while(token != NULL){
                        //printf("TOKEN: %s\n", token);
                        //memset(token_array[index], 0, MESSAGE_LENGTH);
                        token = strtok(NULL, delim);
                        index++;
                        token_array[index] = token;
                    }
                    /* //(SERVER) store tokens in array:
                    index = 0;
                    char* serv_token = strtok(serv_line_copy, delim);
                    char* serv_token_array[4];
                    serv_token_array[index] = serv_token;
                    while(serv_token != NULL){
                        printf("SERVERTOKEN: %s\n", serv_token);
                        //memset(token_array[index], 0, MESSAGE_LENGTH);
                        serv_token = strtok(NULL, delim);
                        index++;
                        serv_token_array[index] = serv_token;
                    } */

                    memset(cli_version, 0, MESSAGE_LENGTH);
                    string_array_get_element(token_array, 0, cli_version);
                    //printf("cli_version: %s\n", cli_version);

                    memset(cli_path, 0, MESSAGE_LENGTH);
                    string_array_get_element(token_array, 1, cli_path);
                    //printf("cli_path: %s\n", cli_path);


                    if(strcmp(cli_version, "version") != 0){
                        memset(cli_hash, 0, MESSAGE_LENGTH);
                        string_array_get_element(token_array, 2, cli_hash);
                        //printf("Client stored hash: %s\n", cli_hash);
                    }else{
                        //cli_mani_version = strtok(NULL, delim);
                        //printf("Token_array[1]: %s\n", token_array[1]);
                        memset(cli_mani_version, 0, MESSAGE_LENGTH);
                        string_array_get_element(token_array, 1, cli_mani_version);
                    }
                    //char* serv_version = strtok(receive_buff, delim);
                    //char* serv_path;
                    //char* serv_hash;
                    //char* serv_mani_version;
                    /* if(strcmp(cli_version, "version") != 0){
                    }else{
                        //memset(serv_hash, 0, MESSAGE_LENGTH);
                        //string_array_get_element(serv_token_array, 2, serv_hash);
                        //serv_mani_version = strtok(NULL, delim);
                    } */

                    //CHECKING MANIFEST VERSIONS:
                    //printf("Client verson: %s\n", token_array[0]);
                    //printf("Server Version: %s\n", serv_version);
                    //string_array_get_element(token_array, 0, cli_version);
                    //strcat(cli_version, token_array[0]);
                    if(strcmp(cli_version, "version") == 0 && strcmp(serv_version, "version") == 0){
                        if(strcmp(cli_mani_version, serv_mani_version) == 0){ //MANIFEST VERSIONS ARE THE SAME: SUCCESS
                            up_to_date = 1;
                            printf("Up To Date\n");
                            char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                            char* conflict_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                            strcpy(update_path, project_name);
                            strcat(update_path, "/.Update");
                            strcpy(conflict_path, project_name);
                            strcat(conflict_path, "/.Conflict");
                            //printf(".Update path: %s\n.Conflict path: %s\n", update_path, conflict_path);
                            //Make empty .Update
                            int update_fd = open(update_path, O_RDWR);
                            if(update_fd == -1 && errno == 2){
                                perror(".Update");
                                creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
                            }else{
                                close(update_fd);
                                remove(update_path);
                                creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
                            }
                            //Delete .Conflict
                            int conflict_fd = open(conflict_path, O_RDONLY);
                            if(conflict_fd != -1){
                                close(conflict_fd);
                                remove(conflict_path);
                            }

                            //send(socket_fd, "end_list", 8, 0);
                            //break_loop = 1;
                            same_ver = 1;
                            free(update_path);
                            free(conflict_path);
                            //break;
                        }

                        //printf("Manifest Versions are DIFFERENT...\n");

                    }else{
                        //****LINE IS NOT FIRST****

                        //MODIFY: IF ENTRY HAS DIFFERENT VERSION AND HASH compared to server
                            // --> LIVE HASH *MATCHES* WHAT CLIENT HAS STORED
                        //ADD: CLIENT DOES NOT HAVE CERTAIN FILES THAT SERVER DOES
                        //DELETE: CLIENT HAS FILE THAT SERVER DOES NOT

                        if(strcmp(cli_version, "version") != 0 && strcmp(serv_version, "version") != 0){
                            memset(live_hash, 0, SHA256_DIGEST_LENGTH);
                            memset(live_hash_string, 0, 65);
                            file_create_hash(cli_path, live_hash);
                            hash_to_string(live_hash, live_hash_string);


                            //printf("CLIENT: %s:%s\n", cli_version, cli_path);
                            //printf("SERVER: %s:%s\n", serv_version, serv_path);

                            if(strcmp(cli_path, serv_path) == 0){
                                client_has_file = 1;
                                //printf("FILE MATCH... COMPARING: %s\n", cli_path);
                                //printf("Live hash: %s\n", live_hash_string);
                                //printf("Client stored hash: %s\n", cli_hash);
                                //strcat(cli_hash, "\0");
                                //cli_hash[65] = '\0';
                                //live_hash[65] = '\0';
                                //printf("Live hash: '%s'\n", live_hash_string);
                                //printf("Clie hash: '%s'\n", cli_hash);
                                //printf("Server stored hash: %s\n", serv_hash);

                                //MODIFY
                                if(strcmp(serv_version, cli_version) != 0 && strcmp(serv_hash, cli_hash) != 0 && strcmp(cli_hash, live_hash_string) == 0){
                                    //printf("file is MODIFIED\n");
                                    char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                                    char* update_message = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                                    strcpy(update_path, project_name);
                                    strcat(update_path, "/.Update");

                                    strcpy(update_message, "M:");
                                    strcat(update_message, cli_path);
                                
                                    int update_fd = open(update_path, O_RDWR | O_APPEND);
                                    if(update_fd == -1 && errno == 2){
                                        perror(".Update");
                                        creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
                                        update_fd = open(update_path, O_RDWR | O_APPEND);
                                        
                                    }else if(update_fd == -1){
                                        perror(".Update");
                                    }
                                    //write(update_fd, "M:", 2);
                                    //write(update_fd, cli_path, MESSAGE_LENGTH);
                                    write(update_fd, update_message, strlen(update_message));
                                    write(update_fd, ":", 1);
                                    write(update_fd, live_hash_string, strlen(live_hash_string));
                                    write(update_fd, "\n", 1);

                                    printf("%s\n", update_message); //OUTPUT MODIFY MESSAGE
                                    free(update_path);
                                    free(update_message);
                                    close(update_fd);
                                }else if(strcmp(serv_hash, cli_hash) != 0 && strcmp(cli_hash, live_hash_string) != 0){

                                    //CONFLICT

                                    printf("CONFLICT FOUND\n");
                                    char* conflict_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                                    char* conflict_message = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                                    strcpy(conflict_path, project_name);
                                    strcat(conflict_path, "/.Conflict");

                                    strcpy(conflict_message, "C:");
                                    strcat(conflict_message, cli_path);
                                
                                    int conflict_fd = open(conflict_path, O_RDWR | O_APPEND);
                                    if(conflict_fd == -1 && errno == 2){
                                        perror(".Conflict");
                                        creat(conflict_path, S_IRUSR | S_IWUSR | S_IXUSR);
                                        conflict_fd = open(conflict_path, O_RDWR | O_APPEND);
                                        
                                    }else if(conflict_fd == -1){
                                        perror(".Update");
                                    }

                                    write(conflict_fd, conflict_message, strlen(conflict_message));
                                    write(conflict_fd, ":", 1);
                                    write(conflict_fd, live_hash_string, strlen(live_hash_string));
                                    write(conflict_fd, "\n", 1);

                                    printf("%s\n", conflict_message); //OUTPUT conlfict MESSAGE
                                    printf("Please fix conflicts before next update!\n");

                                    free(conflict_message);
                                    free(conflict_path);
                                    close(conflict_fd);
                                }

                                break;

                            }

                            /* //CHECK IF CLIENT HAS THE SAME FILE THAT THE SERVER DOES
                            if(client_has_file == 0){
                                printf("CLIENT DOES NOT HAVE FILE: %s\n", serv_path);
                            }else{
                                
                            } */


                            
                        }

                        
                    }
                    //free(cli_line_copy);


                    memset(cli_line, 0, 256);
                }

                /* //CHECK IF CLIENT HAS THE SAME FILE THAT THE SERVER DOES
                if(client_has_file == 0){
                    printf("CLIENT DOES NOT HAVE FILE: %s\n", serv_path);
                }else{

                } */
                //client_has_file = 0;
                memset(read_buff, 0, 1);
            }
            //APPEND BUFFER TO LINE UNTIL END OF LINE

            //printf("client has file %s = %d\n", serv_path, client_has_file);
            //printf("Server line number: %d\n", line_number);
            if(line_number > 1 && client_has_file == 0){
                //printf("\nCLIENT DOES NOT HAVE FILE: %s\n\n", serv_path);

                char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                char* update_message = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                strcpy(update_path, project_name);
                strcat(update_path, "/.Update");

                strcpy(update_message, "A:");
                strcat(update_message, serv_path);
                                
                int update_fd = open(update_path, O_RDWR | O_APPEND);
                if(update_fd == -1 && errno == 2){
                    perror(".Update");
                    creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
                    update_fd = open(update_path, O_RDWR | O_APPEND);
                                        
                }else if(update_fd == -1){
                    perror(".Update");
                }
                //write(update_fd, "M:", 2);
                //write(update_fd, cli_path, MESSAGE_LENGTH);
                write(update_fd, update_message, strlen(update_message));
                write(update_fd, ":", 1);
                write(update_fd, serv_hash, strlen(serv_hash));
                write(update_fd, "\n", 1);

                printf("%s\n", update_message); //OUTPUT MODIFY MESSAGE
                free(update_path);
                free(update_message);
                close(update_fd);
            }

            free(cli_line);
        }

        //send(socket_fd, "SEND*", 5,0);

        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
        client_has_file = 0;
        
    }   

    if(up_to_date == 1){
        char update_path[MESSAGE_LENGTH];
        strcpy(update_path, project_name);
        strcat(update_path, "/.Update");
        int update_fd = open(update_path, O_RDWR);
        if(update_fd == -1 && errno == 2){
            perror(".Update");
            creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
        }else{
            close(update_fd);
            remove(update_path);
            creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
        }
        //Delete .Conflict
        /* int conflict_fd = open(conflict_path, O_RDONLY);
        if(conflict_fd != -1){
            close(conflict_fd);
            remove(conflict_path);
        } */
    }

    //memset(receive_buff, 0, MESSAGE_LENGTH);
    //recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);

    //printf("\nSERVERS MANIFEST:\n%s\n\n", serv_mani_string);
    int mani_fd = open(mani_path, O_RDONLY);
    if(mani_fd == -1){
        perror("ERROR opening .Manifest");
    }
    int n;
    char* rb = (char*) malloc(1024);
    memset(rb, 0, 1024);
    char* ln = (char*) malloc(256 * sizeof(char));
    memset(ln, 0, 256);
    while((n = read(mani_fd, rb, 1))> 0 && same_ver == 0){
        if(strcmp(rb, "\n") != 0){
            strcat(ln, rb);
        }else{
            //printf("line: %s\n", ln);
            if(strncmp(ln, "version", 7) != 0){
                //printf("line: %s\n", ln);
                strtok(ln, delim);
                char* lpath = strtok(NULL, delim);
                char* lhash = strtok(NULL, delim);
                //printf("path: %s\n", lpath);
                //printf("hash: %s\n", lhash);

                if(strstr(serv_mani_string, lpath) == NULL){
                    //printf("Server does not contain client entry...\n");

                    char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                    char* update_message = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
                    strcpy(update_path, project_name);
                    strcat(update_path, "/.Update");

                    strcpy(update_message, "D:");
                    strcat(update_message, lpath);
                                
                    int update_fd = open(update_path, O_RDWR | O_APPEND);
                    if(update_fd == -1 && errno == 2){
                        perror(".Update");
                        creat(update_path, S_IRUSR | S_IWUSR | S_IXUSR);
                        update_fd = open(update_path, O_RDWR | O_APPEND);
                                        
                    }else if(update_fd == -1){
                     perror(".Update");
                    }
                    //write(update_fd, "M:", 2);
                    //write(update_fd, cli_path, MESSAGE_LENGTH);
                    write(update_fd, update_message, strlen(update_message));
                    write(update_fd, ":", 1);
                    write(update_fd, lhash, strlen(lhash));
                    write(update_fd, "\n", 1);

                    printf("%s\n", update_message); //OUTPUT MODIFY MESSAGE
                    free(update_path);
                    free(update_message);
                    close(update_fd);
                }

            }

            memset(ln, 0, 256);
        }
        memset(rb, 0, 1);
    }



    //printf("To Server: end_list\n" );
    send(socket_fd, "end_list", 8, 0);

    //WAIT FOR SERVER TO FINISH
    //recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    
    while(1){
        //printf("SErver: %s\n", receive_buff);
        if(strcmp(receive_buff, "update_complete") == 0){
            printf("Project Update Completed...\n");
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    }

    free(serv_mani_string);
    free(mani_path);
    free(cli_hash);
    free(cli_path);
    free(cli_version);
    free(cli_mani_version);
    free(serv_mani_version);
    //free(serv_path);
    free(serv_hash);
    free(serv_path);
    free(live_hash);
    free(live_hash_string);
    free(receive_buff);
    free(no_project);
    
    return;
}

void strtok_to_array(char* original, char* str_arr[128]){
    
    int index = 0;
    char* token = strtok(original, ":");
    strcpy(str_arr[index], token);
    while(token != NULL){
        str_arr[index++] = token;
        token = strtok(NULL, ":");
    }

    //return str_arr;
}

void string_array_get_element(char* string_arr[], int index, char* output){
    //printf("TRANSFERRING STRING ARRAY DATA\n");
    int i;
    for(i = 0; i < strlen(string_arr[index]); i++){
        //printf("%c", string_arr[index][i]);
        output[i] = string_arr[index][i];
    }
}

void upgrade_project(char* project_name){

    //FAILS:
    //IF PROJECT DOESN'T EXIST ON SERVER
    //IF NO CONTACT WITH SERVER
    //NO .Update ON CLIENT
    //.Conflict FILE EXISTS ON CLIENT

    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* success_message = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(success_message, 0, MESSAGE_LENGTH);
    strcpy(success_message, "upgrade_complete");

    char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(no_project, 0, MESSAGE_LENGTH);
    strcpy(no_project, "project_dne");

    char* conflict_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));

    strcpy(conflict_path, project_name);
    strcat(conflict_path, "/.Conflict");

    strcpy(update_path, project_name);
    strcat(update_path, "/.Update");

    int con_fd = open(conflict_path, O_RDONLY);
    int update_fd = open(update_path, O_RDONLY);

    //CHECK IF THERES A CONFLICT FILE OR UPDATE FILE DOES NOT EXIST
    if(con_fd != -1){
        printf(".Conflict file exists...\nExiting...\n");
        send(socket_fd, "STOP*", 5, 0);
        return;
    }else if(update_fd == -1){
        perror(".Update error");
        printf("Exiting...\n");
        send(socket_fd, "STOP*", 5, 0);
        return;
    }

    send(socket_fd, "CONT*", 5, 0);

    //CHECK IF PROJECT EXISTS ON SERVER
    send(socket_fd, project_name, strlen(project_name), 0);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("From server: %s\n", receive_buff);
    if(strcmp(receive_buff, no_project) == 0){
        printf("Project does not exit...\n");
        free(receive_buff);
        free(success_message);
        free(no_project);
        return;
    }

    //WAIT FOR SERVER COMMAND TO CONTINUE:
    //printf("Waiting for server GO...\n");
    while(1){
        if(strcmp(receive_buff, "GO*") == 0){
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }



    //SCAN UPDATE FILE LINE-BY-LINE AND MAKE CHANGES LISTED
    char* line = (char*) malloc(256 * sizeof(char));
    memset(line, 0, 256);
    
    char* read_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(read_buff, 0, MESSAGE_LENGTH);
    int r;
    //APPEND BUFFER TO LINE UNTIL END OF LINE
    //send(socket_fd, "SEND*", 5, 0);
    //printf("Reading Lines...\n");
    while((r = read(update_fd, read_buff, 1))> 0){
        if(strcmp(read_buff, "\n") != 0){
            strcat(line, read_buff);
        }else{
            //printf("Line: %s\n", line);
            char* line_copy = (char*) malloc(256 * sizeof(char));
            strcpy(line_copy,line);
            //TOKENIZE LINE INTO ACTION::PATH
            char* action = strtok(line, ":");
            char* path_temp = strtok(NULL, ":");

            strtok(line_copy, ":");
            char* path = strtok(NULL, ":");

            //printf("Action: %s\nPath: %s\n", action, path);

            int index = 0;
            char* token = strtok(path_temp, "/");
            char* file_path_array[10];
            file_path_array[index] = token;
            while(token != NULL){
                //printf("SERVERTOKEN: %s\n", serv_token);
                //memset(token_array[index], 0, MESSAGE_LENGTH);
                token = strtok(NULL, "/");
                index++;
                file_path_array[index] = token;
            }
            
            index = 0;
            while(file_path_array[index] != NULL){
                index++;
            }
            char* filename = file_path_array[index-1];
            //printf("Path section at index %d: %s\n", index-1, file_path_array[index-1]);
            //printf("Filename: %s\n", filename);

            /* char* filename;
            filename = strtok(path, "/");
            printf("Filename: %s\n", filename);
            while(filename != NULL){
                filename = strtok(NULL, "/");
            }
            printf("Filename: %s\n", filename); */
            //send(socket_fd, "STOP*", 5, 0);
            //CHECK ACTION:
            if(strcmp(action, "D") == 0){ //DELETE ENTRY FROM MANIFEST
                printf("Deleting Entry...\n");
                manifest_remove_entry(project_name, filename);
            }
            if(strcmp(action, "M") == 0){//M: CREATE FILE
                printf("Overwriting File...\n");

            }
            if(strcmp(action, "A") == 0 || strcmp(action, "M") == 0){//M: CREATE FILE  A: OVERWRITE FILE
                send(socket_fd, path, MESSAGE_LENGTH, 0);
                int file_fd;
                if((file_fd = open(path, O_RDWR)) != -1){
                    printf("File exists... Removing in order to update\n");
                    close(file_fd);
                    remove(path);
                }
                //memset(receive_buff, 0, MESSAGE_LENGTH);
                //recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
                //printf("Server: %s\n", receive_buff);
                //printf("Making File...\n");
                //printf("To server: Requesting File\n");
                request_file(path);
                
                
            }

            //send(socket_fd, "STOP*", 5, 0);

            memset(line, 0, 256);
        }
        memset(read_buff, 0, MESSAGE_LENGTH);
    }

    

    send(socket_fd, "STOP*", 5, 0);

    //WAIT FOR SERVER CLOSE CONFIRMATION
    //recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    while(1){
        
        //printf("SERVER: %s\n", receive_buff);
        if(strcmp(receive_buff, success_message) == 0){
            printf("Upgrade Complete...\n");
            //send(socket_fd, success_message, MESSAGE_LENGTH, 0);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }
    //printf("SERVER: %s\n", receive_buff);
    printf("Project is UP TO DATE\n");

    close(update_fd);
    remove(update_path);

    free(line);
    free(read_buff);
    free(success_message);
    free(no_project);
    free(receive_buff);
    free(conflict_path);
    free(update_path);
}

void commit_project(char* project_name){
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* success_message = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(success_message, 0, MESSAGE_LENGTH);
    strcpy(success_message, "commit_complete");

    char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(no_project, 0, MESSAGE_LENGTH);
    strcpy(no_project, "project_dne");

    char* update_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(update_path, project_name);
    strcat(update_path, "/.Update");
    int update_fd = open(update_path, O_RDONLY);

    char* conflict_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(conflict_path, project_name);
    strcat(conflict_path, "/.Conflict");
    int conflict_fd = open(conflict_path, O_RDONLY);

    if(conflict_fd != -1){
        printf(".Conflict exists! Cannot continue...\n");
        send(socket_fd, "STOP*", 5, 0);

        free(update_path);
        free(conflict_path);
        close(conflict_fd);
        close(update_fd);
        free(receive_buff);
        free(success_message);
        free(no_project);
        return;
    }

    if(update_fd != -1){
        printf(".Update exists. Checking if it's empty...\n");
        char* read_buf = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
        int r;
        int bytes = 0;
        while((r = read(update_fd, read_buf, 1)) > 0){
            bytes+=r;
        }
        if(bytes > 0){
            printf(".Update is not empty!\nExiting...\n");

            send(socket_fd, "STOP*", 5, 0);

            free(update_path);
            free(conflict_path);
            close(update_fd);
            close(conflict_fd);
            free(receive_buff);
            free(success_message);
            free(no_project);
            free(read_buf);
            return;
        }
        
        printf(".Update is empty. Free to continue...\n");
    }
    send(socket_fd, "CONT*", 5, 0);

    //CHECK IF PROJECT EXISTS ON SERVER
    send(socket_fd, project_name, strlen(project_name), 0);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("From server: %s\n", receive_buff);
    if(strcmp(receive_buff, no_project) == 0 || strcmp(receive_buff, "no_manifest") == 0){
        printf("Project or .Manifest does not exit...\n");

        free(update_path);
        free(conflict_path);
        close(update_fd);
        close(conflict_fd);
        free(receive_buff);
        free(success_message);
        free(no_project);
        
        return;
    }

    //recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);

    //GET SERVER MANIFEST INFO
    char mani_size[MESSAGE_LENGTH];
    memset(mani_size, 0, MESSAGE_LENGTH);
    recv(socket_fd, mani_size, MESSAGE_LENGTH, 0);
    //printf("Server manifest size: %s\n", mani_size);
    int bytes = atoi(mani_size);
    char* serv_mani_contents = (char*) malloc(bytes);
    recv(socket_fd, serv_mani_contents, bytes, 0);
    //printf("Server Manifest Contents:\n%s\n", serv_mani_contents);

    //OPEN OR CREATE .Commit:
    char* commit_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(commit_path, project_name);
    strcat(commit_path, "/.Commit");
    int commit_fd = open(commit_path, O_RDWR | O_APPEND);
    if(commit_fd != -1){
        printf("Commit exists.\n");
        close(commit_fd);
        remove(commit_path);
        commit_fd = open(commit_path, O_RDWR);
    }
    if(commit_fd == -1 && errno == 2){
        printf("Creating .Commit file...\n");
        creat(commit_path, S_IRUSR | S_IWUSR | S_IXUSR);
        commit_fd = open(commit_path, O_RDWR | O_APPEND);
    }

    //COMPARE MANIFEST VERSION: IF DIFFERENT, QUIT:
    char* mani_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(mani_path, project_name);
    strcat(mani_path, "/.Manifest");
    int mani_fd = open(mani_path, O_RDONLY);

    char* line = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(line, 0, MESSAGE_LENGTH);
    int r;
    //int bytes;
    char* read_buff = (char*) malloc(MESSAGE_LENGTH*sizeof(char));
    //printf("Client manifest:\n");

    //MODIFY: CLIENT AND SERVER HASH FOR A FILE IS THE SAME
        //--> CLIENTS LIVE-HASH IS DIFFERENT FROM STORED
            //Append 'M <file/path> <server's hash>' to .Commit with the file version incremented
    //ADD: SERVER MANIFEST DOES NOT CONTAIN FILE THAT CLIENT HAS
        //--> Append 'A <file/path> <server's hash>' to .Commit with the file version incremented
    //DELETE: CLIENT DOES NOT HAVE ENTRY THAT SERVER HAS
        //--> Append 'D <file/path> <server's hash>' to .Commit with the file version incremented

    //Get number of server manifest lines:
    
    int server_lines = 0;
    int i;
    for(i = 0; i < strlen(serv_mani_contents); i++){
        if(serv_mani_contents[i] == '\n'){
            server_lines++;
        }
    }

    //Get number of client manifest lines:
    int client_lines = 0;
    i = 0;
    while((r = read(mani_fd, read_buff, 1))>0){
        if(strcmp(read_buff, "\n") == 0){
            client_lines++;
        }
    }

    //ARRAY OF SERVER LINES:
    char* serv_mani_contents_copy = (char*) malloc(bytes);
    strcpy(serv_mani_contents_copy, serv_mani_contents);
    char* serv_array[server_lines];
    int index = 0;
            char* serv_token = strtok(serv_mani_contents_copy, "\n");
            char* token_array[4];
            serv_array[index] = serv_token;
            while(serv_token != NULL){
                //printf("TOKEN: %s\n", serv_token);
                //memset(token_array[index], 0, MESSAGE_LENGTH);
                serv_token = strtok(NULL, "\n");
                index++;
                serv_array[index] = serv_token;
            }

    
    //printf("serv_array[0]: %s\n", serv_array[0]);
    //printf("serv_array[1]: %s\n", serv_array[1]);

    close(mani_fd);
    //printf("Server manifest lines: %d\n", server_lines);
    //printf("Client manifest lines: %d\n", client_lines);

    //Array of clients entries:
    char* cli_line_array[client_lines];


    mani_fd = open(mani_path, O_RDONLY);
    int line_number = 1;
    int has_file = 0;
    int break_loop = 0;
    while((r = read(mani_fd, read_buff, 1))>0){

        if(break_loop == 1){
            break;
        }

        if(strcmp(read_buff, "\n") != 0){
            strcat(line, read_buff);
        }else{
            has_file =0;
            char line_copy[MESSAGE_LENGTH];
            strcpy(line_copy, line);
            //printf("Line: %s\n", line_copy);
            if(strncmp(line_copy, "version:", 8) ==0 && strstr(serv_mani_contents, line_copy) == NULL){
                printf("Client and Server Versions don't match...\nPlease make sure these match before next Commit...\n");
                break;
            }

            //cli_line_array[line_number-1] = line_copy;

            //TOKENIZE ENTRY:
            index = 0;
            char* token = strtok(line_copy, ":");
            char* token_array[4];
            token_array[index] = token;
            while(token != NULL){
                //printf("TOKEN: %s\n", token);
                //memset(token_array[index], 0, MESSAGE_LENGTH);
                token = strtok(NULL, ":");
                index++;
                token_array[index] = token;
            }

            //ADD:
            if(strstr(serv_mani_contents, token_array[1]) == NULL && line_number > 1){
                //printf("SERVER DOES NOT CONTAIN %s **\n", token_array[1]);
                char add_message[MESSAGE_LENGTH];
                int new_version = atoi(token_array[0]);
                new_version++;
                char vers[10];
                sprintf(vers, "%d", new_version);
                strcpy(add_message, "A:");
                strcat(add_message, vers);
                strcat(add_message, ":");
                strcat(add_message, token_array[1]);
                strcat(add_message, ":");
                strcat(add_message, token_array[2]);
                printf("%s\n", add_message);
                write(commit_fd, add_message, strlen(add_message));
                write(commit_fd, "\n", 1);
            }
            
            //printf("Line: %d\nClient Lines: %d\n", line_number, client_lines);
            if(client_lines < server_lines){
                //has_file = 1;
                //printf("Client is missing a file***\n");
            }


            //MODIFY
            if(strstr(serv_mani_contents, token_array[1]) != NULL && line_number > 1){
                //printf("Client and Server have the same file: %s\n", token_array[1]);
               // printf("zzz: %s\n", serv_array[1]);
                int n;
                for(n = 1; n < server_lines; n++){
                    //FIND LINE WITH SAME PATH
                    //printf("zzz: %s\n", serv_array[n]);
                    if(serv_array[n] != NULL && strstr(serv_array[n], token_array[1]) != NULL){
                        //printf("Server line match...\n");
                        char serv_line_copy[MESSAGE_LENGTH];
                        strcpy(serv_line_copy, serv_array[n]);
                        //printf("Serv_line_copy: %s\n", serv_line_copy);

                        //TOKENIZE:
                        char* sr_ln_ver = strtok(serv_line_copy, ":");
                        char* sr_ln_path = strtok(NULL, ":");
                        char* sr_ln_hash = strtok(NULL, ":");
                        int sr_ln_vr = atoi(sr_ln_ver);
                        int cli_ln_vr = atoi(token_array[0]);

                        //COMPARE CLIENT AND SERVER HASH
                        //IF THEY MATCH, THEN COMPARE LIVE HASH TO STORED CLIENT HASH
                        //printf("Client: %s\n", token_array[2]);
                        //printf("Server: %s\n", sr_ln_hash);
                        if(strcmp(token_array[2], sr_ln_hash) == 0){
                            //printf("HASHES match\n");
                            unsigned char live_hash[32];
                            char live_hash_string[65];
                            memset(live_hash, 0, SHA256_DIGEST_LENGTH);
                            memset(live_hash_string, 0, 65);
                            file_create_hash(token_array[1], live_hash);
                            hash_to_string(live_hash, live_hash_string);
                            //printf("Live hash: %s\n", live_hash_string);
                            if(strcmp(token_array[2], live_hash_string) != 0){
                                //printf("LIVE HASH is different from stored...\n");
                                char modify_message[MESSAGE_LENGTH];
                                int new_version = atoi(token_array[0]);
                                new_version++;
                                char vers[10];
                                sprintf(vers, "%d", new_version);
                                strcpy(modify_message, "M:");
                                strcat(modify_message, vers);
                                strcat(modify_message, ":");
                                strcat(modify_message, token_array[1]);
                                strcat(modify_message, ":");
                                strcat(modify_message, sr_ln_hash);
                                printf("%s\n", modify_message);
                                write(commit_fd, modify_message, strlen(modify_message));
                                write(commit_fd, "\n", 1);
                                
                            }
                            //REMOVE LINE FROM LIST
                            serv_array[n] = NULL;
                        }else if(strcmp(token_array[2], sr_ln_hash) != 0 && sr_ln_vr >= cli_ln_vr){//IF HASHES DONT MATCH AND SERVER'S VERSION IS GREATER THAN CLIENT'S
                            //FAILURE
                            //printf("Cli ver: %d\nServ ver: %d\nVersions are incompatable...\n", cli_ln_vr, sr_ln_vr);
                            //printf("Client must sync with repository before further Commit.\n");
                            //printf("Removing .Commit file\n");

                            close(commit_fd);
                            remove(commit_path);

                            break_loop = 1;
                            break;
                    
                        }
                        
                    }
                    
                }
            }
            
            memset(line, 0, MESSAGE_LENGTH);
            line_number++;
            //printf("Has file: %d\n", has_file);

        }
        
        memset(read_buff, 0, MESSAGE_LENGTH);
    }

    //DELETE
        //SCAN THROUGH REMAINING FILES AND ADD THEM TO COMMIT
    i = 0;
    //serv_array[1] = NULL;
    for(i = 1; i < server_lines; i++){
        //printf("-- %s\n", serv_array[i]);
        if(serv_array[i] != NULL){

            //TOKENIZE:
            char* sr_ln_ver = strtok(serv_array[i], ":");
            char* sr_ln_path = strtok(NULL, ":");
            char* sr_ln_hash = strtok(NULL, ":");

            char delete_message[MESSAGE_LENGTH];
            int new_version = atoi(sr_ln_ver);
            new_version++;
            char vers[10];
            sprintf(vers, "%d", new_version);
            strcpy(delete_message, "D:");
            strcat(delete_message, vers);
            strcat(delete_message, ":");
            strcat(delete_message, sr_ln_path);
            strcat(delete_message, ":");
            strcat(delete_message, sr_ln_hash);
            printf("%s\n", delete_message);
            write(commit_fd, delete_message, strlen(delete_message));
            write(commit_fd, "\n", 1);
        }
    }

    
    close(commit_fd);
    commit_fd = open(commit_path, O_RDONLY);
    //SEND .Commit to the server
    send(socket_fd, commit_path, strlen(commit_path), 0);
    //send(socket_fd, ".COMM", 5, 0);
    //send_file_data(commit_path);
    char* read_buf[MESSAGE_LENGTH];
    memset(read_buf, 0, MESSAGE_LENGTH);
    int file_bytes = 0;
    int res;
    while((res = read(commit_fd, read_buf, 1)) > 0){
        //printf("%s",read_buf);
        file_bytes+=res;
        //write(sfd, read_buf, res);
    }

    close(commit_fd);
    commit_fd = open(commit_path, O_RDONLY);
    if(commit_fd == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    //printf("\nBytes in the file: %d\n", file_bytes);
    char* file_contents = (char*) malloc(file_bytes);
    read(commit_fd, file_contents, file_bytes);
    
    if(file_bytes == 0){
        strcpy(file_contents, "");
    }
    //printf("File Contents:\n%s\n", file_contents);
    char send_amount[128];
    sprintf(send_amount, "%d", file_bytes);
    //printf("%s\n", send_amount);

    send(socket_fd, send_amount, 128,0);
    
    //printf("Sending...\n");
    send(socket_fd, file_contents, file_bytes, 0);


    //WAIT FOR SERVER CLOSE CONFIRMATION
    //recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    while(1){
        
        //printf("SERVER: %s\n", receive_buff);
        if(strcmp(receive_buff, success_message) == 0){
            printf("Commit Complete...\n");
            //send(socket_fd, success_message, MESSAGE_LENGTH, 0);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }

    free(commit_path);
    close(commit_fd);
    free(update_path);
    free(conflict_path);
    close(update_fd);
    close(conflict_fd);
    free(receive_buff);
    free(success_message);
    free(no_project);
}

void push_project(char* project_name){
    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    char* success_message = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(success_message, 0, MESSAGE_LENGTH);
    strcpy(success_message, "push_complete");

    char* no_project = (char*) malloc(MESSAGE_LENGTH* sizeof(char));
    memset(no_project, 0, MESSAGE_LENGTH);
    strcpy(no_project, "project_dne");

    char* commit_path = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    strcpy(commit_path, project_name);
    strcat(commit_path, "/.Commit");
    int commit_fd = open(commit_path, O_RDONLY);

    if(commit_fd == -1 && errno == 2){
        printf(".Commit does not exist, Cannot continue...\n");
        send(socket_fd, "STOP*", 5, 0);

        free(commit_path);
        //free(conflict_path);
        //close(conflict_fd);
        close(commit_fd);
        free(receive_buff);
        free(success_message);
        free(no_project);
        return;
    }
    send(socket_fd, "CONT*", 5, 0);

    //CHECK IF PROJECT EXISTS ON SERVER
    send(socket_fd, project_name, strlen(project_name), 0);
    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("From server: %s\n", receive_buff);
    if(strcmp(receive_buff, no_project) == 0 || strcmp(receive_buff, "no_manifest") == 0){
        printf("Project or .Manifest does not exit...\n");

        free(commit_path);
        //free(update_path);
        //free(conflict_path);
        //close(update_fd);
        close(commit_fd);
        free(receive_buff);
        free(success_message);
        free(no_project);
        
        return;
    }

    //SEND SERVER .Commit
    //send(socket_fd, commit_path, strlen(commit_path), 0);
    char* read_buf[MESSAGE_LENGTH];
    memset(read_buf, 0, MESSAGE_LENGTH);
    int file_bytes = 0;
    int res;
    while((res = read(commit_fd, read_buf, 1)) > 0){
        //printf("%s",read_buf);
        file_bytes+=res;
        //write(sfd, read_buf, res);
    }

    close(commit_fd);
    commit_fd = open(commit_path, O_RDONLY);
    if(commit_fd == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    //printf("\nBytes in the file: %d\n", file_bytes);
    char* file_contents = (char*) malloc(file_bytes);
    read(commit_fd, file_contents, file_bytes);
    //printf("File Contents:\n%s\n", file_contents);
    char send_amount[128];
    sprintf(send_amount, "%d", file_bytes);
    //printf("send amout: %s\n", send_amount);

    send(socket_fd, send_amount, 128,0);
    
    //printf("Sending...\n");
    send(socket_fd, file_contents, file_bytes, 0);

    //printf("\n");
    send_project_rec(project_name, project_name, 1);
    //printf("\n");
    send(socket_fd, "end_list", 8,0);

    //WAIT FOR SERVER CLOSE CONFIRMATION
    //recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    while(1){
        
        //printf("SERVER: %s\n", receive_buff);
        if(strcmp(receive_buff, success_message) == 0){
            printf("Push Complete...\n");
            //send(socket_fd, success_message, MESSAGE_LENGTH, 0);
            break;
        }
        memset(receive_buff, 0, MESSAGE_LENGTH);
        recv(socket_fd, receive_buff, MESSAGE_LENGTH,0);
    }

    close(commit_fd);
    remove(commit_path);

}

void send_project_rec(char* project_path, char* master_path, int is_copy){
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

    if(is_copy == 1){
        //printf("is_copy\n");
        strcat(dir_message, "Copy_");
        strcat(file_message, "Copy_");
    }


    while((dp = readdir(d)) != NULL){
        if(strcmp(dp->d_name,"..") != 0 && strcmp(dp->d_name,".") != 0){
            
            strcpy(path,project_path);
            strcat(path,"/");
            strcat(path, dp->d_name);
            //printf("%s\n", path);

            if(isDirectory(path)){
                strcat(dir_message, path);
                //printf("%s\n", dir_message);
                send(socket_fd, dir_message, PATH_LENGTH,0);
                strcpy(dir_message, "directory:");
                
            }else if(isReg(path)){
                strcat(file_message, path);
                //printf("%s\n", file_message);
                send(socket_fd, file_message, PATH_LENGTH,0);
                strcpy(file_message, "file:"); 
                if(is_copy == 1){
                    strcat(file_message, "Copy_");
                }

                //printf("send: %s\n", path);
                send_f_data(path);
                //send_file_data(path);
            }

            

            send_project_rec(path, master_path, is_copy);
        }
    }
}

int isDirectory(char* path){
    struct stat statbuf;
    stat(path, &statbuf);
    return S_ISDIR(statbuf.st_mode);
}

void send_f_data(char* filepath){

    char* receive_buff = (char*) malloc(MESSAGE_LENGTH * sizeof(char));
    memset(receive_buff, 0, MESSAGE_LENGTH);

    recv(socket_fd, receive_buff, MESSAGE_LENGTH, 0);
    //printf("serv: %s\n", receive_buff);

    //send(socket_fd, "<some size>", PATH_LENGTH, 0);
    //send(socket_fd, "<some size>", 12, 0);

    int file_fd = open(filepath, O_RDONLY);

    char* read_buf[MESSAGE_LENGTH];
    memset(read_buf, 0, MESSAGE_LENGTH);
    int file_bytes = 0;
    int res;
    while((res = read(file_fd, read_buf, 1)) > 0){
        //printf("%s",read_buf);
        file_bytes+=res;
        //write(sfd, read_buf, res);
    }

    close(file_fd);
    file_fd = open(filepath, O_RDONLY);
    if(file_fd == -1){
        printf("Error Number %d\n", errno);  
        perror("Program"); 
    }

    //printf("\nBytes in the file: %d\n", file_bytes);
    char* file_contents = (char*) malloc(file_bytes);
    read(file_fd, file_contents, file_bytes);
    if(file_bytes == 0){
        file_contents = (char*) malloc(1);
        file_contents = "";
    }
    //printf("File Contents:\n%s\n", file_contents);
    char send_amount[128];
    sprintf(send_amount, "%d", file_bytes);
    //printf("send amout: %s\n", send_amount);

    send(socket_fd, send_amount, 128,0);
    
    //printf("Sending...\n");
    send(socket_fd, file_contents, file_bytes, 0);
}