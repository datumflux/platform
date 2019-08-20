/*
Copyright (c) 2018-2019 DATUMFLUX CORP.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
 */
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "sockapi.h"
#include <netinet/ip.h>


/*! \defgroup core_sockapi 소켓에서 공통적으로 사용하는 함수를 정의
 *  @{
 */
int set_sockattr( int fd, int flags) 
{
	int tos = (flags & SOCK_INTERACTIVE) ? IPTOS_LOWDELAY : IPTOS_THROUGHPUT;

	if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
_gC:	;
	else {
		static int on = 1;

		if ((flags & SOCK_NDELAY) == 0)
			;
		else {
			/* NONBLOCK으로 설정한다. */
			if (((flags = fcntl( fd, F_GETFL, 0)) < 0) || 
					(fcntl( fd, F_SETFL, flags | O_NONBLOCK) < 0)) 
				goto _gC;
			else {
				int type;
				unsigned int length = sizeof(int);

				if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) == 0)
					switch (type)
					{
						case SOCK_STREAM:
						{
							/* NAGLE 알고리즘 사용 중지 */
							setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(int));
						} break;

						default: break;
					}
				else goto _gC;
			}
		}
#define setsolsocket( __fd, __opt, __flags)		\
	setsockopt( __fd, SOL_SOCKET, __opt, (char *)&__flags, sizeof(__flags))
		if (/* (setsolsocket( fd, SO_KEEPALIVE, on) < 0) || */
				(setsolsocket( fd, SO_REUSEADDR, on) < 0) /* ||
				(setsolsocket( fd, SO_REUSEPORT, on) < 0) */)
			;
		else {
   			struct linger linger; 

			linger.l_onoff = 1;
			linger.l_linger = 0;
			return setsolsocket( fd, SO_LINGER, linger);
		}
	} return -1;
}

#include <net/if.h>
#include <net/ethernet.h>

struct in_addr *get_inaddr( const char *s_addr, struct in_addr *addr)
{
	if (s_addr == NULL)
		addr->s_addr = INADDR_ANY;
	else {
		if ((addr->s_addr = inet_addr( s_addr)) != INADDR_NONE)
			;
		else {
			struct hostent *h;

			if ((h = gethostbyname( s_addr)) == NULL) addr = NULL;
			else *addr = *((struct in_addr *)h->h_addr_list[0]);
		}
	} return addr;
}


#include <string.h>
#include <stdlib.h>		/* strtol() */
#include <ctype.h>		/* tolower() */


char *get_infaddr( const char *s_addr, struct sockaddr_in *addr)
{
	char *end,
		 *start;

	if ((s_addr == NULL) ||
			((end = strchr( s_addr, ':')) == NULL))
_gC:	return NULL;
	else {
		int s_port = end++ - s_addr;
		char cp[s_port + 2];

		strncpy( cp, s_addr, s_port); cp[s_port] = 0;
		if (!get_inaddr( cp, &addr->sin_addr) ||
				(addr->sin_addr.s_addr == INADDR_NONE) || 
				((s_port = strtol( end, &start, 10)) <= 0))
			goto _gC;
		else {
			;
		} set_inport( s_port, addr);
	} return start;
}


#include <sys/ioctl.h>

struct in_addr *get_ifaddr( int fd, const char *ifname, struct in_addr *addr)
{
	if ((fd <= 0) || 
			(ifname == NULL))
		addr->s_addr = INADDR_ANY;
	else {
		struct ifreq ifreq;

		strncpy( ifreq.ifr_name, ifname, IFNAMSIZ); 
		if (ioctl( fd, SIOCGIFADDR, &ifreq) < 0) addr = NULL;
		else *addr = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;
	} return addr;
}


#define setipsocket( __fd, __opt, __flags)				\
	setsockopt( __fd, IPPROTO_IP, __opt, (char *)&__flags, sizeof(__flags))

int set_malive( int fd, unsigned char nalive) {
	return setipsocket( fd, IP_MULTICAST_TTL, nalive);
}

int set_mloop( int fd, bool is_loop) {
	return setipsocket( fd, IP_MULTICAST_LOOP, is_loop);
}

int set_mgroup( int fd, struct in_addr *addr, int naddr, const char *ifname)
{
	if (naddr == 0)
	    return -1;
	else {
        struct ip_mreq mreq;

        if (ifname == NULL) mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        else if (get_ifaddr( fd, ifname, &mreq.imr_interface) == NULL) return -1;
        {
            LOCAL struct m_cmd {
                int ncmd,
                        nflow;
            } __F[] = {
                    { IP_ADD_MEMBERSHIP , -1 },
                    { IP_DROP_MEMBERSHIP,  1 }
            }; struct m_cmd *f = &__F[(naddr < 0)];

            struct in_addr *start = addr;
            do {
                mreq.imr_multiaddr = *addr++;
                if (setipsocket( fd, f->ncmd, mreq))
                    return (addr - start);
            } while ((naddr += f->nflow) != 0);
        }
	} return 0;
}


int get_sockerr( int fd)
{
	int r;
	socklen_t len = sizeof(r);

	return (getsockopt(fd, SOL_SOCKET, SO_ERROR, &r, &len) < 0) ? -1: r;
}


int get_socktype( int fd)
{
	int r;
	socklen_t len = sizeof(r);

	return (getsockopt(fd, SOL_SOCKET, SO_TYPE, &r, &len) < 0)? -1: r;
}


bool get_socklisten( int fd)
{
	int r;
	socklen_t len = sizeof(r);

	return (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &r, &len) < 0)? false: r;
}


bool get_atmark( int fd)
{
	int r;
	return (ioctl( fd, SIOCATMARK, &r) < 0) ? false: r;
}


int pollfd( int fd, int event, int timeout)
{
	struct pollfd pfds = {
		.fd = fd,
		.events = event,
		.revents = 0
	};

_gW:if ((event = poll( &pfds, 1, timeout)) < 0)
		switch (errno)
		{
			/* case EINTR : */
			case EAGAIN: goto _gW;
			default    : break;
		}
	else if (event) return pfds.revents;
	else errno = ETIMEDOUT;
	return -1;
}


int tcp_connect( const char *s_addr, struct sockaddr_in *addr, int timeout)
{
	int fd;

	if ((fd = tcp_socket(NULL)) < 0)
		return -1;
	else {
		if (s_addr &&
				((addr->sin_addr.s_addr = inet_addr( s_addr)) == INADDR_NONE))
		{
			struct hostent *h;

			if (!(h = gethostbyname( s_addr)))
				;
			else {
				register int naddr = 0;

				do {
					addr->sin_addr = *((struct in_addr *)h->h_addr_list[naddr]);
					if (!connect_timedwait( fd, 
								(void *)addr, sizeof(*addr), timeout))
						return 0;
				} while (h->h_addr_list[++naddr]);
			} goto _gC;
		}

		if (!connect_timedwait( fd, (void *)addr, sizeof(*addr), timeout))
			return fd;
		else {
_gC:		;
		} close( fd);
	} return -1;
}
/* @} */