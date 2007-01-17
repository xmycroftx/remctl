/*  $Id$
**
**  Utility functions for network connections.
**
**  This is a collection of utility functions for network connections and
**  socket creation, encapsulating some of the complexities of IPv4 and IPv6
**  support and abstracting operations common to most network code.
**
**  All of the portability difficulties with supporting IPv4 and IPv6 should
**  be encapsulated in the combination of this code and replacement
**  implementations for functions that aren't found on some pre-IPv6 systems.
**  No other part of remctl should have to care about IPv4 vs. IPv6.
**
**  Copyright (c) 2004, 2005, 2006, 2007
**      by Internet Systems Consortium, Inc. ("ISC")
**  Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
**      2002, 2003 by The Internet Software Consortium and Rich Salz
**
**  This code is derived from software contributed to the Internet Software
**  Consortium by Rich Salz.
**
**  Permission to use, copy, modify, and distribute this software for any
**  purpose with or without fee is hereby granted, provided that the above
**  copyright notice and this permission notice appear in all copies.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
**  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
**  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
**  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
**  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
**  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <config.h>
#include <system.h>
#include <portable/socket.h>

#include <errno.h>

#include <util/util.h>

/* Macros to set the len attribute of sockaddrs. */
#if HAVE_STRUCT_SOCKADDR_SA_LEN
# define sin_set_length(s)      ((s)->sin_len  = sizeof(struct sockaddr_in))
# define sin6_set_length(s)     ((s)->sin6_len = sizeof(struct sockaddr_in6))
#else
# define sin_set_length(s)      /* empty */
# define sin6_set_length(s)     /* empty */
#endif

/* If SO_REUSEADDR isn't available, make calls to set_reuseaddr go away. */
#ifndef SO_REUSEADDR
# define network_set_reuseaddr(fd)      /* empty */
#endif


/*
**  Set SO_REUSEADDR on a socket if possible (so that something new can listen
**  on the same port immediately if the daemon dies unexpectedly).
*/
#ifdef SO_REUSEADDR
static void
network_set_reuseaddr(int fd)
{
    int flag = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
        syswarn("cannot mark bind address reusable");
}
#endif


/*
**  Create an IPv4 socket and bind it, returning the resulting file descriptor
**  (or -1 on a failure).
*/
int
network_bind_ipv4(const char *address, unsigned short port)
{
    int fd;
    struct sockaddr_in server;
    struct in_addr addr;

    /* Create the socket. */
    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        syswarn("cannot create IPv4 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "0.0.0.0";

    /* Flesh out the socket and do the bind. */
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (!inet_aton(address, &addr)) {
        warn("invalid IPv4 address %s", address);
        return -1;
    }
    server.sin_addr = addr;
    sin_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        syswarn("cannot bind socket for %s,%hu", address, port);
        return -1;
    }
    return fd;
}


/*
**  Create an IPv6 socket and bind it, returning the resulting file descriptor
**  (or -1 on a failure).  Note that we don't warn (but still return failure)
**  if the reason for the socket creation failure is that IPv6 isn't
**  supported; this is to handle systems like many Linux hosts where IPv6 is
**  available in userland but the kernel doesn't support it.
*/
#if HAVE_INET6
int
network_bind_ipv6(const char *address, unsigned short port)
{
    int fd;
    struct sockaddr_in6 server;
    struct in6_addr addr;

    /* Create the socket. */
    fd = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        if (errno != EAFNOSUPPORT && errno != EPROTONOSUPPORT)
            syswarn("cannot create IPv6 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "::";

    /* Flesh out the socket and do the bind. */
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(port);
    if (inet_pton(AF_INET6, address, &addr) < 1) {
        warn("invalid IPv6 address %s", address);
        close(fd);
        return -1;
    }
    server.sin6_addr = addr;
    sin6_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        syswarn("cannot bind socket for %s,%hu", address, port);
        close(fd);
        return -1;
    }
    return fd;
}
#else /* HAVE_INET6 */
int
network_bind_ipv6(const char *address, unsigned short port)
{
    warn("cannot bind %s,%hu: not built with IPv6 support", address, port);
    return -1;
}
#endif /* HAVE_INET6 */


/*
**  Create and bind sockets for every local address, as determined by
**  getaddrinfo if IPv6 is available (otherwise, just use the IPv4 loopback
**  address).  Takes the port number, and then a pointer to an array of
**  integers and a pointer to a count of them.  Allocates a new array to hold
**  the file descriptors and stores the count in the third argument.
*/
#if HAVE_INET6
void
network_bind_all(unsigned short port, int **fds, int *count)
{
    struct addrinfo hints, *addrs, *addr;
    int error, fd, size;
    char service[16], name[INET6_ADDRSTRLEN];

    *count = 0;

    /* Do the query to find all the available addresses. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%hu", port);
    error = getaddrinfo(NULL, service, &hints, &addrs);
    if (error < 0) {
        warn("getaddrinfo failed: %s", gai_strerror(error));
        return;
    }

    /* Now, try to bind each of them.  Start the fds array at two entries,
       assuming an IPv6 and IPv4 socket, and grow it by two when necessary. */
    size = 2;
    *fds = xmalloc(size * sizeof(int));
    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        network_sockaddr_sprint(name, sizeof(name), addr->ai_addr);
        if (addr->ai_family == AF_INET)
            fd = network_bind_ipv4(name, port);
        else if (addr->ai_family == AF_INET6)
            fd = network_bind_ipv6(name, port);
        else
            continue;
        if (fd >= 0) {
            if (*count >= size) {
                size += 2;
                *fds = xrealloc(*fds, size * sizeof(int));
            }
            (*fds)[*count] = fd;
            (*count)++;
        }
    }
    freeaddrinfo(addrs);
}
#else /* HAVE_INET6 */
void
network_bind_all(unsigned short port, int **fds, int *count)
{
    int fd;

    fd = network_bind_ipv4("0.0.0.0", port);
    if (fd >= 0) {
        *fds = xmalloc(sizeof(int));
        *fds[0] = fd;
        *count = 1;
    } else {
        *fds = NULL;
        *count = 0;
    }
}
#endif /* HAVE_INET6 */


/*
**  Binds the given socket to an appropriate source address for its family,
**  using innconf information or the provided source address.  Returns true on
**  success and false on failure.
*/
static int
network_source(int fd, int family, const char *source)
{
    if (source == NULL)
        return 1;
    if (family == AF_INET) {
        struct sockaddr_in saddr;

        if (source == NULL || strcmp(source, "all") == 0)
            return 1;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        if (!inet_aton(source, &saddr.sin_addr))
            return 0;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#ifdef HAVE_INET6
    else if (family == AF_INET6) {
        struct sockaddr_in6 saddr;

        if (source == NULL || strcmp(source, "all") == 0)
            return 1;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, source, &saddr.sin6_addr) < 1)
            return 0;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#endif
    else
        return 1;
}


/*
**  Given a linked list of addrinfo structs representing the remote service,
**  try to create a local socket and connect to that service.  Takes an
**  optional source address.  Try each address in turn until one of them
**  connects.  Returns the file descriptor of the open socket on success, or
**  -1 on failure.  Tries to leave the reason for the failure in errno.
*/
int
network_connect(struct addrinfo *ai, const char *source)
{
    int fd = -1;
    int oerrno;
    int success;

    for (success = 0; ai != NULL; ai = ai->ai_next) {
        if (fd >= 0)
            close(fd);
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (!network_source(fd, ai->ai_family, source))
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            success = 1;
            break;
        }
    }
    if (success)
        return fd;
    else {
        if (fd >= 0) {
            oerrno = errno;
            close(fd);
            errno = oerrno;
        }
        return -1;
    }
}


/*
**  Like network_connect, but takes a host and a port instead of an addrinfo
**  struct list.  Returns the file descriptor of the open socket on success,
**  or -1 on failure.  If getaddrinfo fails, errno may not be set to anything
**  useful.
*/
int
network_connect_host(const char *host, unsigned short port,
                     const char *source)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    int fd, oerrno;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &ai) != 0)
        return -1;
    fd = network_connect(ai, source);
    oerrno = errno;
    freeaddrinfo(ai);
    errno = oerrno;
    return fd;
}


/*
**  Create a new socket of the specified domain and type and do the binding as
**  if we were a regular client socket, but then return before connecting.
**  Returns the file descriptor of the open socket on success, or -1 on
**  failure.  Intended primarily for the use of clients that will then go on
**  to do a non-blocking connect.
*/
int
network_client_create(int domain, int type, const char *source)
{
    int fd, oerrno;

    fd = socket(domain, type, 0);
    if (fd < 0)
        return -1;
    if (!network_source(fd, domain, source)) {
        oerrno = errno;
        close(fd);
        errno = oerrno;
        return -1;
    }
    return fd;
}


/*
**  Print an ASCII representation of the address of the given sockaddr into
**  the provided buffer.  This buffer must hold at least INET_ADDRSTRLEN
**  characters for IPv4 addresses and INET6_ADDRSTRLEN characters for IPv6, so
**  generally it should always be as large as the latter.  Returns success or
**  failure.
*/
int
network_sockaddr_sprint(char *dst, size_t size, const struct sockaddr *addr)
{
    const char *result;

#ifdef HAVE_INET6
    if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6;

        sin6 = (const struct sockaddr_in6 *) addr;
        if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
            struct in_addr in;

            memcpy(&in, sin6->sin6_addr.s6_addr + 12, sizeof(in));
            result = inet_ntop(AF_INET, &in, dst, size);
        } else
            result = inet_ntop(AF_INET6, &sin6->sin6_addr, dst, size);
        return (result != NULL);
    }
#endif
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *sin;

        sin = (const struct sockaddr_in *) addr;
        result = inet_ntop(AF_INET, &sin->sin_addr, dst, size);
        return (result != NULL);
    } else {
        errno = EAFNOSUPPORT;
        return 0;
    }
}


/*
**  Compare the addresses from two sockaddrs and see whether they're equal.
**  IPv4 addresses that have been mapped to IPv6 addresses compare equal to
**  the corresponding IPv4 address.
*/
int
network_sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b)
{
    const struct sockaddr_in *a4 = (const struct sockaddr_in *) a;
    const struct sockaddr_in *b4 = (const struct sockaddr_in *) b;

#ifdef HAVE_INET6
    const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *) a;
    const struct sockaddr_in6 *b6 = (const struct sockaddr_in6 *) b;
    const struct sockaddr *tmp;

    if (a->sa_family == AF_INET && b->sa_family == AF_INET6) {
        tmp = a;
        a = b;
        b = tmp;
        a6 = (const struct sockaddr_in6 *) a;
        b4 = (const struct sockaddr_in *) b;
    }
    if (a->sa_family == AF_INET6) {
        if (b->sa_family == AF_INET6)
            return IN6_ARE_ADDR_EQUAL(&a6->sin6_addr, &b6->sin6_addr);
        else if (b->sa_family != AF_INET)
            return 0;
        else if (!IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr))
            return 0;
        else {
            struct in_addr in;

            memcpy(&in, a6->sin6_addr.s6_addr + 12, sizeof(in));
            return (in.s_addr == b4->sin_addr.s_addr);
        }
    }
#endif

    if (a->sa_family != AF_INET || b->sa_family != AF_INET)
        return 0;
    return (a4->sin_addr.s_addr == b4->sin_addr.s_addr);
}


/*
**  Returns the port of a sockaddr or 0 on error.
*/
unsigned short
network_sockaddr_port(const struct sockaddr *sa)
{
    const struct sockaddr_in *sin;

#ifdef HAVE_INET6
    const struct sockaddr_in6 *sin6;

    if (sa->sa_family == AF_INET6) {
        sin6 = (const struct sockaddr_in6 *) sa;
        return htons(sin6->sin6_port);
    }
#endif
    if (sa->sa_family != AF_INET)
        return 0;
    else {
        sin = (const struct sockaddr_in *) sa;
        return htons(sin->sin_port);
    }
}


/*
**  Compare two addresses given as strings, applying an optional mask.
**  Returns true if the addresses are equal modulo the mask and false
**  otherwise, including on syntax errors in the addresses or mask
**  specification.
*/
int
network_addr_match(const char *a, const char *b, const char *mask)
{
    struct in_addr a4, b4, tmp;
    unsigned long cidr;
    char *end;
    unsigned int i;
    unsigned long bits, addr_mask;
#ifdef HAVE_INET6
    struct in6_addr a6, b6;
#endif

    /* If the addresses are IPv4, the mask may be in one of two forms.  It can
       either be a traditional mask, like 255.255.0.0, or it can be a CIDR
       subnet designation, like 16.  (The caller should have already removed
       the slash separating it from the address.) */
    if (inet_aton(a, &a4) && inet_aton(b, &b4)) {
        if (mask == NULL)
            addr_mask = htonl(0xffffffffUL);
        else if (strchr(mask, '.') == NULL) {
            cidr = strtoul(mask, &end, 10);
            if (cidr > 32 || *end != '\0')
                return 0;
            for (bits = 0, i = 0; i < cidr; i++)
                bits |= (1 << (31 - i));
            addr_mask = htonl(bits);
        } else if (inet_aton(mask, &tmp))
            addr_mask = tmp.s_addr;
        else
            return 0;
        return (a4.s_addr & addr_mask) == (b4.s_addr & addr_mask);
    }
            
#ifdef HAVE_INET6
    /* Otherwise, if the address is IPv6, the mask is required to be a CIDR
       subnet designation. */
    if (!inet_pton(AF_INET6, a, &a6) || !inet_pton(AF_INET6, b, &b6))
        return 0;
    if (mask == NULL)
        cidr = 128;
    else {
        cidr = strtoul(mask, &end, 10);
        if (cidr > 128 || *end != '\0')
            return 0;
    }
    for (i = 0; i * 8 < cidr; i++) {
        if ((i + 1) * 8 <= cidr) {
            if (a6.s6_addr[i] != b6.s6_addr[i])
                return 0;
        } else {
            for (addr_mask = 0, bits = 0; bits < cidr % 8; bits++)
                addr_mask |= (1 << (7 - bits));
            if ((a6.s6_addr[i] & addr_mask) != (b6.s6_addr[i] & addr_mask))
                return 0;
        }
    }
    return 1;
#else
    return 0;
#endif
}