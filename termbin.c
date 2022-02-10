#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <wait.h>

int check_require(char require[], char file_name[])
{
    if (strncmp(require, "GET /", 5))
        return 0;
    int count;
    for (count = 5; require[count] != ' '; ++count) {}
    if (strncmp(require + count, " HTTP/1.1", 9))
        return 0;
    strncpy(file_name, require + 5, count - 5);  
    return 1;
}

int main(int argc, char **argv)
{
    pid_t pid1, pid2;
    if ((pid1 = fork()) == 0)
    {
        int listenfd;
        struct addrinfo hints, *ptr, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
        getaddrinfo(NULL, "80", &hints, &result);
        for (ptr = result; ptr; ptr = ptr->ai_next)
        {
            listenfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (listenfd == -1)
                continue;
            if (bind(listenfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
                break;
            close(listenfd);
        }
        freeaddrinfo(result);
        listen(listenfd, 1024);
        while(1)
        {  
            while (waitpid(-1, NULL, WNOHANG) > 0);
            int connfd = accept(listenfd, ptr->ai_addr, &ptr->ai_addrlen);
            if (connfd == -1)
                continue;
            pid_t pid;
            if ((pid = fork()) == 0)
            {
                char require[500], info[5000];
                memset(require, 0, sizeof(require));
                recv(connfd, require, 500, 0); 
                char file_name[50];
                if (!check_require(require, file_name))
                {                    
                    close(connfd);
                    return 0;
                }                    
                FILE *file_ptr = fopen(file_name, "rb");
                if (!file_ptr)
                {
                    close(connfd);
                    return 0;
                }
                fseek(file_ptr, 0, SEEK_END);
                size_t file_size = ftell(file_ptr);
                rewind(file_ptr);                
                char header[200];
                memset(header, 0, sizeof(header));
                sprintf(header, "HTTP/1.1 200 OK\nContent-Type:text/plain\nContent-Length: %llu\n\n", file_size);
                send(connfd, header, strlen(header), MSG_DONTWAIT);
                int read_size;
                do
                {
                    memset(info, 0, sizeof(info));
                    read_size = fread(info, 1, 5000, file_ptr);
                    if(read_size > 0)
                    {
                        send(connfd, info, read_size, 0);
                        file_size -= read_size;
                    }
                } while (file_size > 0);
                fclose(file_ptr);
                close(connfd);
                return 0;
            }            
            close(connfd);
        }
        return 0;
    }
    if ((pid2 = fork()) == 0)
    {
        int listenfd, timer = 0;
        struct addrinfo hints, *ptr, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
        getaddrinfo(NULL, "9999", &hints, &result);
        for (ptr = result; ptr; ptr = ptr->ai_next)
        {
            listenfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (listenfd == -1)
                continue;
            if (bind(listenfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
                break;
            close(listenfd);
        }
        freeaddrinfo(result);
        listen(listenfd, 1024);
        while (1)
        {
            while (waitpid(-1, NULL, WNOHANG) > 0);
            int connfd = accept(listenfd, ptr->ai_addr, &ptr->ai_addrlen);
            if (connfd == -1)
                continue;
            ++timer;
            timer %= 1000000;
            pid_t pid;
            if ((pid = fork()) == 0)
            {
                char file_name[50], info[5000];             
                sprintf(file_name, "%06ld", timer);
                char file_path[50] = "http://pb.honeta.site/";
                FILE *file_ptr = fopen(file_name, "wb");
                printf("Created: %s\n", file_name);
                send(connfd, strcat(file_path, strcat(file_name, "\n")), 50, MSG_DONTWAIT);                
                int recv_size;
                do
                {
                    memset(info, 0, sizeof(info));
                    recv_size = recv(connfd, info, 5000, 0);
                    if(recv_size > 0)
                        fwrite(info, 1, recv_size, file_ptr);
                } while (recv_size > 0);                               
                fclose(file_ptr);                
                close(connfd);
                return 0;
            }            
            close(connfd);
        }
        return 0;
    }
    int state;
    waitpid(pid1, &state, 0);
    waitpid(pid2, &state, 0);
    return 0;
}