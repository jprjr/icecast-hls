#include "socket.h"
#include "ich_time.h"
#include "strbuf.h"

#ifdef ICH_SOCKET_WINDOWS
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>

int ich_socket_nonblocking(SOCKET sock) {
#ifdef ICH_SOCKET_WINDOWS
    unsigned long mode = 1;
    int res = ioctlsocket(sock,FIONBIO,&mode);
    return (res == NO_ERROR) ? 0 : -1;
#else
    int flags = fcntl(sock, F_GETFL);
    return (flags == -1) ? -1 : fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int ich_socket_blocking(SOCKET sock) {
#ifdef ICH_SOCKET_WINDOWS
    unsigned long mode = 0;
    int res = ioctlsocket(sock,FIONBIO,&mode);
    return (res == NO_ERROR) ? 0 : -1;
#else
    int flags = fcntl(sock, F_GETFL);
    return (flags == -1) ? -1 : fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

int ich_socket_init(void) {
#ifdef ICH_SOCKET_WINDOWS
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2,2), &wsa_data) != 0;
#else
    return 0;
#endif
}

void ich_socket_cleanup(void) {
#ifdef ICH_SOCKET_WINDOWS
    WSACleanup();
#endif
}

void ich_socket_close(SOCKET sock) {
#ifdef ICH_SOCKET_WINDOWS
    closesocket(sock);
#else
    close(sock);
#endif
}

SOCKET ich_socket_connect(const char *host, const char *service) {
    int res = 0;
    SOCKET sock = INVALID_SOCKET;
    char ipstr[INET6_ADDRSTRLEN] = { 0 };
    void *addr = NULL;
    struct sockaddr_in *ipv4 = NULL;
    struct sockaddr_in6 *ipv6 = NULL;
    struct addrinfo hints = { 0 };
    struct addrinfo *servinfo = NULL;
    struct addrinfo *p = NULL;

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((res = getaddrinfo(host, service, &hints, &servinfo)) != 0) {
        fprintf(stderr,"getaddrinfo failed for %s:%s: %s\n",
          host, service, gai_strerror(res));
        goto cleanup;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if(p->ai_family == AF_INET) {
            ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(p->ai_family, addr, ipstr, INET6_ADDRSTRLEN);
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(sock == INVALID_SOCKET) continue;
        if(ich_socket_blocking(sock) != 0) {
            fprintf(stderr,"unable to set socket to blocking\n");
            ich_socket_close(sock);
            continue;
        }

        if(connect(sock, p->ai_addr, p->ai_addrlen) != 0) {
            fprintf(stderr,"connect to %s:%s failed: %s\n",
              ipstr, service, strerror(errno));
            ich_socket_close(sock);
            continue;
        }

        if(ich_socket_nonblocking(sock) != 0) {
            fprintf(stderr,"unable to set socket to blocking\n");
            ich_socket_close(sock);
            continue;
        }

        break; /* we have a valid socket and are connected */
    }

    cleanup:
    if(servinfo != NULL) freeaddrinfo(servinfo);
    return sock;
}

int ich_socket_recv(SOCKET sock, char *buf, unsigned int len, unsigned long timeout) {
    int events = 0;

#ifdef ICH_SOCKET_WINDOWS
    struct timeval tv = { 0 };
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
#else
    struct pollfd pfd = { 0 };
    pfd.fd = sock;
    pfd.events = POLLIN;
    pfd.revents = 0;
#endif

    do {
#ifdef ICH_SOCKET_WINDOWS
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        events = select(1,&fds, NULL, NULL, &tv);
#else
        events = poll(&pfd,1,timeout);
#endif
    } while(events == -1 && errno == EINTR);

    if(events == 0) {
        errno = ETIMEDOUT;
        events = -1;
    }

#ifdef ICH_SOCKET_WINDOWS
    if(!FD_ISSET(sock, &fds)) {
        events = -1;
    }
#else
    if(pfd.revents != POLLIN) {
        /* we have a HUP or something on the socket */
        events = -1;
    }
#endif

    if(events < 0) return events;

    return recv(sock,buf,len,0);
}


int ich_socket_send(SOCKET sock, const char *buf, unsigned int len, unsigned long timeout) {
    int events = 0;

#ifdef ICH_SOCKET_WINDOWS
    struct timeval tv = { 0 };
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
#else
    struct pollfd pfd = { 0 };
    pfd.fd = sock;
    pfd.events = POLLOUT;
    pfd.revents = 0;
#endif

    do {
#ifdef ICH_SOCKET_WINDOWS
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        events = select(1,NULL, &fds, NULL, &tv);
#else
        events = poll(&pfd,1,timeout);
#endif
    } while(events == -1 && errno == EINTR);

    if(events == 0) {
        errno = ETIMEDOUT;
        events = -1;
    }

#ifdef ICH_SOCKET_WINDOWS
    if(!FD_ISSET(sock, &fds)) {
        events = -1;
    }
#else
    if(pfd.revents != POLLOUT) {
        /* we have a HUP or something on the socket */
        events = -1;
    }
#endif

    if(events < 0) {
        return events;
    }

    return send(sock,buf,len,0);
}
