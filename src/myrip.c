/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 4
 * Usage: ./myproxy <local_port>
 */

#include "myunp.h"

#define MAX_LISTEN_QUEUE 16
#define MAX_URL_LENGTH 253
#define MAX_HEADER_SIZE 32768
#define BUFFER_SIZE 2048
#define LOGFILE "myproxy.log"

typedef struct {
    int clisock;
    int remotesock;
    int closed;
} tInfo_t;

int servsock, remote_port = 0;
char **forbidden_sites = NULL;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void read_forbidden_list(char *forbidden_file);
int create_socket(int port);
int connect_remote(int clisock, struct sockaddr_in cliaddr);
void *client_to_remote(void *threadInfo);
void *remote_to_client(void *threadInfo);
int allowed_url(char * url);
void send_reply(int sock, int code);
void write_log(char *request, struct sockaddr_in cliaddr, char *remoteaddr, char *message);


int main(int argc, char **argv)
{
    struct sockaddr_in cliaddr;
    int local_port, cliaddr_len = sizeof(cliaddr);
    
    if ((argc < 2)|| (argc > 3) || ((local_port = strtoul(argv[1], NULL, 10)) < 0)) {
        printf("Usage: %s <local_port> [-f forbidden_sites.text]\n\n", argv[0]);
        exit(1);
    }
    if (argc == 3) {
        read_forbidden_list(argv[2]);
    } 
        
    
    servsock = create_socket(local_port);
    
    for (;;) {
        pthread_t tid;
        tInfo_t *tInfo;
        
        if ((tInfo = calloc(1, sizeof(tInfo_t))) == NULL) {
            err_sys("main(): ERROR allocating memory!\n\n");
        }
        tInfo->clisock = Accept(servsock, (SA *) &cliaddr, &cliaddr_len);
        printf("main(): Connection from %s:%u\n", 
            inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
        
        //check packet for validity and try to connect to the remote host
        if((tInfo->remotesock = connect_remote(tInfo->clisock, cliaddr)) > 0) {
            
            //spawn client-to-remote
            int rc = pthread_create(&tid, NULL, client_to_remote, (void *) tInfo);
            if (rc != 0) {
                printf("pthread_create() ERROR: %s\n\n", strerror(rc));
                exit(2);
            }
            if ((rc = pthread_detach(tid)) != 0) {
                printf("pthread_detatch(%d) ERROR: %s\n\n", tid, strerror(rc));
                exit(3);
            }
            
            //spawn remote-to-client
            rc = pthread_create(&tid, NULL, remote_to_client, (void *) tInfo);
            if (rc != 0) {
                printf("pthread_create() ERROR: %s\n\n", strerror(rc));
                exit(4);
            }
            if ((rc = pthread_detach(tid)) != 0) {
                printf("pthread_detatch(%d) ERROR: %s\n\n", tid, strerror(rc));
                exit(5);
            }
        } else if (tInfo->remotesock == -1) {
            send_reply(tInfo->clisock, 501);
            close(tInfo->clisock);
            free(tInfo);
        } else if (tInfo->remotesock == -2) {
            send_reply(tInfo->clisock, 403);
            close(tInfo->clisock);
            free(tInfo);
        } else {
            send_reply(tInfo->clisock, 500);
            close(tInfo->clisock);
            free(tInfo);
        }
    }
    
    return 0;
}

//Read server IPs/ports into string array
//Modified from https://stackoverflow.com/a/19174415
void read_forbidden_list(char *forbidden_file)
{
    int num_alloced = 4;
    FILE *fp = fopen(forbidden_file, "r");
    
    if (!fp) {
        printf("read_forbidden_list(): Unable to open %s for reading (%s)...\n", 
                forbidden_file, strerror(errno));
        forbidden_sites = NULL;
        return;
    }
    
    if ((forbidden_sites = calloc(num_alloced + 1, sizeof(char *))) == NULL) {
        err_sys("read_forbidden_list(): ERROR allocating memory!\n\n");
    }
    
    int i;
    for (i = 0; 1; i++) {
        
        if (i >= num_alloced) {//We need more space!
            num_alloced *= 2;
            //Get us more space
            if ((forbidden_sites = realloc(forbidden_sites, sizeof(char*) * num_alloced + 1)) == NULL) {
                err_sys("read_forbidden_list(): ERROR allocating memory!\n\n");
            }
            
            //Why isn't there a Recalloc?
            int j;
            for (j = num_alloced/2; j < num_alloced + 1; j++) {
                forbidden_sites[j] = NULL;
            }
        }
        
        //Get next line in the filelength
        if ((forbidden_sites[i] = calloc(MAX_URL_LENGTH + 2, sizeof(char))) == NULL) {
            err_sys("read_forbidden_list(): ERROR allocating memory!\n\n");
        }
        if ((fgets(forbidden_sites[i], MAX_URL_LENGTH + 2, fp)) == NULL) {
            //if it's an empty string (only a newline when read in), free and set to NULL
            free(forbidden_sites[i]);
            forbidden_sites[i] = NULL;
            break;
        }
        
        //strip CR/LF characters and insert \0
        int j;
        for (j = strlen(forbidden_sites[i])-1; 
             j <= 0 && (forbidden_sites[i][j] == '\n' || forbidden_sites[i][j] == '\r');
             j--)
             ;
        forbidden_sites[i][j] = '\0';
    }
    fclose(fp);
    
    // int tmp = 0;
    // while (forbidden_sites[tmp] != NULL) {
        // printf("%d: \"%s\"\n", strlen(forbidden_sites[tmp]), forbidden_sites[tmp]);
        // tmp++;
    // }
    // printf("%d\n\n", tmp);
}

//Create socket for the server end of the proxy
int create_socket(int local_port)
{
    int _servsock, optval = 0;
    struct sockaddr_in servaddr;
    
    _servsock = Socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(_servsock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        err_sys("create_socket(): setsockopt() error: %s", strerror(errno));
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(local_port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    
    Bind(_servsock, (SA *) &servaddr, sizeof(servaddr));
    Listen(_servsock, MAX_LISTEN_QUEUE);
    
    return _servsock;
}

int connect_remote(int clisock, struct sockaddr_in cliaddr)
{
    struct sockaddr_in remote_addr;
    struct hostent *remote_host;
    int n, 
        remote_sock = 0, 
        remote_port = 0;
    char buffer[MAX_HEADER_SIZE],
         request[10],
         url[MAX_URL_LENGTH],
         host[MAX_URL_LENGTH],
         version[10],
         *str_port,
         logbuff[20],
         logurl[MAX_URL_LENGTH];
         
    if ((n = recv(clisock, buffer, MAX_HEADER_SIZE, MSG_PEEK)) > 0) {
        sscanf(buffer,"%s %s %s",request, url, version);
        
        //save this from strtok
        strcpy(logurl, url);
        
        //We only support GET and HEAD on HTTP 1.0 and 1.1
        if(((strncmp(request, "GET", 3) == 0) || (strncmp(request, "HEAD", 4) == 0))
          &&((strncmp(version, "HTTP/1.1", 8) == 0) || (strncmp(version, "HTTP/1.0", 8) == 0))
          &&(strncasecmp(url, "http://", 7) == 0)) {
              
            printf("=======%s=========\n", logurl);
            
            remote_sock = Socket(AF_INET, SOCK_STREAM, 0);
            
            //If there's a : before the first / then there's a specific port being requested
            if (strstr(url + 7, ":") != NULL 
              && (strstr(url + 7, ":") < strstr(url + 7, "/"))) {
                  
                  printf("==== if ===%s=========\n", logurl);
                  
                //given "http://www.google.com:80/whatever"
                //this will give us "HTTP"
                str_port = strtok(url, ":");
                //this gives us "//www.google.com"
                str_port = strtok(NULL, ":");
                
                if(!allowed_url(str_port + 2)) {
                    
                    printf("==== ! ===%s=========\n", logurl);
                    
                    sprintf(logbuff, "%s %s", request, version);
                    write_log(logbuff, cliaddr, logurl, "403: Forbidden");
                    
                    return -2;
                }
                
                printf("  connect_remote(): if host=\"%s\"\n", str_port + 2);
                if ((remote_host = gethostbyname(str_port + 2)) == NULL) {
                    printf("  connect_remote(): gethostbyname() ERROR\n");
                    close(remote_sock);
                    
                    sprintf(logbuff, "%s %s", request, version);
                    write_log(logbuff, cliaddr, logurl, "500: gethostbyname() error");
                    
                    return -1;
                }
                
                //this will give us "80/whatever"
                str_port = strtok(NULL, ":");
                
                remote_port = strtoul(str_port, NULL, 10);
            } else {
                
                printf("=== else ====%s=========\n", logurl);
                
                //this gives us "HTTP"
                str_port = strtok(url, "//");
                //this gives us "www.google.com"
                str_port = strtok(NULL, "/");
                
                if(!allowed_url(str_port)) {
                    
                    printf("=== ! ====%s=========\n", logurl);
                    
                    sprintf(logbuff, "%s %s", request, version);
                    write_log(logbuff, cliaddr, logurl, "403: Forbidden");
                    
                    return -2;
                }
                
                printf("  connect_remote(): else host=\"%s\"\n", str_port);
                if ((remote_host = gethostbyname(str_port)) == NULL) {
                    printf("  connect_remote(): gethostbyname() ERROR\n");
                    close(remote_sock);
                    
                    sprintf(logbuff, "%s %s", request, version);
                    write_log(logbuff, cliaddr, logurl, "500: gethostbyname() error");
                    
                    return -1;
                }
                remote_port = 80;
            }
            
            bzero(&remote_addr, sizeof(remote_addr));
            remote_addr.sin_family = AF_INET;
            memcpy(&remote_addr.sin_addr.s_addr, remote_host->h_addr, remote_host->h_length);
            remote_addr.sin_port = htons(remote_port);
            
            
            printf("  connect_remote(): Connecting to %s:%u\n", 
            inet_ntoa(remote_addr.sin_addr), remote_addr.sin_port);
            if ((connect(remote_sock, (SA *) &remote_addr, sizeof(remote_addr))) < 0) {
                printf("  connect_remote(): connect() ERROR\n");
                close(remote_sock);
                
                sprintf(logbuff, "%s %s", request, version);
                write_log(logbuff, cliaddr, logurl, "500: gethostbyname() error");
                
                return -1;
            }
            
        } 
    }
    printf("  connect_remote(): Connected, returning...\n");
    
    sprintf(logbuff, "%s %s", request, version);
    write_log(logbuff, cliaddr, logurl, "connected...");
    
    return remote_sock;
}

void *client_to_remote(void *threadInfo)
{
    tInfo_t *tInfo = (tInfo_t *) threadInfo;
    int n;
    char buffer[MAX_HEADER_SIZE],
         request[10],
         url[MAX_URL_LENGTH],
         version[10],
         *host;
    
    while (
            (!(tInfo->closed)
            && (n = recv(tInfo->clisock, buffer, BUFFER_SIZE, 0)) > 0)
          ) {
        printf("client_to_remote(): recieved %d bytes: \n---------------\n%s---------------\n", 
                n, buffer);
        

        //check if the request is valid and get the host...
        sscanf(buffer,"%s %s %s",request, url, version);
        
        //We only support GET and HEAD on HTTP 1.0 and 1.1
        if(((strncmp(request, "GET", 3) == 0) || (strncmp(request, "HEAD", 4) == 0))
          &&((strncmp(version, "HTTP/1.1", 8) == 0) || (strncmp(version, "HTTP/1.0", 8) == 0))
          &&(strncasecmp(url, "http://", 7) == 0)) {
            
            //If there's a : before the first / then there's a specific port being requested
            if (strstr(url+7, ":") != NULL 
              && (strstr(url + 7, ":") < strstr(url + 7, "/"))) {
                //given "http://www.google.com:80/whatever"
                //this gives us "HTTP"
                host = strtok(url, "//");
                //this gives us "www.google.com"
                host = strtok(NULL, ":");
                
            } else {
                //given "http://www.google.com/whatever"
                //this gives us "HTTP"
                host = strtok(url, "//");
                //this gives us "www.google.com"
                host = strtok(NULL, "/");
            }
            
            //check the hostname against forbidden_sites
            if (allowed_url(host)) {
                if (!(tInfo->closed)) {
                    send(tInfo->remotesock, buffer, n, 0);
                }
            } else {
                send_reply(tInfo->clisock, 403);
                break;
            }
        } else {
            send_reply(tInfo->clisock, 501);
            printf("client_to_remote: 501\n\n");
            break;
        }
    }
    //if we break the while loop then the connection is closed/broken
    tInfo->closed = 1;
    close(tInfo->clisock);
    close(tInfo->remotesock);
    free(tInfo);
}
void *remote_to_client(void *threadInfo)
{
    tInfo_t *tInfo = (tInfo_t *) threadInfo;
    int n;
    char buffer[BUFFER_SIZE];
    
    while ((!(tInfo->closed)) && ((n = recv(tInfo->remotesock, buffer, BUFFER_SIZE, 0)) > 0)) {
        printf("remote_to_client(): recieved %d bytes: \n---------------\n%s---------------\n", 
                n, buffer);
        //need to check closed flag before and after the recv since it blocks
        if (!(tInfo->closed)) { 
            send(tInfo->clisock, buffer, n, 0);
        }
        
    }
    //if we break the while loop then the connection is closed/broken
    //set flag for client_to_remote to close everything
    tInfo->closed = 1;
}

int allowed_url(char * url) 
{
    int i;
    for (i = 0; forbidden_sites[i] != NULL; i++) {
        if ((strcasecmp(url, forbidden_sites[i])) == 0) {
            return 0;
        }
    }
    return 1;
}

void send_reply(int sock, int code) 
{
    char response[BUFFER_SIZE];
    
    switch (code) {
        case 403:
            strcpy(response, "HTTP/1.1 403 Forbidden\r\n\r\n");
            break;
        case 500:
            strcpy(response, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
            break;
        case 501:
            strcpy(response, "HTTP/1.1 501 Not Implemented\r\n\r\n");
            break;
    }
    printf("  send_reply(): sending %d\n\n", code);
    send(sock, response, strlen(response), 0);
}


void write_log(char *request, struct sockaddr_in cliaddr, char *remoteaddr, char *message)
{
    pthread_mutex_lock(&m);
    FILE *fp = fopen(LOGFILE, "a");
    time_t ltime;
    char timebuf[30];
    
    if (fp) {
        ltime = time(NULL);
        ctime_r(&ltime, timebuf);
        timebuf[strlen(timebuf) - 1] = '\0';
        
        fprintf(fp, "%s: ", timebuf);
        fprintf(fp, "%s:", inet_ntoa(cliaddr.sin_addr));
        fprintf(fp, "%u -> ", cliaddr.sin_port);
        fprintf(fp, "%s  -  ", remoteaddr);
        fprintf(fp, "%s\n", message);
    } else {
        printf("  write_log(): error opening logfile for writing...\n");
    }
    fclose(fp);
    pthread_mutex_unlock(&m);
}