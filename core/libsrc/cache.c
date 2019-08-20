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
#include "cache.h"
#include "lock.h"

#include <string.h>
#include <assert.h>
#include <malloc.h>

/*! \defgroup core_cache 선형 형태의 데이터를 캐싱하면서 입출력을 처리
 *  @{
 */

int cache_sizeof( int c_size)
{
	return (sizeof(struct cache) + sizeof(LOCK_T) + c_size);
}

struct cache *cache_init( int c_size, char **buffer)
{
    char *x__buffer;
	struct cache *c;

	if (c_size <= 0) return NULL;
	else if (buffer == NULL)
	{
	    if (!(x__buffer = (char *)malloc(cache_sizeof(c_size))))
	        return NULL;
        buffer = &x__buffer;
	}
	{
		LOCK_T *lock = (LOCK_T *)(*buffer);

		INIT_LOCK( lock); 
		c = (struct cache *)(lock + 1); {
			c->ca.end = 
				(c->w_ptr = 
				 (c->r_ptr = (c->ca.begin = (char *)(c + 1)))
				) + c_size;
			c->len = 0;
		} (*buffer) = c->ca.end;
	} return c;
}


static LOCK_T *cache_getlock( struct cache *c)
{
	return (LOCK_T *)(((char *)c) - sizeof(LOCK_T));
}


char *cache_destroy(struct cache *c)
{
    LOCK_T *l = cache_getlock(c);

    DESTROY_LOCK(l);
    return (char *)l;
} 


void enter_cachelock( struct cache *c) { ENTER_LOCK( cache_getlock(c)); }
int  enter_cachetrylock(struct cache *c) { return TRY_LOCK(cache_getlock(c)); }
void leave_cachelock( struct cache *c) { LEAVE_LOCK( cache_getlock(c)); }

// -------------------------------------------------------------------
void cache_clear( struct cache *c)
{
	c->len = 0; c->r_ptr = c->w_ptr = c->ca.begin;
}


int cache_left( struct cache *c)
{
	return (int)((c->ca.end - c->ca.begin) - c->len);
}


int cache_size( struct cache *c, int offset)
{
	return (int)(c->len - offset);
}


int cache_put( struct cache *c, const char *buffer, int size)
{
	if (cache_left(c) < size) return -1;
	{
		register int npart;

        c->len += size;
		if ((npart = (int)(c->ca.end - c->w_ptr)) > size)
			memcpy( c->w_ptr, buffer, size);
		else {
			memcpy( c->w_ptr, buffer, npart);
			if ((size -= npart) > 0)
				memcpy( c->ca.begin, buffer + npart, size);
			c->w_ptr = c->ca.begin;
		} c->w_ptr += size;
	} return c->len;
}


static char *cache_offsetof( struct cache *c, int offset)
{
	register int n;

	if ((n = (int)(c->ca.end - c->r_ptr)) > offset)
		return c->r_ptr + offset;
	return c->ca.begin + (offset - n);
}


int cache_get( struct cache *c, char *buffer, int size, int offset)
{
	if (c->len < (size + offset)) return -1;
	{
		char *ptr = cache_offsetof( c, offset);

		c->len -= (size + offset);
		if ((offset = (int)(c->ca.end - ptr)) > size)
			memcpy( buffer, ptr, size);
		else {
			memcpy( buffer, ptr, offset); {
				ptr = c->ca.begin;
				size = size - offset;
			} memcpy( buffer + offset, ptr, size);
		} c->r_ptr = ptr + size;
	} return c->len;
}


int cache_move( struct cache *i, struct cache *o)
{
    int r = 0;

	while (cache_size(i, 0) > 0)
    {
        uint w_l;
        char *w_b = cache_wbegin( o, &w_l);

        if (w_b == NULL)
            break;
        else {
            uint r_l;
            char *r_b = cache_rbegin( i, &r_l);

            if (w_l > r_l) w_l = r_l;
            memcpy( w_b, r_b, w_l);

            cache_rend( i, r_b, w_l);
        }

        r += w_l;
        cache_wend( o, w_b, w_l);
    }

    // fprintf( stderr, " * DEBUG: %s %d [%d %d]\n", __func__, __LINE__, r, o->len);
    return r;
}


int cache_look( struct cache *c, char *buffer, int size, int offset)
{
	if (c->len < (offset + size)) return -1;
	{
		char *ptr = cache_offsetof( c, offset);

		if ((offset = (int)(c->ca.end - ptr)) > size)
			memcpy( buffer, ptr, size);
		else {
			memcpy( buffer, ptr, offset);
			memcpy( buffer + offset, c->ca.begin, size - offset);
		}
	} return size;
}

/*****************************************************************************/
char *cache_pget( struct cache *c, char *buffer, int size, int offset)
{
	if ((c->len < (size + offset)) || (c->len == offset)) return NULL;
	{
		char *ptr = cache_offsetof( c, offset);

		c->len -= (size + offset);
#if 0
		printf( "OFFSET: %d, %d Bytes\n", c->ca.end - ptr, size);
		dumpBuffer( c->ca.begin, c->ca.end - c->ca.begin);
#endif
		if ((offset = (int)(c->ca.end - ptr)) > size)
			c->r_ptr = ptr + size;
		else {
			if (offset == size)
				c->r_ptr = c->ca.begin;
			else {
				memcpy( buffer, ptr, offset); {
					size = size - offset;
#if 0
					printf( " COPY: %d offset, %d bytes\n", offset, size);
#endif
					memcpy( buffer + offset, c->ca.begin, size);
					c->r_ptr = c->ca.begin + size;
#if 0
					dumpBuffer( buffer, size + offset);
#endif
				} return buffer;
			}
		} return ptr;
	}
}

char *cache_plook( struct cache *c, char *buffer, int size, int offset)
{
	if ((c->len < (offset + size)) || (c->len == offset)) return NULL;
	{
		char *ptr = cache_offsetof( c, offset);

		if ((offset = (int)(c->ca.end - ptr)) >= size)
			buffer = ptr;
		else {
			memcpy( buffer, ptr, offset);
			memcpy( buffer + offset, c->ca.begin, size - offset);
		}
	} return buffer;
}

char *cache_pbuffer( struct cache *c, int *size, int offset)
{
//	fprintf( stderr, "%d %s:%d CACHE = [%ld, %ld]\n", getpid(), __FILE__, __LINE__, c->len, offset);
	if (c->len <= offset) return NULL;
	{
		char *ptr = cache_offsetof( c, offset);

		offset = c->len - offset;
		if (((*size) = (int)(c->ca.end - ptr)) > 0)
			((offset < (*size)) ? ((*size) = offset): 0);
		else {
			(*size) = offset;
			return c->ca.begin;
		} return ptr;
	}
}

/*****************************************************************************/
char *cache_wbegin( struct cache *c, uint *left)
{
	int size;

	if ((size = cache_left( c)) == 0) return NULL;
	{
		(*left) = (uint)(c->ca.end - c->w_ptr);
		if ((*left) > (uint)size)
			(*left) = size;	// 저장 가능한 크기보다 큰 경우
		assert((*left) > 0);
	} return c->w_ptr;
}

int cache_wend( struct cache *c, const char *buffer, int size)
{
	if ((buffer != c->w_ptr) ||
			((c->ca.end - c->w_ptr) < size)) return -1;
	{
		c->len += size;
		if ((c->w_ptr = c->w_ptr + size) == c->ca.end)
			c->w_ptr = c->ca.begin;
		assert(c->w_ptr < c->ca.end);
	} return 0;
}

char *cache_rbegin( struct cache *c, uint *left)
{
	if (c->len > 0)
        (*left) = (uint)(c->ca.end - c->r_ptr);
	else {
        (*left) = 0;
        return NULL;
	}

	if ((*left) > (uint)c->len)
	    (*left) = (uint)c->len;
	return c->r_ptr;
}

int cache_rend( struct cache *c, const char *buffer, int size)
{
	if ((c->len == 0) ||
			((buffer != c->r_ptr) || ((c->ca.end - c->r_ptr) < size)))
		return -1;
	else {
		c->len -= size;
		if ((c->r_ptr += size) == c->ca.end)
			c->r_ptr = c->ca.begin;
	} return 0;
}


/*****************************************************************************/
int cache_fread( struct cache *c,
				 uintptr_t fd, ssize_t (*f)( uintptr_t, void *, size_t), size_t size)
{
	register int n, b;

	if ((n = c->ca.end - c->w_ptr) > size)
	{
		if ((n = f( fd, c->w_ptr, size)) < 0)	/* 한번에 블럭 읽기... */
			return -1;
		else {
			;
		} c->w_ptr += n;
	}
	else {
		if ((b = f( fd, c->w_ptr, n)) < 0) return -1;	/* 첫 블럭 읽기 */
		else if (b < n) c->w_ptr += (n = b) ;	/* 일부 버퍼 완료 */
		else {
			c->w_ptr = c->ca.begin;
			if ((b = size - b) == 0) ;	/* 처리 완료 */
			else {
				if ((b = f( fd, c->w_ptr, b)) >= 0)	/* 두번째 블럭 읽기 */
					c->w_ptr += b;
				else {
					c->len += n;
					return -1;
				} n += b;
			}
		}
	} c->len += n; return n;
}


int cache_fwrite( struct cache *c,
                  uintptr_t fd, ssize_t (*f)( uintptr_t, const void *, size_t), size_t size)
{
	register int n, b;

	if (size == 0) size = c->len;
	if ((n = c->ca.end - c->r_ptr) > size)	/* 한번에 전송 가능.. */
	{
		if ((n = f( fd, c->r_ptr, size)) < 0)
			return -1;
		else {
			;
		} c->r_ptr += n;
	}
	else {
		if ((b = f( fd, c->r_ptr, n)) < 0) return -1;	/* 첫 블럭 전송 */
		else if (b < n) c->r_ptr += (n = b) ;	/* 일부 버퍼 완료 */
		else {
			c->r_ptr = c->ca.begin;		/* 블럭 위치 재 설정 */
			if ((b = size - b) == 0) ;	/* 전송 완료 */
			else {
				if ((b = f( fd, c->r_ptr, b)) >= 0)	/* 두번째 블럭 전송 */
					c->r_ptr += b;
				else {
					c->len -= n;
					return -1;
				} n += b;
			}
		}
	} c->len -= n; return n;
}
/* @} */