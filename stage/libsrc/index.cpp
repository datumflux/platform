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
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <sys/mman.h>

#include "include/index.h"
#include "list.h"

/*! \defgroup core_index Indexed SKIP-LIST 알고리즘을 기반으로 정렬된 인덱스 구조로 관리
 *  @{
 */

#if defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#include "mman.h"

static int getpagesize() {
	SYSTEM_INFO si; GetSystemInfo(&si); return si.dwAllocationGranularity;
}

#  define PAGE_SIZE				getpagesize()
#  define PAGE_ALIGN( __n)		(((__n) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
#  define PAGE_OFFSET( __n)		((__n) & ~(PAGE_SIZE - 1))


static int pwrite64(int fd, const void *buffer, int size, __off64_t offset) {
	return (_lseeki64(fd, offset, SEEK_SET) < 0) ? -1: _write(fd, buffer, size);
}

static int pread64(int fd, void *buffer, int size, __off64_t offset) {
	return (_lseeki64(fd, offset, SEEK_SET) < 0) ? -1 : _read(fd, buffer, size);
}
#endif

/*
	+-------+     +-------+
	|  SLB  | --> |  SLB  |
	+-------+     +-------+
	|
	+-> +-----+ ~~~ +-----+
		| SLN |     | SLN |
		+-----+ ~~~ +-----+
*/

struct io__page { /* 일정 갯수의 블럭 단위로 메모리에 적재 */
	__off64_t offset;

	int   dirty;
	char *buffer; /* mmap() */
	struct list_head l_cache;
}; /* char buffer[length] */


#define MAX_LEVEL           			6

#pragma pack(push, 1)
struct io__object {
	int16_t level;              /* forward의 depth */

	struct f {
		__off64_t offset;       /* 위치 */
		__off64_t width;        /* 거리 */
	} forward[MAX_LEVEL];

	int32_t  retain;            /* 중복된 횟수 */
	/* user data */
	uint16_t length;            /* key 의 길이 */
}; /* char key[length] */

#define INDEX_ORDER_ASC_FLAG				0x01

struct io__parted : public io__object::f {
	uint32_t flags; /* */
};

#define MAX_PARTED					        32
#define HEAD_SIZE						    PAGE_SIZE

struct io__metadata {
	intptr_t SIGN;

	__off64_t l_eof;        /* 신규로 추가된 객체 갯수 */
	struct of {
		struct {
			__off64_t head;     /* 재사용 시작 */
			__off64_t tail;     /* 재사용 마지막 */
		} f;

		__off64_t offset;   /* node를 시작할수 있는 위치 */
		__off64_t end;      /* eof */
	} l_use[MAX_LEVEL];

	int page_size;          /* page page size */
	int block_size;         /* sizeof(struct io__object) + max_length */
	struct io__parted start[MAX_PARTED];
};

struct indexable {
	int       fd;

	__off64_t file_size;	    /* 파일 크기 */
	struct io__metadata *begin;

	struct list_head   l_cache;
	struct random_data r_data;
	io__compare_t      o_compare;
	void              *udata;
};
#pragma pack(pop)


static struct io__page *io__read(struct indexable *f, __off64_t offset, struct io__object **object)
{
	static char __nil;

	__off64_t l_offset;
	struct io__page *page;

	struct list_head *pos; list_for_each(pos, &f->l_cache)
	{
		page = list_entry(pos, struct io__page, l_cache);
		if (page->offset > offset) break; /* add */
		else if ((l_offset = (page->offset + f->begin->page_size)) > offset)
_gR:    {
			/* in */
			if (object != NULL)
			{
				offset = offset - page->offset;
				(*object) = (struct io__object *)(page->buffer + offset);
			}
			return page;
		}
	}

	l_offset = (((offset - HEAD_SIZE) / f->begin->page_size) * f->begin->page_size) + HEAD_SIZE;
	if ((f->file_size <= l_offset) &&
			(pwrite64(f->fd, &__nil, sizeof(__nil), (l_offset + f->begin->page_size) - 1) < 0));
	else if (!(page = (struct io__page *)malloc(sizeof(struct io__page))));
	else {
		page->buffer = (char *)mmap(NULL, f->begin->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, l_offset);
		if (page->buffer != MAP_FAILED)
		{
			page->offset = l_offset;
			page->dirty = 0;
			if (f->file_size <= l_offset)
			{
				memset(page->buffer, 0, f->begin->page_size);
				{
					struct io__object *o = (struct io__object *)page->buffer;

					for (int i = (int)(f->begin->page_size / f->begin->block_size); (--i) >= 0; o++)
						o->level = -1;
				}
				f->file_size = page->offset + f->begin->page_size;

				page->dirty++;
			}

			__list_add(&page->l_cache, pos->prev, pos);
			goto _gR;
		}

		free(page);
	}
	return NULL;
}


int iof_rebase(struct indexable *f)
{
	struct io__page *page;
	int __x = 0;

	struct list_head *pos, *next; list_for_each_safe(pos, next, &f->l_cache)
	{
		if ((page = list_entry(pos, struct io__page, l_cache))->dirty == 0)
			munmap(page->buffer, f->begin->page_size);
		else {
			if (msync(page->buffer, f->begin->page_size, MS_ASYNC) < 0)
				return -1;

			page->dirty = 0;
			continue;
		}

		list_del(&page->l_cache);
		free(page);
		++__x;
	}
	return (msync(f->begin, HEAD_SIZE, MS_ASYNC) == 0) ? __x : -1;
}


int iof_close(struct indexable *f)
{
	struct io__page *page;

	struct list_head *pos, *next; list_for_each_safe(pos, next, &f->l_cache)
	{
		page = list_entry(pos, struct io__page, l_cache);
		if ((page->dirty > 0) &&
		        (msync(page->buffer, f->begin->page_size, MS_SYNC) < 0))
			return -1;
		else {
			munmap(page->buffer, f->begin->page_size);
		}

		list_del(&page->l_cache);
		free(page);
	}

	/* INIT_LIST_HEAD(&f->l_cache); */
	if (msync(f->begin, HEAD_SIZE, MS_SYNC) < 0)
		return -1;
	else {
		munmap(f->begin, HEAD_SIZE);
		close(f->fd);
	}
	free(f);
	return 0;
}


static int io__compare(struct indexable *f,
		struct io__parted *parted, struct io__object *next, const void *k, int k_length)
{
	int delta = f->o_compare(
			parted, (void *)(next + 1), next->length, k, k_length, f->udata
	) * ((parted->flags & INDEX_ORDER_ASC_FLAG) ? 1 : -1);
	return ((delta == 0) && (k_length != next->length)) ? (next->length - k_length) : delta;
}


static int memcmp__compare(struct io__parted *parted,
	const void *l_k, int l_length, const void *r_k, int r_length, void *udata) {
	return memcmp(l_k, r_k, std::min(l_length, r_length));
}


struct indexable *iof_open(const char *file, int key_size, int nelem, io__compare_t compare, void *udata)
{
	struct indexable *f;
#define INDEX_SIGN                  0x0000001046620250
#define RANDOM_BUFSIZ               256
	if (!(f = (struct indexable *)calloc(1, sizeof(struct indexable) + RANDOM_BUFSIZ)))
		;
	else {
		int need_size = std::min((int)(sizeof(struct io__object) + key_size), PAGE_SIZE);

		if ((f->fd = open64(file, O_RDWR | O_CREAT, 0644)) < 0)
			;
		else {
			struct stat st;

			if (fstat(f->fd, &st) < 0)
_gE:            close(f->fd);
			else {
				need_size = PAGE_SIZE / (PAGE_SIZE / need_size);
				if ((f->file_size = st.st_size) == 0)
				{
					struct io__metadata __i; memset(&__i, 0, sizeof(struct io__metadata));

					__i.SIGN = INDEX_SIGN;
					__i.block_size = need_size;
					__i.page_size = PAGE_ALIGN(nelem * need_size);
					__i.l_eof = HEAD_SIZE;
					if ((pwrite64(f->fd, &__i.SIGN, sizeof(__i.SIGN), HEAD_SIZE - sizeof(__i.SIGN)) < 0) ||
					        (pwrite64(f->fd, &__i, sizeof(__i), 0) < 0))
						goto _gE;
					else {
						f->file_size = HEAD_SIZE;
					}
				}

				if ((f->begin = (struct io__metadata *)
						mmap(NULL, HEAD_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, 0)) == MAP_FAILED)
					goto _gE;
				else {
					if ((f->begin->SIGN != INDEX_SIGN) || (f->begin->block_size != need_size))
					{
						munmap((void *)f->begin, HEAD_SIZE);
						goto _gE;
					}

					INIT_LIST_HEAD(&f->l_cache);
				}

				if (!(f->udata = udata)) f->udata = f;
				if (!(f->o_compare = compare)) f->o_compare = memcmp__compare;

				initstate_r((uint32_t)((uintptr_t)f), (char *)(f + 1), RANDOM_BUFSIZ, &f->r_data);
				srandom_r((uint32_t)((uintptr_t)f), &f->r_data);
				return f;
			}
		}

		free(f);
	}
	return NULL;
}


__off64_t iof_count(struct indexable *f, int part_id)
{
	if (part_id < 0)
	{
		__off64_t total = 0;

		for (int i = 0; i < MAX_PARTED; i++)
			total += f->begin->start[i].width;
		return total;
	}
	else if (part_id > MAX_PARTED) return -1;
	return f->begin->start[part_id].width;
}


int iof_flush(struct indexable *f)
{
	struct io__page *page;

	int __x = 0;
	struct list_head *pos; list_for_each(pos, &f->l_cache)
		if ((page = list_entry(pos, struct io__page, l_cache))->dirty > 0)
		{
			if (msync(page->buffer, f->begin->page_size, MS_ASYNC) < 0)
				return -1;
			else {
				page->dirty = 0;
				__x++;
			}
		}
	return (msync(f->begin, HEAD_SIZE, MS_ASYNC) >= 0) ? __x : -1;
}


int iof_bufsize(struct indexable *f) { return f->begin->block_size - sizeof(struct io__object); }


static struct io__page *io__create(struct indexable *f, int level, __off64_t *offset, struct io__object **object)
{
	struct io__metadata::of *o_use = f->begin->l_use + (level - 1);

	if (((*offset) = o_use->f.head) > 0)
		;
	else {
		if (((*offset) = o_use->offset) > 0)
		{
			o_use->offset += f->begin->block_size;
			if (o_use->offset <= o_use->end)
_gS:		{
				struct io__page *page = io__read(f, (*offset), object);

				if (page != NULL)
				{
					++page->dirty; {
						memset(*object, 0, f->begin->block_size);
					} (*object)->level = level;
				}
				return page;
			}
			o_use->end = o_use->offset = 0;
		}

		(*offset) = f->begin->l_eof;
		{
			o_use->offset = (*offset) + f->begin->block_size;
			f->begin->l_eof += f->begin->page_size;
		} o_use->end = f->begin->l_eof;
		goto _gS;
	}

	/* */
	{
		struct io__page *page = io__read(f, (*offset), object);

		if (page != NULL)
		{
			++page->dirty;
			{
				if ((o_use->f.head = (*object)->forward[0].offset) == 0)
					o_use->f.tail = 0;
			} memset(*object, 0, f->begin->block_size);
			(*object)->level = level;
		}
		return page;
	}
}


int iop_id(struct indexable *f, struct io__parted *parted) {
	return (int)(parted - f->begin->start);
}


struct io__parted *iop_get(struct indexable *f, int part_id)
{
	if ((part_id < 0) || (part_id >= MAX_PARTED)) return NULL;
	{
		struct io__parted *parted = f->begin->start + part_id;
		return (parted->offset == 0) ? NULL : parted;
	}
}

struct io__object *iof_touch(struct indexable *f, __off64_t offset)
{
	struct io__page *page;
	struct io__object *node;

	if (!(page = io__read(f, offset, &node)))
		return NULL;
	else {
		page->dirty++;
	} return node;
}

struct io__parted *iop_create(struct indexable *f, int part_id, int flags)
{
	if ((part_id < 0) || (part_id >= MAX_PARTED))
		return NULL;
	else {
		struct io__parted *parted = f->begin->start + part_id;

		if (parted->offset == 0)
		{
			struct io__object *object;
			struct io__page *page;

			if (!(page = io__create(f, MAX_LEVEL, &parted->offset, &object)))
				return NULL;
			else {
				parted->width = 0;
				parted->flags = flags;
				for (int i = 0; i < MAX_LEVEL; i++)
					object->forward[i] = *parted;
				object->level = 0;
			}
			page->dirty++;
		}
		return parted;
	}
}


static int io__free(struct indexable *f, int level, __off64_t l__offset)
{
	struct io__metadata::of *o_use = f->begin->l_use + (level - 1);

	if (o_use->f.head == 0)
		o_use->f.head = l__offset;
	else {
		struct io__object *io__object;
		struct io__page *page = io__read(f, o_use->f.tail, &io__object);
		if (page == NULL)
			return -1;
		else {
			io__object->forward[0].offset = l__offset;
		}
		page->dirty++;
	}
	o_use->f.tail = l__offset;
	return 0;
}


#if 0
int iop_drop(struct indexable *f, int part_id)
{
	if ((part_id < 0) || (part_id >= MAX_PARTED))
		return -1;
	else {
		struct io__parted *parted = f->begin->start + part_id;

		if (parted->offset == 0)
			return -1;
		else {
			struct io__object *io__object;
			struct io__page *page = io__read(f, parted->offset, &io__object);

			if (page == NULL)
				return -1;
			else {
				__off64_t l_offset = parted->offset;

				page->dirty++;
				for (int i = io__object->level; (--i) >= 0; )
					do {
						if (io__object->forward[i].offset == 0)
							break;
						else {
							l_offset = io__object->forward[i].offset;
							if (!(page = io__read(f, l_offset, &io__object)))
								return -1;
						}
					} while (true);
				io__free(f, parted->offset, l_offset);
			}

			memset(parted, 0, sizeof(*parted));
		}
	}
	return 0;
}
#endif

/* */
__off64_t iof_find(struct indexable *f, int part_id, const void *k, int k_length, void **v, __off64_t *a__offset)
{
	struct io__object *io__object;
	struct io__parted *parted;

	if (!(parted = iop_get(f, part_id)) ||
	        !io__read(f, parted->offset, &io__object))
		return -1;
	else {
		__off64_t distance = 0;

		for (int i = io__object->level; (--i) >= 0; )
			do {
				struct io__object *next;

				if (io__object->forward[i].offset == parted->offset) break;
				else if (io__read(f, io__object->forward[i].offset, &next) == NULL)
					return -1;
				else {
					int delta = io__compare(f, parted, next, k, k_length);
					if (delta > 0) break;
					else {
						distance += io__object->forward[i].width;
						if (delta == 0)
						{
							if (v) (*v) = ((char *)(next + 1)) + k_length;
							if (a__offset) (*a__offset) = io__object->forward[i].offset;
							return distance;
						}
					}
				}
				io__object = next;
			} while (true);
	}
	return 0;
}

struct io__object *iof_object(struct indexable *f, __off64_t a__offset)
{
	struct io__object *o;
	return ((a__offset > 0) && io__read(f, a__offset, &o)) ? o : NULL;
}

/* */
struct io__value *iof_value(struct io__object *o, struct io__value *i)
{
	if (o == NULL)
		return NULL;
	else {
		i->k = (const void *)(o + 1);
		i->k_length = o->length;
		i->duplicated = (o->retain > 0) || (o->level == 1);
		i->v = ((char *)i->k) + o->length;
	} return i;
}

__off64_t ioc_fetch(struct io__cursor *cursor, struct io__object **o, __off64_t *a__offset)
{
	if (o) (*o) = cursor->now.object;
	if (a__offset) (*a__offset) = cursor->now.offset;
	return cursor->now.distance;
}

int ioc_touch(struct io__cursor *cursor) { return ++cursor->now.page->dirty; }

int ioc_next(struct io__cursor *cursor, const void *k, int k_length)
{
	if (cursor->now.object->forward[0].offset == cursor->parted->offset)
		return -2; /* EOF */
	else {
		if (!(cursor->now.page = io__read(cursor->f, cursor->now.offset =
				cursor->now.object->forward[0].offset, &cursor->now.object)))
			return -1;
	}

	cursor->now.distance++;
	return ((k != NULL) ? io__compare(cursor->f, cursor->parted, cursor->now.object, k, k_length) : 0);
}


int ioc_ready(struct indexable *f, int part_id, struct io__cursor *cursor)
{
	memset(cursor, 0, sizeof(struct io__cursor));
	return (!(cursor->parted = iop_get(cursor->f = f, part_id)) ||
		!(cursor->now.page = io__read(cursor->f, cursor->parted->offset, &cursor->now.object))) ? -1 : 0;
}

int ioc_distance(struct io__cursor *cursor, __off64_t start)
{
	struct io__object *next;

	for (int i = cursor->now.object->level; (--i) >= 0;)
		do {
			if (cursor->now.object->forward[i].offset == cursor->parted->offset) break;
			else if ((cursor->now.distance + cursor->now.object->forward[i].width) > start) break;
			else if (!(cursor->now.page = io__read(cursor->f,
					cursor->now.offset = cursor->now.object->forward[i].offset, &next)))
				return -1;
			else {
				cursor->now.distance += cursor->now.object->forward[i].width;
				cursor->now.object = next;
				if (cursor->now.distance == start) return 0;
			}
		} while (true);
	return -1;
}

int ioc_find(struct io__cursor *cursor, const void *k, int k_length)
{
	if (cursor->now.object->forward[0].offset == cursor->parted->offset)
		return -1;
	else if (k == NULL)
		cursor->now.page = io__read(cursor->f,
				cursor->now.offset = cursor->now.object->forward[0].offset, &cursor->now.object);
	else {
		struct io__object *next;
		int delta;

		for (int i = cursor->now.object->level; (--i) >= 0;)
			do {
				if (cursor->now.object->forward[i].offset == cursor->parted->offset) break;
				else if (!(cursor->now.page = io__read(cursor->f,
						cursor->now.offset = cursor->now.object->forward[i].offset, &next)))
					return -1;
				else if ((delta = io__compare(cursor->f, cursor->parted, next, k, k_length)) > 0) break;
				else {
					cursor->now.distance += cursor->now.object->forward[i].width;
					cursor->now.object = next;
					if (delta == 0) return 0;
				}
			} while (true);
		return -1;
	}
	return 0;
}


int ioc_lower(struct io__cursor *cursor, const void *k, int k_length)
{
	if (k != NULL)
	{
		struct io__object *next;

		for (int i = cursor->now.object->level; (--i) >= 0;)
			do {
				if (cursor->now.object->forward[i].offset == cursor->parted->offset) break;
				else if (!(cursor->now.page = io__read(cursor->f,
						cursor->now.offset = cursor->now.object->forward[i].offset, &next)))
					return -1;
				/* lower() 처리 이므로 >= 로 변경(크거나 같으면 검색 중지) */
				else if (io__compare(cursor->f, cursor->parted, next, k, k_length) >= 0) break;
				else {
					cursor->now.distance += cursor->now.object->forward[i].width;
					cursor->now.object = next;
				}
			} while (true);
	}

	if (cursor->now.object->forward[0].offset == cursor->parted->offset)
		return -1;
	else {
        ++cursor->now.distance; /* 0 부터 시작되므로 1증가 */
        cursor->now.page = io__read(cursor->f,
        		cursor->now.offset = cursor->now.object->forward[0].offset, &cursor->now.object);
	}
	return 0;
}


/* */
struct i__route {
	struct update {
		__off64_t distance;         /* 다음 object 까지의 거리 */

		struct io__page *page;
		struct io__object *object;  /* 현재 object */
	} update[MAX_LEVEL + 1], *c/* current */, f/* first */;
	struct io__parted *parted;
};


static int ior_lower(struct indexable *f, int part_id, const void *k, int k_length, struct i__route *route)
{
	memset(route, 0, sizeof(struct i__route));

	route->c = route->update + MAX_LEVEL;
	if (!(route->parted = iop_get(f, part_id)) ||
	        !(route->f.page = io__read(f, route->parted->offset, &route->f.object)))
		return -1;
	else {
		struct io__object *object;

		*route->c = route->f;
		for (int i = route->f.object->level; (--i) >= 0; route->update[i] = *route->c)
			do {
				struct io__object::f *F = route->c->object->forward + i;

				/* fprintf(stderr, " * [LOWER.%d] %ld == %ld\n", i, F->offset, R->parted->offset); */
				if (F->offset == route->parted->offset)
					break;
				else {
					struct io__page *page;

					if (!(page = io__read(f, F->offset, &object))) return -1;
					else if (io__compare(f, route->parted, object, k, k_length) >= 0) break;
					route->c->page = page;
				}

				route->c->distance += F->width;
				route->c->object = object;
			} while (true);
		route->f.distance = -1;
	}
	return 0;
}


static void io__move(int k_length, struct io__object *f, struct io__object *t, int block_size)
{
	int v_length = block_size - (sizeof(struct io__object) + k_length);
	memcpy(((char *)(t + 1)) + k_length, ((char *)(f + 1)) + k_length, v_length);
}


__off64_t iof_put(struct indexable *f,
	int part_id, const void *k, int k_length, void **v, __off64_t *a__offset, __off64_t *d__offset)
{
	struct i__route route;
	__off64_t r__offset;

	if ((ior_lower(f, part_id, k, k_length, &route) < 0) ||
	        !(route.c->page = io__read(f,
	        		r__offset = route.c->object->forward[0].offset, &route.c->object)))
_gC:	return -1;
	{
		struct io__object *object;
		__off64_t a_offset;
		int apply = 0;

		int r__delta = io__compare(f, route.parted, route.c->object, k, k_length);
		if (r__delta != 0) /* */
		{
			auto createLevel = [](struct indexable *f, int start)
			{
#define MAX_BIT         31
				int32_t r; random_r(&f->r_data, &r);
				for (uint8_t bit = 0; r & (1 << bit); )
				{
					if ((++start) >= MAX_LEVEL) break;
					else if ((++bit) >= MAX_BIT) break;
				}
#undef MAX_BIT
				return start;
			};

			if (io__create(f, createLevel(f, 2), &a_offset, &object) == NULL)
				goto _gC;
			else {
				memcpy((void *)(object + 1), k, object->length = k_length);
				if (d__offset == (__off64_t *)-1);
				else if (d__offset != NULL) (*d__offset) = -1;
			}
		}
		else if (d__offset == (__off64_t *)-1)
		{
			if (v)
				(*v) = ((char *)(route.c->object + 1)) + k_length;
			(*a__offset) = r__offset;
			return 0;
		}
		else if (io__create(f, 1, &a_offset, &object) == NULL) goto _gC;
		else {
			memcpy((void *)(object + 1), k, object->length = k_length);

			// dup key:
			/* 중복되는 키를 추가하는 경우에는, root에 추가되는 node를 설정하고 이전 node를 뒤로 보낸다.
			* +-----------+      +---
			* |           | ---> | add
			* +-----------+      +---
			*/
			object->forward[0].offset = route.c->object->forward[0].offset;
			if (object->forward[0].offset == route.parted->offset) /* EOF */
				route.c->object->forward[0].width++;
			else object->forward[0].width++;
			{
				route.c->object->forward[0].offset = a_offset;
				if (d__offset != NULL) /* 치환되는 노드의 위치를 반환한다. */
					(*d__offset) = a_offset; /* 변경 오프셋 */

				/* new item to first position */
				io__move(k_length, route.c->object, object, f->begin->block_size);
				++route.c->object->retain;
			} object = route.c->object;

			a_offset = r__offset; /* 기존 오프셋 */
			for (++route.c->page->dirty; (++apply) < route.c->object->level; )
				++object->forward[apply].width;
			goto _gD;
		}

		/* route.f.distance == -1 이므로 객체를 추가하도록 처리 */
		for (; route.f.object->level < object->level; ++route.f.object->level)
			route.update[route.f.object->level] = route.f;

		/* fprintf(stderr, " * %s.%d: level - %d\n", __FUNCTION__, __LINE__, add->level); */
		for (; apply < object->level; ++apply)
		{
			struct i__route::update *U = route.update + apply;
			struct io__object::f *a_F = object->forward + apply,
				                 *u_F = U->object->forward + apply;

			/*
			* +-----+       +-----+       +--
			* | u_F | ----> | a_F | ----> |
			* +-----+       +-----+       +--
			*/
			++U->page->dirty;
			if (U->distance < 0)
			{
				a_F->width = (route.parted->width - route.c->distance);
				u_F->width = route.c->distance + 1;

				/* fprintf(stderr, "\t- APPEND.%d: a_F->width:(%ld - %ld), u_F->width: %ld\n",
						apply, route.parted->width, route.c->distance, u_F->width); */
			}
			else {
				a_F->width = ((U->distance + u_F->width) - route.c->distance);

				/* fprintf(stderr, "\t- INSERT.%d: a_F->width:((%ld + %ld) - %ld), u_F->width: %ld\n",
				        apply, U->distance, u_F->width, route.c->distance, (u_F->width - a_F->width) + 1); */

				u_F->width = (u_F->width - a_F->width) + 1;
			}

			object->forward[apply].offset = u_F->offset;
			u_F->offset = a_offset;
		}

_gD:	++route.parted->width;
		for (; apply < route.f.object->level; ++apply)
			++route.update[apply].object->forward[apply].width;
		{
			if (v) (*v) = ((char *)(object + 1)) + k_length;
			if (a__offset) (*a__offset) = a_offset;
		}

		/* {
			struct io__cursor cursor;
			int64_t count = 0;

			if (ioc_ready(f, part_id, &cursor) < 0);
			else if (ioc_distance(&cursor, 1) == 0) do {
					++count;
				} while (ioc_next(&cursor) == 0);
			if (route.parted->width != count)
			{
				fprintf(stderr, " ** ABORT: %ld, %ld\n", route.parted->width, count);
				abort();
			}
		} */
	}
	return route.c->distance + 1;
}


int iof_remove(struct indexable *f, int part_id, const void *k, int k_length,
		std::function<bool(__off64_t distance, const void *buffer, __off64_t a_offset)>  callback,
		__off64_t *r__offset, void **r__v)
{
	struct i__route route;
	struct io__object *object;
	struct io__page *page;

	if ((ior_lower(f, part_id, k, k_length, &route) < 0) ||
	        !(page = io__read(f, route.c->object->forward[0].offset, &object)))
		return -1;
	else if (io__compare(f, route.parted, object, k, k_length) == 0)
	{
		__off64_t offset = route.c->object->forward[0].offset;

		if (object->retain > 0) /* 중복되는 데이터를 삭제하는 경우 */
		{
			struct io__object *r_object = object;
			struct io__page *r_page = page;

			if ((callback != NULL) &&
			        (callback(route.c->distance, ((char *)(r_object + 1)) + k_length, offset) == false))
			{
				struct io__page *l_page = page;
				struct io__object *l_object = object;
				int distance = 0;

				int retain = object->retain;
				do {
					if (((--retain) < 0) ||
							(r_object->forward[0].offset == route.parted->offset))
						return 0;
					else {
						l_page = r_page;
						l_object = r_object;
						if (!(r_page = io__read(f, offset = l_object->forward[0].offset, &r_object)))
							return -1;
					} ++distance;
				} while (callback(route.c->distance + distance, ((char *)(r_object + 1)) + k_length, offset) == false);

				++l_page->dirty;
				l_object->forward[0].offset = r_object->forward[0].offset;

				if (r__offset != NULL) (*r__offset) = -1;
				if (r__v != NULL)      (*r__v) = NULL;
			} /* 처음 노드는 삭제하지 않고, 위치를 이동해, 다음 노드를 삭제한다. */
			else if (!(r_page = io__read(f, offset = object->forward[0].offset, &r_object))) return -1;
			else {
				if (r__offset != NULL) (*r__offset) = offset; /* 옮겨지는 소스 오프셋 (offset -> delete_offset) */
				if (r__v != NULL)      (*r__v) = ((char *)(object + 1)) + k_length;

				object->forward[0].offset = r_object->forward[0].offset;
				/* move data: r_node -> next (erase to r_node) */
				io__move(k_length, r_object, object, f->begin->block_size);
			}
			/* */

			{
				int i = (object->forward[0].offset != route.parted->offset);

				--object->retain;
				for (; i < object->level; ++i) --object->forward[i].width;
				for (; i < route.f.object->level; ++i) --route.update[i].object->forward[i].width;
			}

			++r_page->dirty;
			(object = r_object)->forward[0].offset = 0;
		}
		else {
			/* */
			if ((callback == NULL) || callback(route.c->distance, ((char *)(object + 1)) + k_length, offset))
				for (int i = 0; i < route.f.object->level; ++i)
				{
					struct io__object::f *u_F = route.update[i].object->forward + i;

					if (u_F->offset == offset)
						u_F->offset = object->forward[i].offset;
					else {
						do {
							++route.update[i].page->dirty;
							--route.update[i].object->forward[i].width;
						} while ((++i) < route.f.object->level);
						break;
					}

					++route.update[i].page->dirty;
					u_F->width += (object->forward[i].width - 1);
				}
			else return 0;

			object->forward[0].offset = 0;
			while ((route.f.object->level > 0) &&
			       (route.f.object->forward[route.f.object->level - 1].offset == route.parted->offset))
			{
				++route.f.page->dirty;
				--route.f.object->level;
			}

			if (r__offset != NULL) (*r__offset) = -1;
			if (r__v != NULL)      (*r__v) = NULL;
		}

		++page->dirty;
		--route.parted->width;
		return (io__free(f, object->level, offset) >= 0) ? 1 : -1;
	}
	return 0;
}


int iof_remove(struct indexable *f, int part_id, const void *k, int k_length)
{
	struct i__route route;
	struct io__object *object;
	struct io__page *page;

	if ((ior_lower(f, part_id, k, k_length, &route) < 0) ||
	        !(page = io__read(f, route.c->object->forward[0].offset, &object)))
		return -1;
	else if (io__compare(f, route.parted, object, k, k_length) == 0)
	{
		__off64_t offset = route.c->object->forward[0].offset;
		/* __off64_t l_offset = -1; */

		struct io__object *l_object = object;
		int nerase = object->retain + 1;

		if (object->retain > 0) /* 중복된 데이터인 경우 */
		{
			struct io__object *r_object = object;

			int retain = object->retain;
			do {
				/* 삭제하고자 하는 데이터를 찾는다. */
				if (io__read(f, r_object->forward[0].offset, &r_object) == NULL)
					return -1;
				else {
					if (io__free(f, 1, r_object->forward[0].offset) < 0)
						return -1;
				} l_object = r_object;
			} while (((--retain) > 0) && (r_object->forward[0].offset != route.parted->offset));
		}

		/* */
		for (int i = 0; i < route.f.object->level; ++i)
		{
			struct io__object::f *u_F = route.update[i].object->forward + i;

			if (u_F->offset == offset)
				u_F->offset = (i == 0) ? l_object->forward[i].offset : object->forward[i].offset;
			else {
				do {
					++route.update[i].page->dirty;
					route.update[i].object->forward[i].width -= nerase;
				} while ((++i) < route.f.object->level);
				break;
			}

			++route.update[i].page->dirty;
			if (i > 0) u_F->width += (object->forward[i].width - nerase);
			else if (u_F->offset == route.parted->offset) u_F->width = 0;
		}

		l_object->forward[0].offset = 0;
		while ((route.f.object->level > 0) &&
		       (route.f.object->forward[route.f.object->level - 1].offset == route.parted->offset))
		{
			++route.f.page->dirty;
			--route.f.object->level;
		}

		++page->dirty;
		route.parted->width -= nerase;
		return (io__free(f, object->level, offset) >= 0) ? 1 : -1;
	}
	return 0;
}
/* @} */