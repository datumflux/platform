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
#ifndef __SOCKAPI_H
#  define __SOCKAPI_H /* extend socket */
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief 소켓에서 공통적으로 사용하는 함수를 구현
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

/*! \addtogroup core_sockapi
 *  @{
 */
#define SOCK_NDELAY						0x10000000
#define SOCK_INTERACTIVE				0x20000000


EXTRN int set_sockattr( int fd, int flags);	/* 소켓 속성을 설정한다. */
EXTRN int get_sockerr( int fd); 	/* 소켓에 발생된 오류를 얻는다. */
EXTRN int get_socktype( int fd);	/* 소켓 형태를 얻는다. (SOCK_...) */
EXTRN bool get_socklisten( int fd);	/* listen() 소켓인지 검사하낟. */
EXTRN bool get_atmark( int fd);		/* out-of-band 데이터가 모두 읽혔는가? */

/* get_inaddr: s_addr 주소를 얻어온다.  */
EXTRN struct in_addr *get_inaddr( const char *s_addr, struct in_addr *);

/* get_infaddr: "ADDR:PORT" 으로 구성된 주소를 얻어온다. */
EXTRN char *get_infaddr( const char *, struct sockaddr_in *);

/* get_ifaddr: ifname(eth) 주소를 얻어온다.  */
EXTRN struct in_addr *get_ifaddr( int fd, const char *ifname, struct in_addr *);


/* set_malive: multicast ttl 값을 설정한다. */
EXTRN int set_malive( int fd, unsigned char nalive);

/* set_mloop: multicast 를 자신에게 반환할 것인지 설정한다. */
EXTRN int set_mloop( int fd, bool isLoop);

/* set_mgroup: multicast group을 설정한다.
 *            만약, naddr < 0 일 경우 in_addr 그룹을 제외 한다.
 */
EXTRN int set_mgroup( int fd, struct in_addr *, int naddr, const char *ifname);

/* is_maddr: addr 주소가 multicast 주소인지 확인한다. */
LOCAL inline bool is_maddr( struct in_addr addr)
{
	uint32_t l_addr = ntohl( addr.s_addr); {
		;
	} return IN_MULTICAST( l_addr);
}

#include <sys/poll.h>

/* pollfd: 특정 소켓의 이벤트를 얻는다. */
EXTRN int pollfd( int fd, int event, int timeout);


#include <errno.h>

/* connect_timedwait: timeout 접속을 처리한다. */
LOCAL inline int connect_timedwait( 
			int fd, struct sockaddr *addr, socklen_t len, int timeout)
{
	if (!connect( fd, addr, len))
		return 0;
	else {
#define X__POLLALL			POLLIN|POLLOUT|POLLERR|POLLHUP
		if ((errno == EINPROGRESS) && 
				(((fd = pollfd( fd, X__POLLALL, timeout)) > 0) &&
				 (fd & (POLLIN|POLLOUT))))
			return 0;
#undef X__POLLALL
	} return -1;

}

#define bindsock( __fd, __addr, __n)					\
	bind( __fd, (struct sockaddr *)__addr, __n)


/* void set_inaddr( in_port_t port, struct sockaddr_in *addr); */
#define set_inport( __port, __addr)				\
	do {										\
		(__addr)->sin_family = AF_INET;			\
		(__addr)->sin_port = htons( __port);	\
	} while(0)


LOCAL inline int udp_socket( void)
{
	int fd;
	
	if ((fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		;
	else {
		if (set_sockattr( fd, SOCK_NDELAY) == 0)
			return fd;
		else {
			;
		} close( fd);
	} return -1;
}


LOCAL inline int udp_server( struct sockaddr_in *addr)
{
	int fd;

	if ((fd = udp_socket()) < 0)
		;
	else {
		if (bindsock( fd, addr, sizeof(*addr)) == 0)
			return fd;
		else {
			;
		} close( fd);
	} return -1;
}

LOCAL inline int tcp_socket(struct sockaddr *addr)
{
	int fd;

	if ((fd = socket((addr ? addr->sa_family: AF_INET), SOCK_STREAM, 0)) < 0)
		;
	else {
        static int flag = 1;

		if ((set_sockattr( fd, SOCK_NDELAY) < 0) ||
                (setsockopt(fd, IPPROTO_TCP, 
                            TCP_NODELAY, (char *)&flag, sizeof(int)) < 0))
            ;
		else {
            static int length = sizeof(int);

            if (setsockopt(fd, SOL_SOCKET, 
                        SO_SNDBUF, (char *)&length, sizeof(int)) == 0)
                return fd;
		} close( fd);
	} return -1;
}

LOCAL inline int tcp_server( struct sockaddr *addr, int backlog)
{
	int fd;

	if ((fd = tcp_socket(addr)) < 0)
		;
	else {
		int addrsize = sizeof(struct sockaddr_in);
		switch (addr->sa_family)
		{
			case AF_INET6: {
				static int opt = 1;

				if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&opt, sizeof(opt)) < 0)
					break;
			} addrsize = sizeof(struct sockaddr_in6);;
			case AF_INET: if (!bindsock( fd, addr, addrsize) &&
								((backlog < 0) || !listen( fd, backlog)))
							return fd;
		}
		close( fd);
	} return -1;
}

EXTRN int tcp_connect( const char *cp, struct sockaddr_in *, int timeout);

LOCAL inline int  tcp_bind( const char *s_addr, struct sockaddr_in *addr)
{
	int fd;

	if ((fd = tcp_socket(NULL)) < 0)
		return -1;
	else {
		(s_addr ? get_infaddr( s_addr, addr): 0);
		if (bindsock( fd, &addr, sizeof(addr)) == 0)
			return fd;
		else {
			;
		} close( fd);
	} return -1;
}

LOCAL inline bool islisten( struct sockaddr_in addr) {
	return (ntohl( addr.sin_addr.s_addr & 0xff) == 0);
}

LOCAL inline int tcp_accept( int fd, struct sockaddr_in *addr)
{
	socklen_t naddr = sizeof(struct sockaddr_in);
	return accept( fd, (struct sockaddr*)addr, &naddr);
}

/* @} */
#endif
