/*
*   File:       myunp.h
*
*   Purpose:    Header for wrapper functions of myunp.c
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>

#define LISTENQ 1024
#define MAXLINE 4096

typedef struct sockaddr SA;

void err_quit(const char *format, ...);
void err_sys(const char *s, ...);
void bzero(void *p, size_t len);
int Socket(int family, int type, int protocol);
int Bind(int sockfd, const struct sockaddr *myaddr, socklen_t addrlen);
void Listen(int sockfd, int backlog);
int Accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen);
int Close(int sockfd);
int Read(int sockfd, char *buffer, int bufferlen);
int Write(int sockfd, char *buffer, int bufferlen);
int Sendto(int sockfd, const void *buf, size_t len, int flags, 
           const struct sockaddr *dest_addr, socklen_t addrlen);
