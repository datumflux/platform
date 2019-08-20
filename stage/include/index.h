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
#ifndef __INDEX_OBJECT_H
#  define __INDEX_OBJECT_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief Indexed SKIP-LIST 알고리즘을 기반으로 정렬된 인덱스 구조로 관리
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"

/*! \addtogroup core_index
 *  @{
 */
typedef int(*io__compare_t)(struct io__parted *,
		const void *l_k, int l_length, const void *r_k, int r_length, void *);

/* */
struct indexable *iof_open(const char *f, int key_size, int nelem, io__compare_t compare = NULL, void *udata = NULL);
int               iof_close(struct indexable *);

int       iof_rebase(struct indexable *f); /* 캐싱된 페이지를 모두 제거한다. */
int       iof_flush(struct indexable *f);	/* 변경된 데이터를 기록한다. */

__off64_t iof_count(struct indexable *f, int part_id = -1); /* 저장된 전체 항목 수 */
int       iof_bufsize(struct indexable *f); /* 저장 가능한 데이터 크기 */

/* */
/* 키의 정렬을 올림차순( 0 -> X) 설정한다. (기본은 내림차순) */
#define INDEX_ORDER_ASC_FLAG			0x01

struct io__parted *iop_get(struct indexable *f, int part_id);
struct io__parted *iop_create(struct indexable *f, int part_id, int flags = 0);

int iop_id(struct indexable *f, struct io__parted *parted); /* 파티션 ID */

/*
 io_put: 인덱스 객체를 추가한다.
 반환:
   항목의 순위 값

   __off64_t *a__offset       데이터가 저장된 파일 오프셋
   __off64_t *d__offset       중복된 객체인 경우, 
                              기존 객체가 이동한 파일 오프셋 (-1 == 중복 없음)
*/
__off64_t iof_put(struct indexable *f, int part_id,
		const void *k, int k_length, void **v, __off64_t *a__offset = NULL, __off64_t *d__offset = NULL);

/* */
__off64_t iof_find(struct indexable *f, int part_id,
		const void *k, int k_length, void **v, __off64_t *a__offset = NULL);

struct io__object *iof_touch(struct indexable *f, __off64_t a__offset); /*rw */
struct io__object *iof_object(struct indexable *f, __off64_t a__offset); /* ro */

/* iof_value: 객체를 접근 가능한 버퍼형태의 값으로 반환한다. */
struct io__value {
	const void *k,
			*v;
	int         k_length;
	bool        duplicated; /* 중복 유무 */
}; struct io__value *iof_value(struct io__object *o, struct io__value *i);

/* io_remove: 저장된 객체를 제거한다. */
int       iof_remove(struct indexable *f, int part_id, const void *k, int k_length);


/* io_erase: 저장된 객체를 제거한다.
   입력
      __off64_t a__offset	삭제할 객체의 저장 오프셋
	  
   반환
      __off64_t *r__offset	중복객체인 경우, 
	              삭제할 위치(a__offset)으로 이동하는 중복객체 저장 오프셋
	  void **r__v			중복객체인 경우, 이동되는 객체의 저장 데이터

   설명
      객체를 삭제할때, 만약 중복되어 저장된 객체의 경우에는 처음 데이터를 
	  삭제하지 않고 다음 데이터를 삭제하려는 객체로 이동하고 다음 객체를 
	  삭제하도록 처리한다.
	  
	  이는 인덱스의 재 구성을 제거하여 처리 속도를 빠르게 하기위한 처리이므로,
	  해당 처리가 발생되면 이동되는 객체의 정보 갱신이 필요할수 있다.
 */
#include <functional>

int iof_remove(struct indexable *f, int part_id, const void *k, int k_length,
		std::function<bool(__off64_t distance, const void *buffer, __off64_t a_offset)> callback,
		__off64_t *r__offset = NULL, void **r__v = NULL);

/* iof_remove: a__offset과 일치하는 항목만 삭제 */
inline int iof_remove(struct indexable *f, int part_id,
		const void *k, int k_length, __off64_t a__offset, __off64_t *r__offset = NULL, void **r__v = NULL) {
	return iof_remove(f, part_id, k, k_length,
	                 [&a__offset](__off64_t distance, const void *v, __off64_t v__offset) {
		                 return (v__offset == a__offset);
	                 }, r__offset, r__v);
}

/* iof_remove: a__offset 항목을 삭제 */
inline int iof_remove(struct indexable *f,
		int part_id, __off64_t a__offset, __off64_t *r__offset = NULL, void **r__v = NULL)
{
	struct io__object *o = iof_object(f, a__offset);
	if (o == NULL)
		return -1;
	else {
		struct io__value v; iof_value(o, &v);
		return iof_remove(f, part_id, v.k, v.k_length,
			[&a__offset](__off64_t distance, const void *v, __off64_t v__offset) {
				return v__offset == a__offset;
			}, r__offset, r__v);
	}
}

/* */
struct io__cursor {
	struct indexable *f;
	struct {
		__off64_t distance;
		__off64_t offset;

		struct io__page   *page;
		struct io__object *object;
	} now;
	struct io__parted *parted;
}; int ioc_ready(struct indexable *f, int part_id, struct io__cursor *cursor);

int ioc_find(struct io__cursor *cursor, const void *k, int k_length);
int ioc_lower(struct io__cursor *cursor, const void *k, int k_length);
int ioc_distance(struct io__cursor *cursor, __off64_t start);

int ioc_touch(struct io__cursor *cursor);

__off64_t  ioc_fetch(struct io__cursor *cursor, struct io__object **o, __off64_t *a__offset = NULL);
int        ioc_next(struct io__cursor *cursor, const void *k = NULL, int k_length = 0);

/* @} */
#endif
