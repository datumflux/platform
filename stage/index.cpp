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
#include "single.h"
#include "lock.h"
#include <unordered_map>
#include <sys/stat.h>
#include "include/index.h"
#include "c++/object.hpp"
#include <list>

DECLARE_LOGGER("index");

namespace index__stage {


#define MAX_TIER            4
struct SCORE__PAIR {
    __off64_t offset[MAX_TIER]; // score offset
}; /* + BSON */


/* */
struct INDEX__STAGE: public ObjectRef {
    static int Compare(const void *l_k, int l_length, const void *r_k, int r_length)
    {
        int8_t *l = (int8_t *)l_k;
        int8_t *r = (int8_t *)r_k;

        if (*(l + 0) == *(r + 0))
            switch (*((int8_t *)l_k))
            {
                case BSON_LONG: /* 큰 값이 상위에 오도록 변경 */
                {
                    int64_t delta = *((int64_t *)(r + 1)) - *((int64_t *)(l + 1));
                    return (delta == 0) ? 0: ((delta < 0) ? 1: -1);
                }
                case BSON_STRING: break;
            }
        {
            int delta = memcmp(l_k, r_k, std::min(l_length, r_length));
            return (delta == 0) ? (l_length - r_length): delta;
        }
    }

    struct SCORE: public ObjectRef {
	    SCORE(const char *fileName, int keySize, int nelem)
                : ObjectRef("INDEX__STAGE::SCORE")
                , timeUpdate(0)
                , iof(NULL)
        {
            if (!(iof = iof_open(fileName, keySize, nelem, f__compare)))
                throw __LINE__;
            else {
                for (int __i = 0; __i <= MAX_TIER; __i++)
                	iop_create(iof, __i);
            }
            INIT_RWLOCK(&rwlock);
        }

        virtual ~SCORE() {
	        {
		        if (iof != NULL)
			        iof_close(iof);
	        } DESTROY_RWLOCK(&rwlock);
        }

        /* */
        RWLOCK_T rwlock;
        std::atomic<std::uint64_t> timeUpdate;

        virtual struct indexable *operator * () { return this->iof; }
    protected:
        virtual void Dispose() { delete this; }

        struct indexable *iof;
    private:
        static int f__compare(struct io__parted *seg,
        		const void *l_k, int l_length, const void *r_k, int r_length, void *udata)
        {
            if (iop_id((struct indexable *)udata, seg) == 0)
                return strcmp((const char *)l_k, (const char *)r_k);
            return Compare(l_k, l_length, r_k, r_length);
        }
    }; typedef std::unordered_map<std::string, SCORE *> SCORE_idSet;

#define PAGE_ITEMS          512
#define KEY_SIZE            128
    INDEX__STAGE(const char *s, Json::Value &V)
        : ObjectRef("INDEX__STAGE")
        , sign((s && *(s + 0)) ? s: "index")
        , _indexPath("")
        , _keySize(KEY_SIZE)
        , _pageItems(PAGE_ITEMS) {
    	if (V.isString()) _indexPath = V.asCString();
    	else if (V.isArray()) for (int x__i = 0, x__p = 0; x__i < (int)V.size(); ++x__i)
	    {
    		Json::Value &v = V[x__i];
    		if (v.isString()) _indexPath = v.asCString();
    		else if (v.isInt()) switch (x__p)
		    {
		        case 0: this->_keySize = v.asInt(); break;
		        case 1: this->_pageItems = v.asInt(); break;
		    }
	    }
        INIT_RWLOCK(&_rwlock);
    }
#undef KEY_SIZE
#undef PAGE_ELEM

    virtual ~INDEX__STAGE() {
        for (SCORE_idSet::iterator it = _scores.begin(); it != _scores.end(); ++it)
            it->second->Unref();
        DESTROY_RWLOCK(&_rwlock);
    }

    void stable(int timeOut)
    {
        utime_t timeNow = utimeNow(NULL);

        ENTER_RWLOCK(&_rwlock);
        for (SCORE_idSet::iterator it = _scores.begin(); it != _scores.end();)
        {
        	SCORE *stage = it->second;

            if ((int)(timeNow - stage->timeUpdate) < timeOut) ++it;
            else if (iof_rebase(**stage) > 0) ++it;
            else {
	            stage->Unref();
                it = _scores.erase(it);
            }
        }
        LEAVE_RWLOCK(&_rwlock);
    }

    SCORE *enter(const char *id)
    {
	    SCORE *index;

        ENTER_RWLOCK(&_rwlock);
        {
	        SCORE_idSet::iterator it = _scores.find(id);
            if (it != _scores.end())
	            index = it->second;
            else {
                std::string __f = (_indexPath + id).c_str();

                try {
                    struct stat st;

                    if (stat(__f.c_str(), &st) < 0)
                    {
                        char *s = (char *)__f.c_str();

                        for (char *p; (p = strchr(s, '/')) != NULL; s = p + 1)
                        {
                            *(p + 0) = 0;
                            if (stat(__f.c_str(), &st) < 0) mkdir(__f.c_str(), 0755);
                            else if (!S_ISDIR(st.st_mode)) throw __LINE__;
                            *(p + 0) = '/';
                        }
                    }

                    it = _scores.insert(
		                    SCORE_idSet::value_type(id, new SCORE(__f.c_str(), this->_keySize, this->_pageItems))
                    ).first;
                } catch (...) { _WARN("%s: %d - %s", id, __LINE__, strerror(errno)) return NULL; }
                (index = it->second)->Ref();
            }
	        index->Ref();
	        index->timeUpdate = utimeNow(NULL);
        } LEAVE_RWLOCK(&_rwlock);
	    return index;
    }

    void leave(SCORE *s) { s->Unref(); }

    std::string sign;
protected:
    virtual void Dispose() { delete this; }

    std::string _indexPath;
	int         _keySize;
	int         _pageItems;

	SCORE_idSet _scores;
    RWLOCK_T    _rwlock;
};


/* */

static void V_toBSON(bson *bo, const char *name, int8_t *v, int v_length)
{
    switch (*((int8_t *)(v + 0)))
    {
        case BSON_LONG: bson_append_long(bo, name, *((int64_t *)(v + sizeof(int8_t)))); break;
        case BSON_STRING:
        {
            bson_append_string_n(bo, name, (char *)(v + sizeof(int8_t)), v_length - sizeof(int8_t));
        } break;
    }
}

#pragma pack(push, 1)
struct BSON_HEAD {
    int32_t length;
    int8_t  type;
};
#pragma pack(pop)

static int I__getTier(struct indexable *F, int tier_id, const char *k, int k_length, bson *bo)
{
    struct SCORE__PAIR *u_v;

    if (iof_find(F, 0, k, k_length, (void **)&u_v) <= 0)
        return -1;
    else {
        auto tierSimple = [bo](int __i, int tier_id, struct io__value *sl) {
            V_toBSON(bo, std::format("#%d", __i).c_str(), (int8_t *)sl->k, sl->k_length); /* index value */
        };

        auto tierFull = [F, bo](int __i, int tier_id, struct io__value *sl)
        {
            bson_append_start_object(bo, std::format("#%d", __i).c_str());
            {
                struct io__cursor cursor;

                if ((ioc_ready(F, tier_id + 1, &cursor) < 0) ||
                        (ioc_find(&cursor, sl->k, sl->k_length) < 0))
                    bson_append_undefined(bo, "d");
                else {
                    __off64_t a_offset;

                    bson_append_long(bo, "d", ioc_fetch(&cursor, NULL, &a_offset));
                }
                V_toBSON(bo, "i", (int8_t *)sl->k, sl->k_length); /* index value */
            }
            bson_append_finish_object(bo);
        };

        const auto buildTier = [tierSimple, tierFull, F, bo](const char *name,
                int tier_id, __off64_t n_offset, std::function<void (int, int, struct io__value *)> buildBSON)
        {
            if (n_offset <= 0)
                bson_append_undefined(bo, name);
            else {
                bson_append_start_array(bo, name);
                {
                    struct io__value sl;

                    for (int __i = 1; n_offset >  0; __i++)
                    {
                        struct io__object *o = iof_object(F, n_offset);

                        if ((o == NULL) || (iof_value(o, &sl) == NULL))
                            bson_append_undefined(bo, std::format("#%d", __i).c_str());
                        else buildBSON(__i, tier_id, &sl);
                        n_offset = *(((__off64_t *)sl.v) + 1);
                    }
                }
                bson_append_finish_array(bo);
            }
        };

        {
            BSON_HEAD *v = (BSON_HEAD *)(u_v + 1);
            bson_append_value(bo, "v", (bson_type)v->type, (const char *)(v + 1), v->length);
        }

        if (tier_id >= 0)
            buildTier("I", tier_id, u_v->offset[tier_id], tierFull);
        else {
#define TIER_FOREACH(I)        for (int I = 0; I < MAX_TIER; I++)
            bson_append_start_array(bo, "I");
            TIER_FOREACH(__x)
                buildTier(std::format("#%d", __x + 1).c_str(), __x, u_v->offset[__x], tierSimple);
#undef TIER_FOREACH
            bson_append_finish_array(bo);
        }
        return tier_id;
    }
}


/*
 * CASE.1: get: [ id, id, id ... ]
 *
 * CASE.2: get: id
 */
static int O__get(INDEX__STAGE::SCORE *stage, int tier_id, const char *b_k, bson_iterator &b_it, bson *bo)
{
    bson_append_start_object(bo, b_k); {
        guard::rdlock __rdlock(&stage->rwlock);

        switch ((int)bson_iterator_type(&b_it))
        {
            case BSON_OBJECT:
            case BSON_ARRAY:
            {
                bson_iterator x_it; bson_iterator_subiterator(&b_it, &x_it);

                while (bson_iterator_next(&x_it) != BSON_EOO)
                {
                    const char *k = bson_iterator_string(&x_it);
                    int k_length = bson_iterator_string_len(&x_it) /* + 1 */;

                    bson_append_start_object(bo, k); {
                        I__getTier(**stage, tier_id, k, k_length, bo);
                    } bson_append_finish_object(bo);
                }
            } break;

            case BSON_STRING:
            {
                const char *k = bson_iterator_string(&b_it);
                int k_length = bson_iterator_string_len(&b_it) /* + 1 */;

                bson_append_start_object(bo, k); {
                    I__getTier(**stage, tier_id, k, k_length, bo);
                } bson_append_finish_object(bo);
            } break;
        }
    } bson_append_finish_object(bo);
    return 0;
}



/* CASE.1 - ranking: [score, count]
 * CASE.2 - ranking: [key, delta, count]
 */
static int O__range(INDEX__STAGE::SCORE *stage, int tier_id, const char *b_k, bson_iterator &b_it, bson *bo)
{
	bson_iterator x_it; bson_iterator_subiterator(&b_it, &x_it);
    bson_append_start_object(bo, b_k); {
        guard::rdlock __rdlock(&stage->rwlock);
        struct io__cursor cursor;

        int delta = 0;
        switch (bson_iterator_next(&x_it))
        {
            case BSON_LONG:
            case BSON_INT:
            {
                std::string buffer;

                buffer.resize(sizeof(int8_t) + sizeof(int64_t));
                {
                    *((int8_t *)(buffer.data() + 0)) = BSON_LONG;
                    *((int64_t *)(buffer.data() + sizeof(int8_t))) = bson_iterator_long(&x_it);
                }
	            if ((ioc_ready(**stage, tier_id + 1, &cursor) < 0) ||
	                    (ioc_lower(&cursor, buffer.data(), buffer.length()) < 0))
                	goto _gE;
            } break;

            case BSON_STRING:
            {
                /* k를 기준으로, 상위 delta만큼 이동 후, count 갯수를 얻는다. */
                const char *k = bson_iterator_string(&x_it);
                int k_length = bson_iterator_string_len(&x_it);

                struct SCORE__PAIR *u_v;
                if (iof_find(**stage, 0, k, k_length, (void **) &u_v) <= 0)
                    goto _gE;
                else {
                    struct io__object *o = iof_object(**stage, u_v->offset[tier_id]);
                    if (o == NULL)
                        goto _gE;
                    else {
                        struct io__value sl;

	                    iof_value(o, &sl);
                        if ((ioc_ready(**stage, tier_id + 1, &cursor) < 0) ||
                                (ioc_find(&cursor, sl.k, sl.k_length) < 0))
                            goto _gE;
                    }
                }

                /* 범위의 상위 랭커수를 적용한다. */
                delta = (bson_iterator_next(&x_it) != BSON_EOO) ? (int)bson_iterator_long(&x_it) : 0;
                if (delta != 0)
                {
                    __off64_t a_offset;

                    int64_t distance = ioc_fetch(&cursor, NULL, &a_offset);
                    if ((distance -= delta) <= 0)
                    {
                        delta += (int)distance;
                        distance = 1;
                    }
	                if ((ioc_ready(**stage, tier_id + 1, &cursor) < 0) ||
	                        (ioc_distance(&cursor, distance) < 0))
                    	goto _gE;
                }
            } break;
            default: goto _gE;
        }

        {
            int count = ((bson_iterator_next(&x_it) != BSON_EOO) ? bson_iterator_long(&x_it): 0);

            do {
                struct io__object *o; __off64_t a__offset;

                int64_t distance = ioc_fetch(&cursor, &o, &a__offset);
                {
                    struct io__value sl;

                    /*
                    * TIER
                    *    __off64_t a__offset;
                    */
                    __off64_t r_offset = *((__off64_t *) iof_value(o, &sl)->v);
                    bson_append_start_object(bo, std::format("#%lld", distance).c_str()); {
                        V_toBSON(bo, "i", (int8_t *)sl.k, sl.k_length);
                        {
                            struct SCORE__PAIR *u_v =
                                    (struct SCORE__PAIR *) iof_value(iof_object(**stage, r_offset), &sl)->v;

                            bson_append_string(bo, "k", (char *)sl.k);
                            {
                                BSON_HEAD *v = (BSON_HEAD *)(u_v + 1);
                                bson_append_value(bo, "v", (bson_type)v->type, (const char *)(v + 1), v->length);
                            }
                            bson_append_long(bo, "d", distance);
                        }
                    } bson_append_finish_object(bo);
                }
            } while (((--count) > 0) && (ioc_next(&cursor) >= 0));
        }
_gE:    ;
    } bson_append_finish_object(bo);
    return 0;
}


/* forEach: [start, count]
*
*/
static int O__forEach(INDEX__STAGE::SCORE *stage, int tier_id, const char *b_k, bson_iterator &b_it, bson *bo)
{
	bson_iterator x_it; bson_iterator_subiterator(&b_it, &x_it);
    bson_append_start_object(bo, b_k);
    switch ((int)bson_iterator_next(&x_it))
    {
        case BSON_EOO: break;
        case BSON_INT:
        case BSON_LONG:
        {
            guard::rdlock __rdlock(&stage->rwlock);

            struct io__cursor cursor;
            if ((ioc_ready(**stage, tier_id + 1, &cursor) < 0) ||
                    (ioc_distance(&cursor, bson_iterator_long(&x_it)) < 0))
                break;
            else {
                /* count == 0 : start와 동일한 값인 경우에만 데이터를 얻는다. */
                int count = (bson_iterator_next(&x_it) != BSON_EOO) ? (int)bson_iterator_long(&x_it) : 0;

                struct io__object *o;
                struct io__value sl;

                do {
                    __off64_t distance = ioc_fetch(&cursor, &o);
                    __off64_t r_offset = *((__off64_t *)iof_value(o, &sl)->v);

                    bson_append_start_object(bo, std::format("#%lld", distance).c_str()); {
                        V_toBSON(bo, "i", (int8_t *)sl.k, sl.k_length);

                        {
                            struct SCORE__PAIR *u_v =
                                    (struct SCORE__PAIR *) iof_value(iof_object(**stage, r_offset), &sl)->v;

                            bson_append_string(bo, "k", (char *)sl.k);
                            {
                                BSON_HEAD *v = (BSON_HEAD *)(u_v + 1);
                                bson_append_value(bo, "v", (bson_type)v->type, (const char *)(v + 1), v->length);
                            }
                        }
                        bson_append_long(bo, "d", distance);

                    } bson_append_finish_object(bo);
                } while (((--count) > 0) && (ioc_next(&cursor) >= 0));
            }
        } break;
    } bson_append_finish_object(bo);
    return 0;
}

/*
 * set: {
 *    "ID": {
 *      "i": score,
 *      "v": { ... },
 *      "I": {
 *       0: score,
 *       1: score,
 *       2: score,
 *       3: score
 *      }
 *   }, ...
 * }
 */
#define TIER_FOREACH(I)		for (int I = 0; I < MAX_TIER; I++)

/* r__offset에 저장된 데이터에서 s__offset로 설정된 값을 d__offset으로 변경한다.
*
*    r__offset		연결 정보를 가지고 있는 main object (SCORE_PAIR가 저장된 위치)
*    s__offset		위치가 이동되는 기존 offset
*    d__offset		변경되는 offset
*/
static void moveOffsetForTier(INDEX__STAGE::SCORE *stage, __off64_t r__offset, __off64_t s__offset, __off64_t d__offset)
{
    struct io__value sl;
    struct SCORE__PAIR *d_v = (struct SCORE__PAIR *)
		    iof_value(iof_object(**stage, r__offset), &sl)->v;

    TIER_FOREACH(__x) if (d_v->offset[__x] == s__offset)
        {
            d_v->offset[__x] = d__offset;
            break;
        }
	iof_touch(**stage, r__offset);
}

static int V_fromBSON(std::stringset &entry, int depth, bson_iterator *b_it)
{
    switch ((int)bson_iterator_type(b_it))
    {
        case BSON_NULL: return 0;
        case BSON_STRING: entry.push_back(std::string());
        {
            std::string &buffer = entry.back();

            buffer.resize(sizeof(int8_t) + bson_iterator_string_len(b_it));
            {
                *((int8_t *)(buffer.data() + 0)) = BSON_STRING;
                memcpy((void *)(buffer.data() + sizeof(int8_t)),
                       bson_iterator_string(b_it), bson_iterator_string_len(b_it));
            }
        } break;

        case BSON_INT:
        case BSON_LONG: entry.push_back(std::string());
        {
            std::string &buffer = entry.back();

            buffer.resize(sizeof(int8_t) + sizeof(int64_t));
            {
                *((int8_t *)(buffer.data() + 0)) = BSON_LONG;
                *((int64_t *)(buffer.data() + sizeof(int8_t))) = bson_iterator_long(b_it);
            }
        } break;

        case BSON_ARRAY: if (depth == 0)
        {
            bson_iterator a_it; bson_iterator_subiterator(b_it, &a_it);

            while (bson_iterator_next(&a_it) != BSON_EOO)
                V_fromBSON(entry, depth + 1, &a_it);
        } break;
    }
    return (int)entry.size();
}


static int O__set(INDEX__STAGE::SCORE *stage, int tier_id, const char *b_k, bson_iterator &b_it, bson *bo)
{
    static struct _TIER {
        std::stringset v;
        __off64_t distance;
    } _TNIL = { std::stringset(), -1 };
	bson_iterator x_it;

	bson_iterator_subiterator(&b_it, &x_it);
    guard::rwlock __rwlock(&stage->rwlock);
    {
        _TIER _T[MAX_TIER];

        for (bson_append_start_object(bo, b_k); bson_iterator_next(&x_it) != BSON_EOO; )
        {
            const char *k = bson_iterator_key(&x_it);
            int k_length = (int)strlen(k) + 1;

            struct SCORE__PAIR *u_v = NULL;
            __off64_t a__offset = -1;

            if (iof_put(**stage, 0,
            		k, k_length, (void **) &u_v, &a__offset, (__off64_t *)-1) < 0)
_gE:            bson_append_undefined(bo, k);
            else {
                TIER_FOREACH(__x) _T[__x] = _TNIL;
                {
                    bson_iterator r_it;

                    for (bson_iterator_subiterator(&x_it, &r_it); bson_iterator_next(&r_it) != BSON_EOO;)
                    {
                        const char *r_k = bson_iterator_key(&r_it);

                        if (strcmp(r_k, "i") == 0)
                        {
                            if (tier_id < 0) goto _gE;

                            V_fromBSON(_T[tier_id].v, 0, &r_it);
                            _T[tier_id].distance = 0;
                        }
                        else if (strcmp(r_k, "v") == 0)
                        {
                            int length = bson_iterator_size(&r_it);

#define EXTRA_LENGTH        (sizeof(struct SCORE__PAIR) + sizeof(struct BSON_HEAD))
                            if ((int)(length + EXTRA_LENGTH) < iof_bufsize(**stage))
                            {
                                BSON_HEAD *v = (BSON_HEAD *)(u_v + 1);

                                v->length = length;
                                v->type = bson_iterator_type(&r_it);
                                memcpy(v + 1, bson_iterator_value(&r_it), length);
                            }
                            else {
                                _WARN(std::format("BSON Overflow: %d >= %d",
                                                  (length + EXTRA_LENGTH), iof_bufsize(**stage)).c_str());
                                goto _gE;
                            }
#undef EXTRA_LENGTH
                        }
                        else if (strcmp(r_k, "I") == 0)
                        {
                            bson_iterator t_it; bson_iterator_subiterator(&r_it, &t_it);

                            TIER_FOREACH(__x)
                                if (bson_iterator_next(&t_it) == BSON_EOO)
                                    break;
                                else {
                                    const char *t_k = bson_iterator_key(&t_it);
                                    int __p = (*(t_k + 0) == '#') ? (strtol(t_k + 1, NULL, 10) - 1): __x;

                                    V_fromBSON(_T[__p].v, 0, &t_it);
                                    _T[__p].distance = 0;
                                }
                        }
                    }
                }

                TIER_FOREACH(__x) if (_T[__x].distance == 0)
                {
                    struct {
                        __off64_t  offset;
                        __off64_t *v;
                        int        keep;
                    } u = { a__offset, u_v->offset + __x, 0 };

                    if (u_v->offset[__x] > 0)
                    {
                        struct io__value sl;
                        __off64_t s_offset = u_v->offset[__x];

                        /*
                         * struct {
                         *    off64_t a__offset;	// data offset
                         *    off64_t n__offset;	// next score offset
                         * };
                         */
                        do {
                            __off64_t d__offset = -1, n__offset;
                            __off64_t *r_v;

                            if (iof_value(iof_object(**stage, s_offset), &sl) == NULL)
                                goto _gE;
                            else {
                                n__offset = *(((__off64_t *)sl.v) + 1);
                                for (std::stringset::iterator v_it = _T[__x].v.begin(); v_it != _T[__x].v.end(); v_it++)
                                    if (INDEX__STAGE::Compare(sl.k, sl.k_length, v_it->data(), (int)v_it->length()) == 0)
                                    {
                                        _T[__x].v.erase(v_it);

                                        u.keep++;
                                        {
                                            if (*(u.v + 0) != s_offset)
                                            {
                                                *(u.v + 0) = s_offset;
	                                            iof_touch(**stage, u.offset);
                                            }

                                            u.offset = s_offset;
                                            u.v = ((__off64_t *)sl.v) + 1;
                                        }
                                        goto _gN;
                                    }
                            }

                            if (iof_remove(**stage, __x + 1,
                                          sl.k, sl.k_length, s_offset, &d__offset, (void **)&r_v) < 0)
	                            goto _gE;
                            else {
                                if (d__offset > 0) /* 기존 데이터가 이동됨 */
                                {
                                    moveOffsetForTier(stage, *(r_v + 0), d__offset, s_offset);
                                    d__offset = -1;
                                }
                            }
_gN:					    s_offset = n__offset;
                        } while (s_offset > 0);
                    }

                    if ((u.keep + _T[__x].v.size()) == 0)
                        u_v->offset[__x] = 0; /* remove */
                    else {
                        for (std::stringset::iterator v_it = _T[__x].v.begin(); v_it != _T[__x].v.end(); v_it++)
                        {
                            __off64_t c_offset = -1, d__offset = -1;
                            __off64_t *r_v;

                            if ((_T[__x].distance = iof_put(**stage, __x + 1,
                            		v_it->data(), (int) v_it->length(), (void **) &r_v, &c_offset, &d__offset)) < 0)
                                goto _gE;
                            else {
                                if (d__offset > 0) /* 기존 데이터가 이동됨 */
                                    moveOffsetForTier(stage, *(r_v + 0), c_offset, d__offset);
                            }

                            // fprintf(stderr, " ** [%s] ADD.%d: %I64d\n", k, __x, *((int64_t *)(v_it->data() + 1)));
                            {
                                *(u.v + 0) = c_offset;
	                            iof_touch(**stage, u.offset);

                                u.offset = c_offset;
                                u.v = r_v + 1;
                            }

                            *(r_v + 0) = a__offset;
                            *(r_v + 1) = 0;
                        }

	                    iof_touch(**stage, u.offset);
                    }
                }

	            iof_touch(**stage, a__offset);
                bson_append_long(bo, k, _T[tier_id].distance);
            }
        } bson_append_finish_object(bo);
    } return 0 /* iof_flush(**stage) */;
}

/*
 * "del": "KEY"
 * "del": [ "KEY0", .. "KEYN" ]
 */
static int I__delete(INDEX__STAGE::SCORE *stage, int tier_id, const char *k)
{
    struct SCORE__PAIR *u_v;
    __off64_t a__offset;

    if (iof_find(**stage, 0, k, (int)strlen(k) + 1, (void **) &u_v, &a__offset) <= 0)
        return -1;
    else {
        auto deleteTier = [stage](int tier_id, __off64_t s_offset)
        {
            struct io__value sl;

            do {
                __off64_t d__offset = -1, n__offset;
                void *r_v;

                if (iof_value(iof_object(**stage, s_offset), &sl) == NULL)
                    return -1;
                else {
                    n__offset = *(((__off64_t *)sl.v) + 1);
                    if (iof_remove(**stage, tier_id, sl.k, sl.k_length, s_offset, &d__offset, &r_v) < 0)
                        return -1;
                    else {
                        if (d__offset > 0) /* 기존 데이터가 이동됨 */
                        {
                            moveOffsetForTier(stage, *((__off64_t *)r_v), d__offset, s_offset);
                            d__offset = -1;
                        }
                    }
                } s_offset = n__offset;
            } while (s_offset > 0);
            return 0;
        };

        if (tier_id < 0)
        {
            TIER_FOREACH(__x) if (u_v->offset[__x] > 0)
                {
                    if (deleteTier(__x + 1, u_v->offset[__x]) < 0)
                        return -1;
                }
            return iof_remove(**stage, 0, (const void *)k, (int)strlen(k) + 1);
        }
        else if ((u_v->offset[tier_id] <= 0) ||
                 (deleteTier(tier_id + 1, u_v->offset[tier_id]) < 0))
            return -1;
        else {
            u_v->offset[tier_id] = 0;
	        iof_touch(**stage, a__offset);
        }
    }

    return 0;
}

static int O__delete(INDEX__STAGE::SCORE *stage, int tier_id, const char *b_k, bson_iterator &b_it, bson *bo)
{
    guard::rwlock __rwlock(&stage->rwlock);

	bson_append_start_object(bo, b_k);
    switch ((int)bson_iterator_type(&b_it))
    {
        case BSON_STRING:
        {
            const char *k = bson_iterator_string(&b_it);

	        bson_append_bool(bo, k, I__delete(stage, tier_id, k) >= 0);
        } break;

        case BSON_ARRAY:
        case BSON_OBJECT:
        {
        	bson_iterator x_it; bson_iterator_subiterator(&b_it, &x_it);
            while (bson_iterator_next(&x_it) != BSON_EOO)
            {
                const char *k = bson_iterator_string(&x_it);
                bson_append_bool(bo, k, (I__delete(stage, tier_id, k) >= 0));
            }
        }
    } bson_append_finish_object(bo);
    return 0 /* iof_flush(**stage) */;
}
#undef TIER_FOREACH


/* */
static int messageCallback(utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr, INDEX__STAGE **__stage)
{
    struct KV : public std::string {
        bson_type   t;
        std::string k;
        std::string i;

        INDEX__STAGE *stage;
        struct sockaddr_stage addr;
        KV(INDEX__STAGE *s, struct sockaddr_stage *a, const char *__i, bson_iterator *it)
                : t(bson_iterator_type(it))
                , stage(s) {

            {
                if (strchr(__i, '?') == NULL)
                    this->k.assign(__i);
                else {
                    const char *p = strchr(__i, '=');
                    if (p == NULL)
                        this->i.assign(__i);
                    else {
                        this->k.assign(p + 1);
                        this->i.assign(__i, p - __i);
                    }
                } this->assign(bson_iterator_value(it), bson_iterator_size(it));
                if (a == NULL)
                    memset((struct sockaddr_stage *)&addr, 0, sizeof(struct sockaddr_stage));
                else {
                    if ((k.length() > 0) &&
                        *(a->ss_stage + 0) && !strchr(k.c_str(), ':'))
                        k = a->ss_stage + (":" + k);
                    memcpy((struct sockaddr_stage *)&addr, a, sizeof(struct sockaddr_stage));
                }
            } stage->Ref();
        }
        ~KV() { stage->Unref(); }
    };

#define STABLE_TIMEOUT              (60 * 1000)
    if (addr == NULL) (*__stage)->Unref();
    else if (bson_it == NULL) (*__stage)->stable(STABLE_TIMEOUT);
#undef STABLE_TIMEOUT
    else if (utimeNow(NULL) < timeBreak)
    {
        /*
         * "STAGE_ID:CMD?INDEX_PATH/SLOT=RETURN_ID": {
         *       ...
         * }
         *
         * "STAGE_ID:RETURN_ID": {
         *    "#SEQ.CMD?INDEX_PATH/SLOT": { ... },
         *    ...
         * }
         */
        const char *__i = bson_iterator_key(bson_it);

        if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) != 0) return 0;
        else if (*(__i += (*__stage)->sign.length()) != ':') return 0;

        stage_submit(0, [](utime_t timeClosure, void *__r) {
            auto run_Index = [](KV *kv, const char *x, bson_iterator b_it, bson *b_o)
            {
                auto strfchr = [](const char *x, ...)
                {
                    const char *p = NULL;
                    {
                        va_list args;

                        va_start(args, x);
                        for (char c; (c = va_arg(args, int)) > 0; )
                            if ((p = strchr(x, c)) != NULL)
                                break;
                        va_end(args);
                    } return p;
                };

                std::string __x = x;
                char *o = (char *)strfchr(__x.c_str(), '.', 0); /* skip #SEQ */
                char *i = (char *)strfchr(o = (o ? o + 1 : (char *)__x.c_str()), '?', ':', 0);
                if (i != NULL) /* #SEQ.CMD:INDEX_PATH/SLOT OR #SEQ.CMD?INDEX_PATH/SLOT */
                {
                    int tier_id = 0;

                    *i++ = 0; /* "INDEX_PATH/SLOT" */
                    {
                        char *s = strrchr(i, '/');
                        if (s != NULL)
                        {
                            *s++ = 0;
                            tier_id = (*(s + 0) == 0) ? -1: (strtol(s, NULL, 10) - 1);
                        }
                    }

                    if (kv->i.empty() == false) x = bson_iterator_key(&b_it); /* direct */

                    INDEX__STAGE::SCORE *stage = kv->stage->enter(i);
                    if (stage == NULL)
                        bson_append_null(b_o, x);
                    else {
                        if ((*(o + 0) == 'g') || (strcmp(o, "get") == 0))          O__get(stage, tier_id, x, b_it, b_o);
                        else if ((*(o + 0) == 's') || (strcmp(o, "set") == 0))     O__set(stage, tier_id, x, b_it, b_o);
                        else if ((*(o + 0) == 'd') || (strcmp(o, "del") == 0))     O__delete(stage, tier_id, x, b_it, b_o);
                        else if ((*(o + 0) == 'f') || (strcmp(o, "forEach") == 0)) O__forEach(stage, tier_id, x, b_it, b_o);
                        else if ((*(o + 0) == 'r') || (strcmp(o, "range") == 0))   O__range(stage, tier_id, x, b_it, b_o);
                        else if ((*(o + 0) == 't') || (strcmp(o, "total") == 0))
                        {
                            int64_t total = iof_count(**stage, tier_id);
#if 0
                            {
                                guard::rdlock __rdlock(&stage->rwlock);
                                struct io__cursor cursor;
                                int64_t count = 0;

                                if (ioc_ready(**stage, tier_id, &cursor) < 0);
                                else if (ioc_distance(&cursor, 1) == 0) do {
                                    ++count;
                                } while (ioc_next(&cursor) == 0);
                                fprintf(stderr, " ** VERIFY: %ld, %ld\n", total, count);
                                if (total != count) abort();
                            }
#endif
                            bson_append_long(b_o, x, total);
                        }

                        kv->stage->leave(stage);
                    }
                }
            };

            KV *kv = (KV *)__r;

            bson_scope b_o;
            if (kv->i.empty() == false) /* 단일 명령 */
            {
                bson_scope b_i;
                bson_iterator b_it;

                {
                    bson_append_value(&b_i, kv->k.c_str(), kv->t, kv->data(), kv->length());
                    bson_finish(&b_i);

                    bson_iterator_init(&b_it, &b_i);
                } run_Index(kv, kv->i.c_str(), b_it, &b_o);
            }
            else {
                bson_iterator b_it;

                bson_append_start_object(&b_o, kv->k.c_str());
                switch (kv->t)
                {
                    case BSON_OBJECT: bson_iterator_from_buffer(&b_it, kv->data());
                    {
                        std::list<bson_iterator> kv_entry;

                        {
                            while (bson_iterator_next(&b_it) != BSON_EOO)
                                kv_entry.push_back(b_it);
                        } kv_entry.sort([](const bson_iterator &l, const bson_iterator &r) {
                            char *l__p = (char *)bson_iterator_key(&l);
                            char *r__p = (char *)bson_iterator_key(&r);
                            long l__i = INT32_MAX;
                            long r__i = INT32_MAX;

                            if (*(l__p + 0) == '#') l__i = strtol(l__p + 1, &l__p, 10);
                            if (*(r__p + 0) == '#') r__i = strtol(r__p + 1, &r__p, 10);

                            if (l__i != r__i) return (l__i < r__i);
                            else if (l__i == INT32_MAX) return (l.cur < r.cur);
                            else return strcmp(l__p, r__p) < 0;
                        });

                        for (;kv_entry.empty() == false; kv_entry.pop_front())
                        {
                            b_it = kv_entry.front();
                            run_Index(kv, bson_iterator_key(&b_it), b_it, &b_o);
                        }
                    } break;

                    case BSON_ARRAY: bson_iterator_from_buffer(&b_it, kv->data());
                    {
                        bson_iterator b_it;

                        while (bson_iterator_next(&b_it) != BSON_EOO)
                            run_Index(kv, bson_iterator_key(&b_it), b_it, &b_o);
                    } break;
                    default: break;
                } bson_append_finish_object(&b_o);
            }

            if (kv->k.empty() == false)
            {
                bson_finish(&b_o);
                if (stage_signal(0, NULL, &b_o, 0, &kv->addr) < 0)
                    _WARN("%d: %s", __LINE__, strerror(errno));
            }
            delete kv;
            return 0;
        }, new KV(*__stage, addr, __i + 1, bson_it));
    } return 3000;
}

/* */
};

EXPORT_STAGE(index)
{
    {
        Json::Value &startup = *((Json::Value *)j__config);
        char *__i = (char *)strchr(stage_id, '+');

        {
            index__stage::INDEX__STAGE *stage =
                    new index__stage::INDEX__STAGE(__i ? __i + 1: NULL, startup);

            stage->Ref();
            (*udata) = stage;
        }
    } (*r__callback) = (stage_callback )index__stage::messageCallback;
    return 0;
}
