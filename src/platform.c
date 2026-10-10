#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"

int netResolveHost(const char *hostname, struct in_addr *addr) {
  struct hostent *lookup;

  // Dotted addresses need no lookup; this also accepts 0.0.0.0
  if (inet_pton(AF_INET, hostname, addr) == 1) return 0;

  lookup = gethostbyname(hostname);
  if (lookup == NULL || lookup->h_addrtype != AF_INET) return -1;
  memcpy(addr, lookup->h_addr_list[0], sizeof(*addr));
  return 0;
}

void netLocalAddress(char *buf, int buflen) {
  // Connecting a UDP socket sends nothing, but makes the OS pick the
  // interface it would route through, which is the address others can use.
  struct sockaddr_in remote, local;
  socklen_t len = sizeof(local);
  int sock;

  strncpy(buf, "localhost", buflen);
  buf[buflen - 1] = 0;
  if (netInitialize() != 0) return;
  sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) return;
  memset(&remote, 0, sizeof(remote));
  remote.sin_family = AF_INET;
  remote.sin_port = htons(53);
  inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
  if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) == 0 &&
      getsockname(sock, (struct sockaddr *)&local, &len) == 0) {
    inet_ntop(AF_INET, &local.sin_addr, buf, buflen);
  }
  netCloseSocket(sock);
}

#ifdef _WIN32

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

static void netCleanup() {
  WSACleanup();
}

int netInitialize() {
  static int initialized = 0;
  WSADATA wsaData;
  int status;
  if (initialized) return 0;
  status = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (status == 0) {
    initialized = 1;
    atexit(netCleanup);
  }
  return status;
}

void netCloseSocket(int mysocket) {
  closesocket((SOCKET)mysocket);
}

int netLastError() {
  return WSAGetLastError();
}

void netIgnoreConnReset(int mysocket) {
  BOOL report = FALSE;
  DWORD bytes = 0;
  WSAIoctl((SOCKET)mysocket, SIO_UDP_CONNRESET, &report, sizeof(report), NULL, 0, &bytes, NULL, NULL);
}

double platformNow() {
  static LARGE_INTEGER frequency;
  LARGE_INTEGER counter;
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)frequency.QuadPart;
}

#else

int netInitialize() {
  return 0;
}

void netCloseSocket(int mysocket) {
  close(mysocket);
}

int netLastError() {
  return errno;
}

void netIgnoreConnReset(int mysocket) {
  (void)mysocket;
}

double platformNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

#endif
