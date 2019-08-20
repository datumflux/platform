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
#ifndef __EVENTIO_H
#  define __EVENTIO_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief epoll기반의 비동기 입출력을 처리
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"
#include <sys/epoll.h>

/*! \addtogroup core_eventio
 *  @{
 */

/*! \enum epoll_set
 *
 * 	   eventio에 의한 확장 EPOOL 이벤트
 *
 * 	\hideinitializer
 */
enum {
	EPOLLBAND = 0x10000, /*!< action 발생(accept() 처리용 예약)  */
	EPOLLALIVE = 0x20000, /*!< 이벤트 발생 검사 */
};

#define EPOLLBAND					EPOLLBAND
#define EPOLLALIVE					EPOLLALIVE

typedef struct eio_adapter *eadapter;
typedef struct eio_file    *efile;



EXTRN int eio_init( int nfiles); /* efile 리소스 초기화 */

/*! \brief eventio에 할당된 전체 관리 가능 파일 갯수를 얻는다 */
#define eio_nfiles()				eadapter_nfiles(NULL)

EXTRN eadapter eadapter_create( int nfiles, void *arg); /* eadapter 초기화 */
EXTRN int      eadapter_atfork( eadapter,
	void (*alive_atfork)(efile, void *), void *udata); /* eadapter 재초기화 */
EXTRN int      eadapter_destroy( eadapter);

EXTRN void   **eadapter_udata( eadapter);

EXTRN int      eadapter_nfiles( eadapter);	/* 최대 관리 files수 */
EXTRN int      eadapter_nactive( eadapter);	/* 개방된 files수 */

/* eadapter_shutdown: adapter에 연결된 모든 fd를 해제한다. */
EXTRN int eadapter_shutdown( eadapter);

/*! \enum epoll_status
 *
 * 	   eadapter_wait()에 의해 반환되는 eventio 확장 이벤트
 *
 * 	\hideinitializer
 */
enum {
	EPOLLIDLE = 0x0000,				/*!< timeout 발생 */
	EPOLLRESET = 0x8000, 			/*!< 발생 이벤트 처리 완료 */
	EPOLLCACHE = EPOLLIN,			/*!< 입력 cache된 efile에서 데이터 발생 */
	EPOLLRAW = EPOLLIN | 0x4000,	/*!< cache되지 않은 efile에서 데이터 발생 */
};

#define EPOLLIDLE					EPOLLIDLE
#define EPOLLRESET					EPOLLRESET
#define EPOLLCACHE					EPOLLIN
#define EPOLLRAW					EPOLLRAW

/* 
 *   EPOLLIDLE					timeout 발생
 *   EPOLLRESET					발생 이벤트 처리 완료
 *   EPOLLHUP					EPOLLHUP 발생
 *   EPOLLBAND					EPOLLBAND 발생 (*nwait - 발생 이벤트)
 *
 *   EPOLLCACHE					cache된 EPOLLIN 발생
 *   EPOLLRAW					cache되지 않은 EPOLLIN 발생
 *
 *   EPOLLONESHOT				EPOLLONESHOT처리된 이벤트 발생
 */

struct eadapter_event {
    uint32_t events;
    struct eio_file *f;
};

EXTRN int eadapter_wait( eadapter,
		struct eadapter_event *, size_t *nwait, int timeout);
EXTRN int eadapter_dispatch( eadapter, efile *, size_t *nwait, int timeout);

/* eadapter_alive: EPOLLRDALIVE 설정 efile를 검사한다. */
EXTRN int eadapter_alive( eadapter, efile *, int max, uint32_t timeout);

/* eadapter_flush: adapter에 설정된 cache를 flush 한다. */
EXTRN int eadapter_flush( eadapter, efile *, int max, int timeout);

#include "cache.h"

/*! \brief efile_setcache()에 설정되는 입력 cache 버퍼 filter callback 함수
 *
 * 		이 함수는 eadapter_wait()에서 데이터 발생시 EPOLLCACHE이벤트의 발생
 * 		유무를 판단하는 함수로 데이터를 블럭별로 분리하는 기능을 수행한다.
 *
 *  \code
 *     int eacache_combine( efile efd, struct cache *ca, uint64_t *l)
 *     {
 *        uint32_t T;
 *
 *        if ((int)(*l) > ca->len)
 *     _gD:  return 0;
 *        else {
 *           if ((*l) > 0) T = (uint32_t)(*l);
 *           else if (ca->len < (int)sizeof(uint32_t)) goto _gD;
 *           else {
 *                cache_look( ca, (char *)&T, sizeof(T), 0);
 *                if ((int)(T += sizeof(T)) <= ca->len)
 *                   return T;
 *                else {
 *                   (*l) = T; 
 *                } goto _gD;
 *           } (*l) = 0;
 *        } return T;
 *     }
 *
 *     ...
 *     efile_setcache( efd, MAX_CACHESIZE, ecache_combine, 0, MAX_CACHESIZE);
 *     ...
 *  \endcode
 *
 * 	\param efile	efile_open()에서 할당받은 주소
 * 	\param cache	데이터가 저장된 cache 주소
 * 	\param uint64_t	efile_setadapter()에서 설정된 추가 param
 * 	\retval			읽을수 있는 블럭 크기
 */
typedef int (*ecache_combine_t)( efile, struct cache *, uint64_t *);


EXTRN int      efile_regno( efile);		/* efile의 일련번호 */
EXTRN efile    efile_get( int);			/* efile의 일련번호 */
EXTRN eadapter efile_adapter( efile);	/* efile에 연결된 eadapter */
EXTRN uint64_t efile_lutime( efile);	/* efile의 마지막 EPOLLIN발생 시간 */
EXTRN uint64_t efile_futime( efile);	/* efile의 마지막 flush 시간 */

EXTRN int      efile_fileno( efile);	/* efile에 연결된 fd */
EXTRN void   **efile_udata( efile);

EXTRN uint32_t efile_isevent( efile);	/* 설정 이벤트 정보를 얻는다. */

/* efile_iscache: cache설정 정보를 얻는다.
 *   iscache	0x01	in cache
 *         		0x02	out cache
 */
EXTRN ecache_combine_t efile_iscache( efile, int *iscache);

/* efile_open: efile를 할당한다.
 *
 *    events		기본 설정하고자 하는 epoll events
 *       | EPOLLBAND		EPOLLBAND 설정된 fd
 *
 *       | EPOLLALIVE		eadapter_alive()하고자 하는 경우
 *       | EPOLLIN			EPOLLIN
 *       | EPOLLOUT			EPOLLOUT
 *       | EPOLLET			edge trigger
 *       | EPOLLONESHOT		1회성 event 설정
 */
EXTRN efile efile_open( int fd, uint32_t events, void *arg);
EXTRN int   efile_close( efile);

/* efile_setcache; efile의 cache를 설정한다. */
EXTRN int efile_setcache( 
			efile, int in_b, ecache_combine_t, uint64_t narg, int out_b
		);

/* efile_close()된 efile의 메모리를 반환한다. */
EXTRN int efile_purge(uint32_t timeout);

/* efile_shutdown()을 통해 제거된 efile를 얻는다. */
EXTRN int efile_refuse(efile *, int max, uint32_t timeout);

EXTRN struct cache *efile_rcache( efile);
EXTRN struct cache *efile_wcache( efile);

/* efile_setadapter: efile에 eadapter를 연결한다. */
EXTRN int efile_setadapter( efile, eadapter);

/* efile_setalive: efile의 이벤트 발생을 reset 한다. */
EXTRN int efile_setalive( efile);

EXTRN int efile_shutdown( efile, bool refuse);	/* 연결된 eadapter에셔 분리 */

/* efile_setevent: eadapter에 연결된 efile의 epoll 이벤트를 설정한다.
 *
 *    events		기본 설정하고자 하는 epoll events
 *       | EPOLLBAND		EPOLLBAND설정된 fd
 *
 *       | EPOLLALIVE		eadapter_alive()하고자 하는 경우
 *       | EPOLLIN			EPOLLIN
 *       | EPOLLOUT			EPOLLOUT
 *       | EPOLLET			edge trigger
 *       | EPOLLONESHOT		1회성 event 설정
 */
EXTRN int efile_setevent( efile, uint32_t events);

EXTRN ssize_t efile_ready( efile, void **buffer, size_t len);

EXTRN ssize_t efile_read( efile, void *buffer, size_t len);
EXTRN ssize_t efile_write( efile, const void *buffer, size_t len);

EXTRN ssize_t efile_transfer( struct eio_file *f, struct cache *o);

EXTRN int efile_flush( efile, int timeout);	/* cache를 비운다. */
/*! @} */
#endif
