#include <sys/types.h>
#ifdef _WIN32
# include <winsock.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include "net.h"

#ifdef _WIN32
/* Windows */
#define WOULDBLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)
#define CLOSESOCKET(s)  closesocket(s)
#else
/* Unix */
typedef int SOCKET;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#define WOULDBLOCK() (errno == EWOULDBLOCK)
#define CLOSESOCKET(s)  close(s)
#endif

#define ISCONNECTED() (conn != INVALID_SOCKET)


#define BUFSIZE 1024
SOCKET conn = INVALID_SOCKET;
#ifdef _WIN32
int winsockstarted = 0;
#endif
unsigned char buffer[BUFSIZE];
unsigned char *bufptr;
signed int buflen;

void (*net_status)(int, char *);


int net_connect(unsigned char *host, int port, void (*status)(int, char *)) {
  struct sockaddr_in address;
  struct hostent *hostent;
#ifdef _WIN32
  WSADATA wsaData;
  u_long nonblock = 1;

  if (!winsockstarted) {
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
      printf("WinSock startup failed\n");
      return(1);
    }
    atexit((void (*)(void))WSACleanup);
    winsockstarted = 1;
  }
#endif
  net_status = status;
  if ((conn = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    if (net_status) net_status(2, "Socket call failed");
    return(1);
  }
  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if (isdigit((int) host[strlen(host) - 1])) {
    address.sin_addr.s_addr = inet_addr(host);
  } else {
    hostent = gethostbyname(host);
    if (hostent) {
      address.sin_addr = *((struct in_addr *) hostent->h_addr);
    }
    else {
      if (net_status) net_status(2, "Unknown host");
      return(1);
    }
  }

  buflen = 0;
  bufptr = buffer;
  if (net_status) net_status(0, "Connecting...");
  if (connect(conn, (struct sockaddr *)&address, sizeof(address)) == 0) {
#ifdef WIN32
    ioctlsocket(conn, FIONBIO, &nonblock);
#else
    fcntl(conn, F_SETFL, O_NONBLOCK);
#endif
    if (net_status) net_status(1, "Connected");
    return(0);
  } else {
    if (net_status) net_status(2, strerror(errno));
    return(1);
  }
}


signed int net_receive(void) {
  if (!ISCONNECTED()) {
    return(-2);
  }
  if (!buflen) {
    buflen = recv(conn, buffer, BUFSIZE, 0);
    if (buflen == 0) {
      net_disconnect();
      return(-2);
    } else if (buflen < 0) {
      buflen = 0;
      if (WOULDBLOCK()) {
	return(-1);
      } else {
	if (net_status) net_status(2, strerror(errno));
	net_disconnect();
	return(-2);
      }
    }
    bufptr = buffer;
  }
  --buflen;

  printf("%s", buffer);

  return(*bufptr++);
}


signed int net_get(unsigned char *onechar)
{
  if (!ISCONNECTED())
  {
    return(-2);
  }
  buflen = recv(conn, onechar, 1, 0);
  if (buflen == 0)
  {
      net_disconnect();
      return(-2);
  }
  return buflen;
}

void net_send(unsigned char c) {
  if (ISCONNECTED()) {
    send(conn, &c, 1, 0);
  }
}


void net_send_string(unsigned char *s) {
  while (*s) {
    net_send(*s++);
  }
}


void net_disconnect(void) {
  if (ISCONNECTED()) {
    CLOSESOCKET(conn);
    conn = INVALID_SOCKET;
    buflen = 0;
  }
}


int net_connected(void) {
  return(ISCONNECTED());
}
