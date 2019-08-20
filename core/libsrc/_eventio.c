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
#include "eventio.h"
#include "list.h"
#include "lock.h"
#include "utime.h"

#include <errno.h>
#include <sys/ioctl.h>

/*! \defgroup core_eventio epoll 기반의 event 관리자
 *  @{
 *
 *   리눅스는 전통적으로 이벤트 기반의 비동기 통지 방식 보다는 동기적으로
 *   관심있어 하는 파일(소켓)에 읽기/쓰기 이벤트가 발생했는지를 검사하는
 *   입출력 다중화 방식을 주로 사용해왔다. 혹은 여러개의 프로세스를 생성
 *   시켜서 다중의 클라이언트를 처리하는 방법을 주로 이용해왔다. 
 *
 *   이들 방법은 보통 비용이 매우 많이 소비된다. 입출력 다중화를 위해서
 *   사용하는 select(2), poll(2)은 커널과 유저공간사이에 여러번의 데이터
 *   복사가 있을 뿐 아니라 이벤트가 발생했는지를 확인하기 위해서 넓은 
 *   범위의 소켓테이블을 검사해야 했다. select(2)라면 최악의 경우 단 
 *   하나의 이벤트가 어느 소켓에서 발생했는지 확인하기 위해서 1024개의 
 *   이벤트 테이블을 몽땅 검색해야 하는 비효율을 감수해야 한다.
 *
 *   이러한 문제를 해결하기 위해서 kqueue, RTS, epoll과 같은 이벤트 통지
 *   기반의 입출력 처리 도구가 개발되었다.
 *
 *   epoll은 이름에서 알 수 있듯이 좀 더 빠르고 효율적으로 입출력 이벤트의
 *   처리가 가능하도록 poll(2)을 확장시킨 도구이다. 이러한 성능의 향상은
 *   Edge Trigger(ET)과 Level Trigger(LT) 인터페이스를 채용해서 관심있어
 *   하는 파일을 좀더 효과적으로 관리 할 수 있도록 함으로써 이루어 졌다.
 *
 */

/*! \brief EPOLL filter 마스크 */
#define EPOLLMASK				\
    (EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP|EPOLLRDHUP|EPOLLONESHOT|EPOLLET)
#define EVENTIOMASK			    (EPOLLMASK|EPOLLBAND|EPOLLALIVE)

static ssize_t io_read( uintptr_t fd, void *buffer, size_t size)
{
	register int len = size,
			     n;

	do {
		while ((n = read( fd, buffer, len)) < 0)
			switch (errno)
			{
				case EAGAIN: break;
				/* case EINTR : break; */
				default    : return -1;
			} buffer = ((char *)buffer) + n;
	} while ((len -= n) > 0);
	return size;
}

static ssize_t io_write( uintptr_t fd, const void *buffer, size_t size)
{
	register int n = 0;
	size_t l = size;

	do {
#define WRITE_SIZE			16000
		buffer = (const void *)(((char *)buffer) + n);
		while ((n = write( fd, buffer, (size > WRITE_SIZE) ? WRITE_SIZE: size)) < 0)
			switch (errno)
			{
				/* case EINTR : */
				case EAGAIN: break;
				case ENOSPC: return 0;
				default    : return -1;
			}
		//fprintf( stderr, "%s: fd = %d, %d bytes [%d]\n", __func__, fd, size, n);
	} while ((size -= n) > 0);
	return l;
}


struct eio_adapter {
	int epfd;
	struct {
		int nactive, nfiles;		/* 활성화 수, 최대 설정 가능 수 */
		struct {
			int nwait;
			struct epoll_event *e;
		}; struct list_head l_files;
	}; void *app_private; LOCK_T lock;
};


#define efile_entry(ptr)	list_entry(ptr,struct eio_file,l_file)
struct eio_file {
	int fd;
	struct {
		uint32_t action;		/* epoll() 이벤트 */
		struct {
			char *buffer;					/* cache buffer */
			struct {
				uint64_t ncache;			/* cache arg */
				struct cache *i,
							 *o;
			}; ecache_combine_t combine;	/* cache 조합 f */
			utime_t futime;                 /* flush 처리 시간 */
		} ca; struct eio_adapter *adapter;	/* 연결 adapter */
		utime_t lutime;		/* 마지막 EPOLLIN 발생 시간 */
	}; void *app_private; struct list_head l_file;
};


static struct eventio {
	struct eio_file *f_begin;
	struct {
		int nfiles;
		struct list_head l_files,
                         l_shutdown;
	}; LOCK_T lock;
} *ESYS;


#include <sys/resource.h>
#include <unistd.h>

static int f_rlimit( int nfiles)
{
	struct rlimit r;

	if (getrlimit( RLIMIT_NOFILE, &r) < 0)
		return -1;
	else {
		;
	} return ((nfiles <= 0) || (nfiles > r.rlim_cur)) ? r.rlim_cur: nfiles;
}

#include <malloc.h>
#include <sys/socket.h>

/*! \brief eventio 초기화
 *
 * 		eventio를 관리하기 위한 내부 리소스를 초기화 하는 작업을 수행한다.
 * 		만약, nfiles == 0 일 경우 시스템에서 기본적으로 개방 가능한 최대
 * 		파일수를 가진다. (ulimit -n으로 확인)
 *
 *  \param nfiles		최대 관리 파일수
 *  \retval				성공 여부
 */
int eio_init( int nfiles)
{
	if (((nfiles = f_rlimit( nfiles)) < 0) ||
			!(ESYS = (struct eventio *)calloc( 1,
					sizeof(*ESYS) + (nfiles * sizeof(struct eio_file)))))
		return -1;
	else {
		INIT_LIST_HEAD( &ESYS->l_shutdown);
		INIT_LIST_HEAD( &ESYS->l_files); {
			struct eio_file *f_begin = 
				(ESYS->f_begin = (struct eio_file *)(ESYS + 1)),
							*f_end = f_begin + (ESYS->nfiles = nfiles);

			do {
				f_begin->fd = -1;
				list_add_tail( &f_begin->l_file, &ESYS->l_files);
			} while ((++f_begin) != f_end);
		} INIT_LOCK( &ESYS->lock);
	} return nfiles;
}

/*! \brief event adapter를 생성한다.
 *
 * 		epoll과 연결되어 지는 파일의 그룹을 생성하는 함수로, 모든 파일은
 * 		adapter에 의해 관리 되어 진다.
 *
 *  \param nfiles	adapter에서 관리하고자 하는 최대 파일수 (0일 경우 전체)
 *  \param arg		adapter에 저장할 user data
 *  \retval			생성된 adapter 주소
 */
struct eio_adapter *eadapter_create( int nfiles, void *arg)
{
	if (!ESYS)
		errno = EINVAL;
	else {
		struct eio_adapter *r;

		(((nfiles > ESYS->nfiles) || (nfiles <= 0)) ? nfiles = ESYS->nfiles: 0);
		if (!(r = (struct eio_adapter *)calloc( 1,
						sizeof(*r) + (sizeof(struct epoll_event) * nfiles))))
			;
		else {
			if ((r->epfd = epoll_create1(0)) < 0)
				;
			else {
				r->nfiles = nfiles;
				INIT_LOCK( &r->lock);
				r->e = (struct epoll_event *)(r + 1); {
					INIT_LIST_HEAD( &r->l_files);

					r->app_private = arg;
				} return r;
			} free( r);
		}
	} return NULL;
}

int eadapter_atfork(struct eio_adapter *adapter,
					void (*alive_atfork)(struct eio_file *, void *), void *udata)
{
	close(adapter->epfd);
	if ((adapter->epfd = epoll_create(adapter->nfiles)) < 0)
		return -1;
	else {
		utime_t timeNow = utimeNow(NULL);
		int n = 0;

		ENTER_LOCK( &adapter->lock); {
			struct eio_file *f;
			struct list_head *pos, *next;

			list_for_each_safe(pos, next, &adapter->l_files)
			{
				if ((f = efile_entry(pos))->fd < 0)
					;
				else {
					struct epoll_event e = {
							.events = (f->action & EPOLLMASK) | EPOLLWAKEUP,
							.data.ptr = (void *)f
					};

					if (epoll_ctl( adapter->epfd, EPOLL_CTL_ADD, f->fd, &e) < 0)
						return -1;
					else {
						/* touch */{ size_t nwait = 0; ioctl( f->fd, FIONREAD, &nwait); }

						f->lutime = timeNow;
					    f->ca.futime = 0;
						if (alive_atfork)
						{
							LEAVE_LOCK(&adapter->lock); {
								alive_atfork(f, udata);
							} ENTER_LOCK(&adapter->lock);
						}
					}
					n++;
				}
			} LEAVE_LOCK( &adapter->lock);
		} return n;
	}
}

/*! \brief adapter를 제거한다.
 *
 * 		사용을 마친 adapter의 리소스를 해제한다.
 * 		만약, 관리중인 파일이 존재한다면 모두 반환한다. 이때 파일은 close() 처리 되지 않는다.
 *
 *  \param adapter	eadapter_create()에서 생성된 adapter 주소
 *  \retval			성공 여부
 */
int eadapter_destroy( struct eio_adapter *adapter)
{
	int n = 0;

	/* eadapter_shutdown(adapter); */
	ENTER_LOCK( &adapter->lock); {
		close(adapter->epfd);
		while (list_empty(&adapter->l_files) == false)
		{
			struct eio_file *f = efile_entry(adapter->l_files.next);

			ENTER_LOCK(&ESYS->lock); {
				if (f->fd < 0)
					;
				else {
					f->adapter = NULL; {
						++n;
						close(f->fd);
					} list_move_tail(&f->l_file, &ESYS->l_files);

					f->fd = -1;
					f->lutime = utimeNow(NULL);
				}
			} LEAVE_LOCK(&ESYS->lock);
		}

		free( adapter);
	}
	return n;
}

/*! \brief adapter에 설정된 user data를 얻는다.
 *
 * 		eadapter_create() 함수에서 연결된 user data 저장 주소를 얻는다.
 *
 *  \param adapter	eadapter_create()에서 생성된 adapter 주소
 *  \retval			user data 연결 주소
 */
void **eadapter_udata( struct eio_adapter *adapter) { 
	return &adapter->app_private; }

/*! \brief adapter에 할당된 최대 파일 수를 얻는다.
 *
 * 		eadapter_create() 함수에서 초기화된 최대 관리 파일 수를 얻는다.
 *
 * 	\param adapter	eadapter_create()에서 생성된 adapter 주소 (NULL일 경우 전체)
 * 	\retval			관리 가능 파일의 수
 */
int eadapter_nfiles( struct eio_adapter *adapter) { 
	return (adapter ? adapter->nfiles: (ESYS ? ESYS->nfiles: -1)); }

/*! \brief adapter에서 관리중인 파일의 수를 얻는다.
 *
 * 		efile_setadapter()를 통해 연결된 관리 파일의 수를 얻는다.
 *
 * 	\param adapter	eadapter_create()에서 생성된 adapter 주소
 * 	\retval			관리 중인 파일의 수
 */
int eadapter_nactive( struct eio_adapter *adapter) { return adapter->nactive; }

int eadapter_shutdown(struct eio_adapter *adapter)
{
	int n = 0;

	ENTER_LOCK( &adapter->lock); {
		while (list_empty(&adapter->l_files) == false)
		{
			struct eio_file *f = efile_entry(adapter->l_files.next);

			ENTER_LOCK(&ESYS->lock); {
				if (f->fd < 0)
					;
				else {
					f->adapter = NULL; {
						++n;
						close(f->fd);
					} list_move_tail(&f->l_file, &ESYS->l_files);

					f->fd = -1;
					f->lutime = utimeNow(NULL);
				}
			} LEAVE_LOCK(&ESYS->lock);
		}
	} LEAVE_LOCK(&adapter->lock);
	return n;
}

/*! \brief adapter에서 관리중인 파일의 사용 여부를 검증한다.
 *
 * 		adapter에 연결된 파일중에 설정 event 속성이 EPOOLALIVE로 설정된 
 * 		파일을 대상으로 하여, 파일의 사용 여부를 검증하고 timeout 이상
 * 		사용되지 않은 파일을 반환한다.
 *
 * 	\param[in] adapter	eadapter_create()에서 생성된 adapter 주소
 * 	\param[out] fs		반환될 파일 목록 저장 주소
 * 	\param[in] max		fs에 최대 저장 가능한 파일 수
 * 	\param[in] timeout	timeout(msec) 값
 * 	\retval			fs 에 저장된 파일 수
 */
int eadapter_alive( struct eio_adapter *adapter, 
							struct eio_file **fs, int max, uint32_t timeout)
{
	register int n = 0;
    utime_t o = utimeNow(NULL) - ((int32_t)timeout);
    int     off = (((int32_t)timeout) < 0) ? (EPOLLHUP|EPOLLRDHUP): EPOLLALIVE;

	ENTER_LOCK( &adapter->lock); {
        struct eio_file *f;
        struct list_head *pos;

        list_for_each_prev(pos, &adapter->l_files)
        {
            if ((f = efile_entry(pos))->lutime > o)
_gB:		    break;
			else {
#if 0
				fprintf( stderr, " %s: check - %d (%x)\n", __func__, f->fd, f->action);
#endif
                if ((f->fd < 0) || ((f->action & off) == 0))
					;
				else {
                    // fprintf( stderr, " * %d: %s - %d [%ld]\n", getpid(), __func__, f->fd, o - f->lutime);
					fs[n] = f;
					if ((++n) >= max) goto _gB;
				}
			}
		} LEAVE_LOCK( &adapter->lock);
    } return n;
}

/*! \brief adapter에서 관리중인 파일중에 cache out된 파일의 데이터를 flush한다.
 *
 * 		adapter에 연결된 파일중에 output cache를 사용중인 파일을 찾아
 * 		데이터를 기록하도록 처리한다.
 *
 * 	\param[in] adapter	eadapter_create()에서 생성된 adapter 주소
 * 	\param[in] timeout	timeout(msec) 값
 * 	\retval			처리된 파일수
 */
int eadapter_flush( struct eio_adapter *adapter, struct eio_file **fs, int max, int timeout)
{
	register int n = 0;
	utime_t o = utimeNow(NULL) - timeout;

	ENTER_LOCK( &adapter->lock); {
		struct list_head *l;

		list_for_each( l, &adapter->l_files)
		{
			struct eio_file *f = efile_entry(l);

			if ((f->ca.o == NULL) || (f->ca.futime >= o));
			else if (fs != NULL)
            {
                fs[n] = f;
                if ((++n) >= max) break;
            }
			else if (efile_flush( f, timeout) > 0) n++;
		} LEAVE_LOCK( &adapter->lock);
	} return n;
}


/*! \brief 파일을 adapter와 연결하기 위한 리소스를 할당 받는다.
 *
 * 		파일(fd)를 관리하기 위한 초기화 함수로, 기본 설정 이벤트(events)와
 * 		해당 파일에 연결된 user data를 설정한다.
 *
 * 	\param fd		파일 핸들
 * 	\param events	기본 설정하고자 하는 EPOOL 이벤트 설정 값
 * 	\param arg		user data
 * 	\retval			내부에 저장된 관리 주소
 */
struct eio_file *efile_open( int fd, uint32_t events, void *arg)
{
	ENTER_LOCK( &ESYS->lock);
	if ((fd < 0) || list_empty( &ESYS->l_files))
		LEAVE_LOCK( &ESYS->lock);
	else {
		struct eio_file *f = efile_entry(ESYS->l_files.next);

        f->ca.futime = 0;
        if (f->ca.buffer == NULL)
            ;
        else {
            cache_destroy(f->ca.i);
            cache_destroy(f->ca.o);

            f->ca.i = f->ca.o = NULL;

            free(f->ca.buffer);
            f->ca.buffer = NULL;
        } list_del_init(&f->l_file);

        LEAVE_LOCK( &ESYS->lock); {
	        f->lutime = utimeNow(NULL);
            f->fd = fd; {
                f->action = EPOLLHUP|EPOLLRDHUP|EPOLLERR|(events & EVENTIOMASK);
                f->adapter = NULL;

                f->ca.combine = NULL;
                f->ca.ncache = 0LL;
            } f->app_private = arg;
        } return f;
	} return NULL;
}

/*! \brief efile의 고유 번호를 얻는다.
 *
 * 		efile_open()에 의해 연결된 fd에 대한 고유 번호를 확인하고자 할때
 * 		사용하는 함수로, 고유 번호는 0 ~ nfiles - 1 까지 이며, nfiles는
 * 		eio_init()시에 설정되는 값이다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		고유 번호
 */
int efile_regno  ( struct eio_file *f) { return f - ESYS->f_begin; }

/*! \brief efile의 고유 번호를 efile ptr로 변경한다.
 *
 * 		efile_regno()로 변환된 고유 번호를 efile로 변환한다.
 *
 * 	\param no	efile_regno()에서 얻은 고유 번호
 * 	\retval		efile
 */
struct eio_file *efile_get( int no) { return ESYS->f_begin + no; }

/*! \brief efile에 할당된 fd를 얻는다.
 *
 * 		efile_open()에 의해 연결된 fd를 얻는다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		fd
 */
int efile_fileno ( struct eio_file *f) { return f ? f->fd: -1; }

/*! \brief efile에 저장된 user data를 얻는다.
 *
 * 		efile_open()에 의해 지정된 user data의 저장 주소를 얻는다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		user data 저장 주소
 */
void **efile_udata  ( struct eio_file *f) { return &f->app_private; }

/*! \brief efile의 마지막 이벤트 발생 시간을 얻는다. (msec)
 *
 * 		efile에 연결된 fd의 마지막 이벤트가 발생된 시간을 나타내며,
 * 		해당 값은 eadapter_alive()함수에 의해 timeout 되는 efile을 검출하는
 * 		기본 값이 된다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		마지막 이벤트 발생 시간
 */
utime_t efile_lutime ( struct eio_file *f) { return f->lutime; }

/*! \brief efile의 마지막 버퍼의 flush 시간을 얻는다. (msec)
 *
 * 		efile_setcache()로 설정된 efile에 대해, efile_flush()를 수행한 시간을 반환한다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		마지막 이벤트 발생 시간
 */
utime_t efile_futime ( struct eio_file *f) { return f->ca.futime; }

/*! \brief efile에 연결된 adapter를 얻는다.
 *
 * 		efile_setadapter()에 의해 지정된 adapter를 얻는다.
 *
 *  \param f	efile_open()에서 할당받은 주소
 *  \retval		adapter 주소
 */
struct eio_adapter *efile_adapter( struct eio_file *f) { return f->adapter; }

/*! \brief efile에 설정된 event 정보를 얻는다.
 *
 * 		efile_open() 또는 efile_setevent()에 의해 설정된 event 정보를 얻는다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		event 설정 값
 */
uint32_t efile_isevent( struct eio_file *f) { return f->action; }

/*! \brief efile에 지정된 cache 정보를 얻는다.
 *
 * 		efile은 입/출력에 대한 cache를 가질수 있는데, 이에 대한 설정 정보를
 * 		얻는 함수이다.
 *
 * 		설정 정보(iscache)는
 * 			0x01		입력 cache
 * 			0x02		출력 cache
 * 		의 값이 OR 되어 반환 된다.
 *
 * 	\param[in] f		efile_open()에서 할당받은 주소
 * 	\param[out] iscache	설정 cache 정보
 * 	\retval				설치된 입력 cache filter
 */
ecache_combine_t efile_iscache( struct eio_file *f, int *iscache)
{
	if (f == NULL) return 0;
	else if (iscache)
		(*iscache) = ((f->ca.i ? 0x01: 0x00) | (f->ca.o ? 0x02: 0x00));
	return f->ca.combine;
}

/*! \brief 개방된 efile을 제거하고, 사용한 리소스를 반환한다.
 *
 * 		efile의 사용을 마친 후 리소스를 반환하는 함수로, 만약 adapter와 연결
 * 		되어 있다면 해당 adapter와의 연결을 자동으로 해제 한다.
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		efile에 연결되어 사용된 fd
 */
int efile_close( struct eio_file *f)
{
	int fd;

	fd = efile_shutdown( f, false);
    ENTER_LOCK( &ESYS->lock); {
        if (f->fd < 0)
            ;
        else {
            f->fd = -1;
			f->lutime = utimeNow(NULL);
            list_move_tail( &f->l_file, &ESYS->l_files);
#if 0
            /**** efile_transfer() 에서 crash 발생 가능성 존재 ****/
            if (f->ca.buffer == NULL)
                ;
            else {
                f->ca.i = 
                    f->ca.o = NULL;

                free(f->ca.buffer);
                f->ca.buffer = NULL;
            }
#endif
        }
    } LEAVE_LOCK( &ESYS->lock);
	return fd;
}

/*! \brief shutdown된 파일을 얻는다.
 *
 *      efile_shutdown()을 통해 제거된 파일을 검증하여, 제거할지를
 *      판단하게 된다. 소멸한지 timeout이상된 파일을 반환한다.
 *
 * 	\param[out] fs		반환될 파일 목록 저장 주소
 * 	\param[in] max		fs에 최대 저장 가능한 파일 수
 * 	\param[in] timeout	timeout(msec) 값
 * 	\retval			fs 에 저장된 파일 수
 */
int efile_refuse(struct eio_file **fs, int max, uint32_t timeout)
{
	register int n = 0;
	utime_t t = utimeNow( NULL);

	ENTER_LOCK( &ESYS->lock); {
		if (list_empty( &ESYS->l_shutdown))
			;
		else {
			struct eio_file *f;
			utime_t o = t - ((int32_t)timeout);

_gW:	    if ((f = efile_entry( ESYS->l_shutdown.prev))->lutime > o)
                ;
			else {
				f->lutime = t;
				list_move( &f->l_file, &ESYS->l_shutdown);
#if 0
				fprintf( stderr, " %s: check - %d (%x)\n", __func__, f->fd, f->action);
#endif
				fs[n] = f;
				if ((++n) < max) goto _gW;
			}
		} LEAVE_LOCK( &ESYS->lock);
	} return n;
}


/*! \brief efile_close()된 파일의 캐싱을 리셋한다.
 *
 *      efile_close()을 통해 제거된 파일의 메모리를 반환한다.
 *
 * 	\param[in] timeout	timeout(msec) 값
 * 	\retval			cleare파일의 갯수
 */
int efile_purge(uint32_t timeout)
{
	register int n = 0;
	utime_t t = utimeNow( NULL);

	ENTER_LOCK( &ESYS->lock); {
		if (list_empty( &ESYS->l_files))
			;
		else {
            struct list_head *l;
			utime_t o = t - ((int32_t)timeout);

            list_for_each_prev(l, &ESYS->l_files)
            {
                struct eio_file *f = efile_entry(l);

                if (f->lutime == 0)
                    break;
                else {
                    if (f->lutime <= o)
                    {
                        if (f->ca.buffer == NULL)
                            ;
                        else {
                            cache_destroy(f->ca.i);
                            cache_destroy(f->ca.o);
                            f->ca.i = f->ca.o = NULL;

                            free(f->ca.buffer);
                            f->ca.buffer = NULL;
                        }

                        f->lutime = 0;
                        n++;
                    }
                }
			}
		} LEAVE_LOCK( &ESYS->lock);
	} return n;
}



/*! \brief adapter와의 연결을 해제 한다.
 *
 * 		efile_setadapter()에 의해 연결된 adapter와의 연결을 해제할 경우
 * 		사용 하는 함수
 *
 * 	\param f	efile_open()에서 할당받은 주소
 * 	\retval		efile에서 관리 중인 fd
 */
int efile_shutdown( struct eio_file *f, bool refuse)
{
	struct eio_adapter *adapter;

	ENTER_LOCK( &ESYS->lock); {
        if ((adapter = f->adapter) != NULL)
            f->adapter = NULL;
    } LEAVE_LOCK(&ESYS->lock);

    if (adapter == NULL)
        ;
    else {
	    ENTER_LOCK( &adapter->lock); {
            static struct epoll_event e;
#if 0
            fprintf( stdout, "%s: delete - %d\n", __func__, f->fd);
            fflush(stdout);
#endif
            --adapter->nactive;

            f->lutime = utimeNow(NULL);
            if (refuse == false)
                list_del_init( &f->l_file);
            else {
                ENTER_LOCK( &ESYS->lock); {
                    list_move_tail( &f->l_file, &ESYS->l_shutdown);
                } LEAVE_LOCK(&ESYS->lock);
            }

            epoll_ctl( adapter->epfd, EPOLL_CTL_DEL, f->fd, &e);
		} LEAVE_LOCK( &adapter->lock);
    }

	return f->fd;
}


static int ecache_rawcombine( struct eio_file *f, struct cache *c, uint64_t *nc)
{
	return cache_size( c, 0);
}

/*! \brief efile에 cache를 설정한다.
 *
 * 		efile은 입/출력 cache를 가질수 있는데 그에 대한 설정을 처리하는 함수로
 * 		입력의 경우, 발생된 데이터에서 실제 처리 데이터를 분리하는 
 * 		ecache_combine_t 라는 callback 함수를 가지고 있다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param in_b		입력 버퍼 크기(바이트)
 * 	\param fc		입력 버퍼에 대한 패킷 조합 함수
 * 	\param narg		fc에 연결되는 추가 param
 * 	\param out_b	출력 버퍼 크기(바이트)
 * 	\retval			성공 여부
 */
int efile_setcache( struct eio_file *f, 
		int in_b, ecache_combine_t fc, uint64_t narg, int out_b)
{
    if ((in_b + out_b) <= 0)
        ;
    else {
        char *buffer;

        if ((f->fd < 0) || f->ca.i || f->ca.o ||
                !(buffer = (char *)realloc( f->ca.buffer, cache_sizeof(in_b) + cache_sizeof(out_b))))
            return -1;
        else {
            f->ca.ncache = narg;
            f->ca.combine = fc ? fc: ecache_rawcombine;
        }

        f->ca.buffer = buffer;
        f->ca.i = cache_init( in_b, &buffer);
        f->ca.o = cache_init( out_b, &buffer);
	} return 0;
}

struct cache *efile_rcache( struct eio_file *f) { return f->ca.i; }
struct cache *efile_wcache( struct eio_file *f) { return f->ca.o; }

/*! \brief efile에 adapter를 지정한다.
 *
 * 		efile_open()으로 연결된 fd가 실제로 사용되기 위해서는 adapter에 연결
 * 		되어야 하는데, 이 함수는 efile과 adapter를 연결하는 함수이다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param adapter	eadapter_create()에서 할당받은 주소
 * 	\retval			성공 여부
 */
int efile_setadapter( struct eio_file *f, struct eio_adapter *adapter)
{
	int r = -1;

	if ((f == NULL) || f->adapter)
		errno = EINVAL;
	else {
		ENTER_LOCK( &adapter->lock);
		if ((r = (adapter->nactive + 1)) >= adapter->nfiles)
			errno = ENOSPC;
		else {
			struct epoll_event e = {
				.events = (f->action & EPOLLMASK) | EPOLLWAKEUP,
				.data.ptr = (void *)f
			};

#if 0
			fprintf( stdout, " %s: add %d fd, 0x%x events\n", __func__, f->fd, e.events);
            fflush(stdout);
#endif
			if ((r = epoll_ctl( adapter->epfd, EPOLL_CTL_ADD, f->fd, &e)) < 0)
                ;
            else {
				/* touch */{ size_t nwait = 0; ioctl( f->fd, FIONREAD, &nwait); }

                (f->adapter = adapter)->nactive++;
                ENTER_LOCK( &ESYS->lock); {
                    list_move( &f->l_file, &adapter->l_files);
                } LEAVE_LOCK( &ESYS->lock);
            }

            f->lutime = utimeNow(NULL);
        } LEAVE_LOCK( &adapter->lock);
	} return r;
}


static void efile_reset( struct eio_file *f, struct eio_adapter *adapter)
{
	f->lutime = utimeNow(NULL);
	ENTER_LOCK( &adapter->lock); {
		list_move( &f->l_file, &adapter->l_files);
	} LEAVE_LOCK( &adapter->lock);
}

/*! \brief efile의 마지막 이벤트 발생 시간을 갱신한다.
 *
 * 		efile의 마지막 이벤트 발생 시간을 강제로 갱신 시키는 함수로,
 * 		만약, EPOOLALIVE 이벤트를 가지고 있는 efile이라면 eadapter_alive()에
 * 		의해 efile이 추출되는 것에 대해 제외하는 역활을 수행하게 된다.
 *
 * 	\param f	efile_open()에서 할당 받은 주소
 * 	\retval		성공여부
 */
int efile_setalive( struct eio_file *f)
{
	struct eio_adapter *adapter;

	if (!(adapter = f->adapter))
		errno = EINVAL;
	else {
		efile_reset( f, adapter); return 0;
	} return -1;
}

/*! \brief efile에 설정된 이벤트를 변경한다.
 *
 * 		efile의 이벤트는 epoll 이벤트와 eventio 이벤트로 나누어 지며, 이러한
 * 		이벤트를 변경하는 함수이다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param events	설정 이벤트
 * 	\retval			성공 여부
 */
#include <string.h>

int efile_setevent( struct eio_file *f, uint32_t events)
{
	struct eio_adapter *adapter;
    int r = -1;

	if (!(adapter = f->adapter))
		errno = EINVAL;
	else {
        uint32_t r_events = (events == 0)
				? f->action
				: ((f->action & ~EVENTIOMASK)|(events & EVENTIOMASK));

		struct epoll_event e = {
			.events = (r_events & EPOLLMASK) | EPOLLWAKEUP,
			.data.ptr = (void *)f
		};

#if 0
        fprintf(stdout, "%s: %d modify - %d 0x%x\n", __func__, getpid(), f->fd, e.events);
        fflush(stdout);
#endif
#if 0
fprintf(stderr, " * %d %s:%d - fd: %d event: 0x%x\n", getpid(), __FILE__, __LINE__, f->fd, e.events);
#endif
        if ((r = epoll_ctl( adapter->epfd, EPOLL_CTL_MOD, f->fd, &e)) < 0)
            ;
        else {
            /* touch */{ size_t nwait = 0; ioctl( f->fd, FIONREAD, &nwait); }
            {
                f->action = r_events;
                efile_reset( f, adapter);
            }
        }

        f->lutime = utimeNow(NULL);
    } return r;
}


static int cache_flush( int fd, struct cache *c, int *r)
{
    int __r; 
	int n;

#define DEF_NFLUSH						32000	/* 32K */
    if (r == NULL) r = &__r;
	enter_cachelock(c); {
		if ((n = c->len) == 0)
			(*r) = 0;
		else {
_gW:	    switch ((*r) = cache_fwrite( c, fd,
						io_write, (c->len >= DEF_NFLUSH) ? DEF_NFLUSH: c->len))
			{
				default: if (c->len) goto _gW;
				case  0: errno = EAGAIN; (*r) = -1;
				case -1: break;
			}
		} leave_cachelock(c); 
	} return n - c->len;
}


#include <sys/poll.h>

static int pollfd_wait( int fd, short events, int timeout)
{
	struct pollfd e = {
		.fd = fd,
		.events = POLLERR | POLLHUP | events,
		.revents = 0
	};

	while ((fd = poll( &e, 1, timeout)) < 0)
		switch (errno)
		{
			case EAGAIN:
			/* case EINTR : */break;
			default    : return -1;
		}

	return (fd > 0) ? e.revents: 0;
}

/*! \brief efile 출력 cache를 강제로 비운다.
 *
 * 		해당 함수는 efile_setcache()를 통해 출력 cache가 설정된 상태에서 사용
 * 		되어지는 함수로, 출력 cache에 저장된 데이터를 강제로 fd에 기록하는
 * 		역활을 수행한다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param timeout	데이터를 비우기 위한 대기 시간
 * 	\retval			비워진 데이터 크기
 */
int efile_flush( struct eio_file *f, int timeout)
{
	register int n = 0;

	if (f->ca.o == NULL)
		;
	else {
        f->ca.futime = utimeNow(NULL);
        if (timeout > 0)
        {
            int r;

_gD:	    n += cache_flush( f->fd, f->ca.o, &r);
		    if (r < 0)
			    switch (errno)
			    {
				    case EAGAIN:
_gW:				    if ((r = pollfd_wait( f->fd, POLLOUT, timeout)) < 0)
				    default:n = -1;
					    else {
						    if (r & POLLOUT)
							    goto _gD;
					    } break;
			    }
		    else if (r > 0) goto _gW;
        }
	} return n;
}

/*! \brief adapter에서 발생되는 이벤트를 대기한다.
 *
 * 		eadapter_create()에 의해 연결된 epoll로 부터 이벤트를 추출하고, 
 * 		분류하는 역활을 하는 함수
 *
 * 	\param[in] adapter		eadapter_create()에서 할당된 주소
 * 	\param[out] r			발생된 이벤트 정보가, efile위치를 저장
 *	\param[out] nwait		efile에서 발생된 데이터 수
 *	\param[in] timeout		이벤트 추출 대기 시간
 *
 *	\retval EPOLLIDLE	timeout 발생
 *	\retval EPOLLRESET	발생 이벤트 처리 완료
 *	\retval EPOLLHUP	f에 EPOLLHUP 발생
 *	\retval EPOLLBAND	EPOLLBAND설정 된 f에 이벤트 발생
 *	\retval EPOLLCACHE	cache설정된 fd에 데이터 발생(nwait = 데이터 크기)
 *	\retval EPOLLRAW	cache설정 되지 않은 f에 데이터 발생(nwait = 데이터 크기)
 *	\retval EPOLLONESHOT	EPOLLONESHOT설정된 f에 이벤트 처리 마침
 */
int eadapter_wait( struct eio_adapter *adapter, struct eadapter_event *r, size_t *nwait, int timeout)
{
	struct epoll_event *e;
	int __x = 0;

_gD:if (adapter->nwait > 0) ;
	else if ((--adapter->nwait) == -1) return EPOLLRESET;
	else {
#if 0
		fprintf(stderr, "%d %s: nfiles = %d, %d msec\n", getpid(), __func__, adapter->nfiles, timeout);
#endif
_gW:    if ((adapter->nwait = epoll_pwait( adapter->epfd, adapter->e, adapter->nfiles, timeout, NULL)) < 0)
			switch (errno)
			{
				case EINTR:
				case EAGAIN: goto _gW;
				default    : return -1;
			}
		else if (adapter->nwait == 0) return EPOLLIDLE;
#if 0
		fprintf(stderr, "%d %s: check - %d nwait\n", getpid(), __func__, adapter->nwait);
#endif
	}
    
    r->f = (struct eio_file *)(e = (adapter->e + (--adapter->nwait)))->data.ptr;
    r->events = e->events | (r->f->action & (EPOLLBAND|EPOLLONESHOT));

#if 0
	fprintf( stderr, "%d %s:fd = %d, result[0x%x:0x%0x]\n", getpid(), __func__, r->f->fd, e->events, r->events);
#endif
    (*nwait) = r->events;

	e->events &= ~(EPOLLIN|EPOLLPRI|EPOLLOUT|EPOLLONESHOT);
	if (r->events & EPOLLERR)
_gE:    return EPOLLERR;
	else if ((r->events & (EPOLLIN|EPOLLPRI)))
		efile_reset( r->f, adapter);
	else {
		if (r->events & (EPOLLHUP|EPOLLRDHUP))
_gC:       	return EPOLLHUP;
		else if (r->events & EPOLLOUT) ; /* return EPOLLOUT; */
		else if (r->f->action & EPOLLONESHOT) return EPOLLONESHOT;
		else goto _gD; // dead code...
	}

#if 0
	fprintf(stderr, "%d %s: fd = %d, result[%x]\n", getpid(), __func__, r->f->fd, r->events);
#endif
	if (r->f->action & EPOLLBAND) return EPOLLBAND;
	else if (!(r->events & (EPOLLIN | EPOLLPRI))) return EPOLLOUT;
	else if (ioctl( r->f->fd, FIONREAD, nwait) < 0)
		switch (errno)
		{
			// Not support FIONREAD
			case ENOTTY: (*nwait) = 0;
            {
                __x = 0;
            } goto _gR;
			default: goto _gE;
		}
	else {
#if 0
		fprintf( stderr, "%d %s: FIONREAD - %d fd, %ld bytes [%p]\n", getpid(), __func__, r->f->fd, (*nwait), r->f->ca.i);
#endif
		if ((*nwait) == 0) goto _gC;

		adapter->nwait += (__x = 1);   // 동일 이벤트를 다음에 처리 예약
_gR:	if (r->f->ca.i == NULL) return EPOLLRAW;
		{
			struct cache *ca = r->f->ca.i;

			enter_cachelock( ca); {
				int l = cache_left(ca);

#if 0
				fprintf( stderr, "%d %s: fd = %d, before cache = %d\n", getpid(), __func__, r->f->fd, l);
#endif
				if (l > 0)
					;
				else {
					adapter->nwait -= __x;
					leave_cachelock(ca); {
						errno = ENOBUFS;
					} goto _gE;
				} if (((*nwait) == 0) || ((*nwait) > l)) (*nwait) = l;

				cache_fread( ca, r->f->fd, io_read, (*nwait));
				(*nwait) = cache_size(ca, 0);
			} leave_cachelock(ca);
		}
#if 0
		fprintf( stderr, "%d %s: fd = %d, after cache = %ld\n", getpid(), __func__, r->f->fd, (*nwait));
#endif
	}
	return EPOLLCACHE;
}

/*! \brief adapter에서 발생되는 이벤트를 대기한다.
 *
 *
 * 		eadapter_create()에 의해 연결된 epoll로 부터 이벤트를 추출하고, 
 * 		분류하는 역활을 하는 함수
 *
 * 	\param[in] adapter		eadapter_create()에서 할당된 주소
 * 	\param[out] f			이벤트가 발생된 efile을 저장하는 주소
 *	\param[out] nwait		efile에서 발생된 데이터 수
 *	\param[in] timeout		이벤트 추출 대기 시간
 *
 *	\retval EPOLLIDLE	timeout 발생
 *	\retval EPOLLRESET	발생 이벤트 처리 완료
 *	\retval EPOLLHUP	f에 EPOLLHUP 발생
 *	\retval EPOLLBAND	EPOLLBAND설정 된 f에 이벤트 발생
 *	\retval EPOLLCACHE	cache설정된 fd에 데이터 발생(nwait = 데이터 크기)
 *	\retval EPOLLRAW	cache설정 되지 않은 f에 데이터 발생(nwait = 데이터 크기)
 *	\retval EPOLLONESHOT	EPOLLONESHOT설정된 f에 이벤트 처리 마침
 */
int eadapter_dispatch( struct eio_adapter *adapter, struct eio_file **f, size_t *nwait, int timeout)
{
    struct eadapter_event e;
    int r;

    (*nwait) = 0;
_gW:switch (r = eadapter_wait( adapter, &e, nwait, timeout))
    {
        case EPOLLRESET:
        case EPOLLIDLE: break;
        default: (*f) = e.f; if ((e.events & EPOLLOUT) && e.f->ca.o)
        {
            e.f->ca.futime = utimeNow(NULL);
            cache_flush( e.f->fd, e.f->ca.o, NULL);
        }

        if (r == EPOLLCACHE)
        {
	        enter_cachelock(e.f->ca.i); {
                (*nwait) = e.f->ca.combine( e.f, e.f->ca.i, &e.f->ca.ncache);
                if ((*nwait) <= cache_size(e.f->ca.i, 0))
                    ;
                else {
                    errno = ENOBUFS;
                    (*nwait) = -1;
                }
            } leave_cachelock(e.f->ca.i);

            if ((*nwait) < 0) return EPOLLERR;
            else if ((*nwait) == 0)
			{
                if (e.f->action & EPOLLONESHOT) return ((*nwait) = EPOLLONESHOT);
                else if (adapter->nwait == 0) return ((*nwait) = EPOLLRESET);

                goto _gW;
            }
        } break;
    }

    return r;
}

/*! \brief cache설정된 efile에서 읽을수 있는 데이터를 얻어온다.
 *
 * 		efile_setcache()에 의해 입력 cache가 설정된 efile에 저장된 데이터를
 * 		읽는다. 만약, cache가 지정되지 않았다면 read()함수를 통해 데이터를
 * 		읽는다.
 *
 * 	\param[in] f		efile_open()에서 할당받은 주소
 * 	\param[out] buffer	데이터를 저장할 버퍼
 * 	\param[in] len		읽어 올 데이터 크기
 * 	\retval			저장한 데이터 크기
 */
ssize_t efile_ready( struct eio_file *f, void **buffer, size_t len)
{
    ssize_t r = 0;

    if (f->ca.i == NULL)
        r = (!(*buffer) && !((*buffer) = malloc(len))) ? -1 : io_read( f->fd, *buffer, len);
    else {
        enter_cachelock( f->ca.i);
        if (cache_size(f->ca.i, 0) == 0)
            ;
        else {
            if ((r = f->ca.combine( f, f->ca.i, &f->ca.ncache)) <= 0)
				;
            else {
				if ((*buffer) != NULL)
				{
					if (r <= len)
						;
					else {
						errno = ENOMEM;
_gC:					r = -1;
					}
				}
				else if (!((*buffer) = malloc(r))) goto _gC;
				cache_get( f->ca.i, *buffer, r, 0);
			}
        } leave_cachelock( f->ca.i);
    } 

    return r;
}

/*! \brief cache설정된 efile에서 len만큼 데이터를 읽는다.
 *
 * 		efile_setcache()에 의해 입력 cache가 설정된 efile에 저장된 데이터를
 * 		읽는다. 만약, cache가 지정되지 않았다면 read()함수를 통해 데이터를
 * 		읽는다.
 *
 * 	\param[in] f		efile_open()에서 할당받은 주소
 * 	\param[out] buffer	데이터를 저장할 버퍼
 * 	\param[in] len		읽어 올 데이터 크기
 * 	\retval			저장한 데이터 크기
 */
ssize_t efile_read( struct eio_file *f, void *buffer, size_t len)
{
	struct cache *c;

	// fprintf( stderr, " * [DEBUG] %s:%d - %p\n", __func__, __LINE__, f->ca.i);
	if (!(c = f->ca.i))
		return io_read( f->fd, buffer, len);
	else {
		enter_cachelock( c);
		if (cache_get( f->ca.i, buffer, len, 0) < 0)
			len = -1;
		else {
			;
		} leave_cachelock( c);
	} return len;
}

/*! \brief 출력 cache가 설정된 efile에 데이터를 저장한다.
 *
 * 		efile_setcache()를 통해 출력 cache가 설정된 efile에 데이터를 저장한다.
 * 		만약, cache가 지정되지 않은 efile이라면 write()함수의 기능을 수행한다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param buffer	데이터 버퍼
 * 	\param len		buffer의 크기
 * 	\retval			저장한 데이터 크기
 */
ssize_t efile_write( struct eio_file *f, const void *buffer, size_t len)
{
	if ((buffer != NULL) && (len == 0))
		;
	else {
		struct cache *c;

		if (!(c = f->ca.o))
			return io_write( f->fd, buffer, len);
		else {
			enter_cachelock( c);
            if (buffer == NULL) len = cache_size(c, 0);
            else if (cache_put( c, buffer, len) < 0) len = -1;
			leave_cachelock( c);
		}
	} return len;
}


/*! \brief efile의 출력 cache를 외부 cacahe로 옮긴다.
 *
 *    efile에 있는 출력 cache의 데이터를 이동시켜 전송된것 처럼 처리한다.
 *    해당 처리는, 재 전송과 같은 동작을 처리하고자 할때 cache 데이터를 외부로
 *    가져가서 처리할때 유용하다.
 *
 * 	\param f		efile_open()에서 할당받은 주소
 * 	\param o	    외부 cache
 * 	\retval			저장한 데이터 크기
 */
ssize_t efile_transfer( struct eio_file *f, struct cache *o)
{
    int r = -1;

    if (f->ca.o == NULL)
        ;
    else {
        enter_cachelock( f->ca.o); {
            r = cache_move( f->ca.o, o);
        } leave_cachelock( f->ca.o);
	} return r;
}


#if 0
#include <assert.h>
int main( int argc, char *argv[])
{
	eadapter adapter;

	assert(eio_init(32) >= 0);
	if ((adapter = eadapter_create( 1024, NULL)) == NULL)
		fprintf( stderr, " not create\n");
	else {
		efile efd;
		int n = 6;

		do {
			if ((efd = efile_open( fileno( stdin), 0, NULL)) == NULL)
				fprintf( stderr, " not open\n");
			else { 
				struct list_head *l;

				fprintf( stderr, " [%d]: ", efile_regno(efd));
				if (n & 1) { efile_close( efd); fprintf( stderr, "*"); }
				list_for_each( l, &ESYS->l_files)
				{
					struct eio_file *f = efile_entry(l);

					fprintf( stderr, "%d(%d) ", efile_regno(f), f->fd);
				} fprintf( stderr, "\n");
			}
		} while ((--n) > 0);
	} return 0;
}
#endif
/* @} */
