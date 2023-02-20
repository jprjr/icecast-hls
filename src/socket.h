#ifndef ICH_SOCKETH
#define ICH_SOCKETH

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
#define ICH_SOCKET_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#include <winsock2.h>
#else
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ich_socket_init(void);
void ich_socket_cleanup(void);
SOCKET ich_socket_connect(const char *host, const char *service);
void ich_socket_close(SOCKET);
int ich_socket_recv(SOCKET sock, char *buf, unsigned int len, unsigned long timeout);
int ich_socket_send(SOCKET sock, const char *buf, unsigned int len, unsigned long timeout);

#ifdef __cplusplus
}
#endif

#endif
