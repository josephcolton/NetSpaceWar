#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "platform.h"

#ifdef _WIN32

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

static void netCleanup() {
  WSACleanup();
}

int netInitialize() {
  WSADATA wsaData;
  int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (status == 0) atexit(netCleanup);
  return status;
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
