#ifndef PLATFORM_H
#define PLATFORM_H

/*
 * Operating system differences live here, so the rest of the code can use
 * BSD sockets and a monotonic clock the same way on Linux, macOS and Windows.
 *
 * Sockets are stored in plain ints throughout the code. On Windows socket()
 * returns a SOCKET (an unsigned handle); real handle values fit in an int,
 * and INVALID_SOCKET becomes -1, so the existing "< 0" error checks work.
 */

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
#endif

// Start up the socket library before creating any socket. Safe to call
// more than once. Returns 0 on success.
int netInitialize();

// Look up a host name or dotted IP address ("0.0.0.0" means any interface).
// Returns 0 on success.
int netResolveHost(const char *hostname, struct in_addr *addr);

// Close a socket
void netCloseSocket(int mysocket);

// Best guess at this machine's LAN address, for telling other players where
// to connect. Writes a dotted IP into buf, or "localhost" if there is none.
void netLocalAddress(char *buf, int buflen);

// Error code of the last failed socket call (errno, or WSAGetLastError())
int netLastError();

// On Windows, a UDP socket reports an error on the next recvfrom() after
// sending to a port nobody is listening on (for example a client that quit).
// This turns that behavior off; it does nothing elsewhere.
void netIgnoreConnReset(int mysocket);

// Seconds since an arbitrary fixed point, from a monotonic clock
double platformNow();

#endif
