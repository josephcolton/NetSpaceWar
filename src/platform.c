#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"

#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#endif

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

int netInterfaces(struct netInterface *list, int max) {
  ULONG size = 16384, flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES *)malloc(size), *a;
  IP_ADAPTER_UNICAST_ADDRESS *u;
  ULONG status = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &size);
  int count = 0;

  if (status == ERROR_BUFFER_OVERFLOW) {
    free(adapters);
    adapters = (IP_ADAPTER_ADDRESSES *)malloc(size);
    status = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &size);
  }
  if (status != NO_ERROR) {
    free(adapters);
    return 0;
  }
  for (a = adapters; a != NULL && count < max; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    for (u = a->FirstUnicastAddress; u != NULL && count < max; u = u->Next) {
      struct sockaddr_in *sin = (struct sockaddr_in *)u->Address.lpSockaddr;
      unsigned long mask = (u->OnLinkPrefixLength == 0) ? 0 : 0xffffffffUL << (32 - u->OnLinkPrefixLength);
      list[count].address = sin->sin_addr;
      list[count].broadcast.s_addr = sin->sin_addr.s_addr | htonl(~mask & 0xffffffffUL);
      count++;
    }
  }
  free(adapters);
  return count;
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

int netInterfaces(struct netInterface *list, int max) {
  struct ifaddrs *all, *i;
  int count = 0;

  if (getifaddrs(&all) != 0) return 0;
  for (i = all; i != NULL && count < max; i = i->ifa_next) {
    if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_INET) continue;
    if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
    list[count].address = ((struct sockaddr_in *)i->ifa_addr)->sin_addr;
    if ((i->ifa_flags & IFF_BROADCAST) && i->ifa_broadaddr != NULL) {
      list[count].broadcast = ((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr;
    } else {
      list[count].broadcast.s_addr = htonl(INADDR_BROADCAST);
    }
    count++;
  }
  freeifaddrs(all);
  return count;
}

double platformNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

#endif
