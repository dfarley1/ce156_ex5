/*
*   File:       myunp.c
*
*   Purpose:    Wrapper functions for Unix Network Programming assignments.
*/

#include "myunp.h"

void err_quit(const char *format, ...)
{
    va_list ap;
    char    buffer[200];
    int     prefix_length;

    va_start(ap, format);

    strcpy(buffer, "err_quit: ");
    prefix_length = strlen(buffer);

    vsnprintf(buffer + prefix_length,
              sizeof buffer - prefix_length,
              format,
              ap);

    va_end(ap);

    printf("%s\n", buffer);
    exit(1);
}

void err_sys(const char *s, ...)
{
    printf("err_sys: %s: %s\n", s, strerror(errno)); exit(1);
}

void bzero(void *p, size_t len) { memset(p, 0, len); }

int Socket(int family, int type, int protocol)
{
    int n;

    if ( (n = socket(family, type, protocol)) < 0)
    {
        err_sys("socket error");
    }

    return n;
}

int Bind(int sockfd, const struct sockaddr *myaddr, socklen_t addrlen)
{
    int n;

    if ( (n = bind(sockfd, myaddr, addrlen)) < 0)
    {
        err_sys("bind error: %s", strerror(errno));
    }

    return n;
}

void Listen(int sockfd, int backlog)
{
    char *ptr;

    if ( (ptr = getenv("LISTENQ")) != NULL)
    {
        backlog = atoi(ptr);
    }

    if (listen(sockfd, backlog) < 0)
    {
        err_sys("listen error");
    }
}

int Accept(int sockfd, struct sockaddr *cliaddr, socklen_t *addrlen)
{
    int n;

    if ( (n = accept(sockfd, cliaddr, addrlen)) < 0)
    {
        printf("  Accept() error: %s!  Ignoring connection.\n", strerror(errno));
    }

    return n;
}

int Close(int sockfd)
{
    int n;

    if ( (n = close(sockfd)) < 0)
    {
        printf("  Close() error: %s!  What do I do!?\n", strerror(errno));
    }

    return n;
}

int Read(int sockfd, char *buffer, int bufferlen)
{
    int n;

    if ( (n = read(sockfd, buffer, bufferlen)) < 0)
    {
        printf("  Read() error: %s!  Closing connection...\n", strerror(errno));
        Close(sockfd);
    }

    return n;
}

int Write(int sockfd, char *buffer, int bufferlen)
{
    int n;

    if ( (n = write(sockfd, buffer, bufferlen)) < 0)
    {
        printf("  Write() error: %s!  Closing connection...\n", strerror(errno));
        Close(sockfd);
    }

    return n;
}

int Sendto(int sockfd, const void *buf, size_t len, int flags, 
           const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int n;

    if ( (n = sendto(sockfd, buf, len, flags, dest_addr, addrlen)) < 0)
    {
        printf("  Sendto() error: %s!\n", strerror(errno));
    }

    return n;
}
