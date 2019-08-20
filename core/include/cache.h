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
#ifndef __CACHE_H
#  define __CACHE_H

/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief 선형 형태의 데이터를 캐싱하면서 입출력을 처리
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"

/*! \addtogroup core_cache
 *  @{
 */

struct cache {
	char *r_ptr,
		 *w_ptr;
	struct {
		char *begin,
			 *end;
	} ca; int len;
};

/* cache_sizeof: c_size 버퍼를 갖는 cache의 size를 얻는다. */
EXTRN int cache_sizeof( int c_size);

/* cache_init: c_size 버퍼를 갖는 cache를 buffer로 부터 생성 */
EXTRN struct cache *cache_init( int c_size, char **buffer);
EXTRN char         *cache_destroy(struct cache *);

EXTRN void cache_clear( struct cache *);

EXTRN void enter_cachelock( struct cache *);	/* cache lock */
EXTRN int  enter_cachetrylock(struct cache *);	/* cache lock */
EXTRN void leave_cachelock( struct cache *);	/* cache unlock */


EXTRN int cache_left( struct cache *);		/* 남은 버퍼 크기 */
EXTRN int cache_size( struct cache *, int offset);	/* 읽을 버퍼 크기 */

/* buffer 처리 cache */
EXTRN int cache_put( struct cache *, const char *, int size);
EXTRN int cache_get( struct cache *, char *, int size, int offset);

EXTRN int cache_look( struct cache *, char *, int size, int offset);

EXTRN int cache_move( struct cache *i, struct cache *o); // i -> o

/** OVERLAP 함수 ************************************************************/
/* overlap cache: copy 처리를 최소화 하는 함수로, 만약 버퍼가 연속 저장 
 *     상태라면 해당 ptr를 반환하고, 그렇지 않다면 buffer에 copy한다.
 */
EXTRN char *cache_pget( struct cache *, char *, int size, int offset);
EXTRN char *cache_plook( struct cache *, char *, int size, int offset);

EXTRN char *cache_pbuffer( struct cache *, int *size, int offset);

/* write buffer에 직접 접근 처리한다. */
EXTRN char *cache_wbegin( struct cache *c, uint *left);
EXTRN int   cache_wend( struct cache *c, const char *buffer, int size);

/* read buffer에 직접 접근 처리한다. */
EXTRN char *cache_rbegin( struct cache *c, uint *left);
EXTRN int   cache_rend( struct cache *c, const char *buffer, int size);
/******************************************************************************/

/* i/o 연결 cache */
EXTRN int cache_fread( struct cache *,
                       uintptr_t fd, ssize_t (*f)( uintptr_t, void *, size_t), size_t size);
EXTRN int cache_fwrite( struct cache *,
                        uintptr_t fd, ssize_t (*f)( uintptr_t, const void *, size_t), size_t size);

/* @} */
#endif
