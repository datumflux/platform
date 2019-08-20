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
#include "c++/object.hpp"
#include <unordered_map>
#include <list>

DECLARE_LOGGER("route");

namespace route__stage {

struct ROUTE;

struct SUBSCRIBE: public ObjectRef {
    ROUTE *route;
    std::string id;

    SUBSCRIBE(ROUTE *t, const char *i);
    virtual ~SUBSCRIBE();

    virtual SUBSCRIBE *Ref() { ObjectRef::Ref(); return this; }

    ROUTE *operator -> () { return route; }
protected:
    virtual void Dispose() { delete this; }
}; typedef std::unordered_map<uintptr_t, SUBSCRIBE *> SUBSCRIBE_addrSet;
typedef std::unordered_map<std::string, SUBSCRIBE_addrSet *> SUBSCRIBE_nameSet;

/* */
typedef std::unordered_map<std::string, utime_t> READY_nameSet;

struct addrStageCompare : public std::binary_function<struct sockaddr_stage *, struct sockaddr_stage *, bool>
{
    bool operator()(const struct sockaddr_stage *l, const struct sockaddr_stage *r) const
    {
        if (l->ss_length != r->ss_length)
            return (l->ss_length < r->ss_length);
        else {
            int delta = memcmp(l, r, l->ss_length);

            if (delta != 0) return delta < 0;
        } return strcmp(l->ss_stage, r->ss_stage) < 0;
    }
};

struct ROUTE: public ObjectRef
		    , public READY_nameSet {
    struct sockaddr_stage addr;
    utime_t               timeUpdate;

    ROUTE(struct sockaddr_stage *addr)
            : ObjectRef("ROUTE")
            , READY_nameSet()
            , timeUpdate(0) {
        memcpy(&this->addr, addr, sizeof(struct sockaddr_stage));
    }
    virtual ~ROUTE() { }

    virtual ROUTE *Ref() { ObjectRef::Ref(); return this; }
protected:
    virtual void Dispose() { delete this; }
}; typedef std::map<struct sockaddr_stage *, ROUTE *, addrStageCompare> ROUTE_nameSet;


SUBSCRIBE::SUBSCRIBE(ROUTE *t, const char *i)
    : ObjectRef("SUBSCRIBE")
    , route(t)
    , id(i) {
    t->Ref();
}

SUBSCRIBE::~SUBSCRIBE() { route->Unref(); }

/* */
	typedef std::unordered_map<std::string, SUBSCRIBE *> SUBSCRIBE_idSet;
typedef std::map<struct sockaddr_stage */* ROUTE */, SUBSCRIBE_idSet, addrStageCompare> SUBSCRIBE_allowSet;

struct ROUTE__KV: public ObjectRef
		        , public std::string /* 반환 주소 */{
    bson_type   t; /* bson 자료형 */
    std::string k;

    struct sockaddr_stage addr;
    utime_t               timeUpdate;
    SUBSCRIBE_allowSet    allows;   /* 요청을 허용한 id (addr, subscribe_id) */

    ROUTE__KV(bson_iterator *bson_it, struct sockaddr_stage *addr)
            : ObjectRef("ROUTE__KV")
            , std::string()
            , t(bson_iterator_type(bson_it))
            , k(bson_iterator_key(bson_it))
            , timeUpdate(utimeNow(NULL)) {
        {
            this->assign(bson_iterator_value(bson_it), bson_iterator_size(bson_it));
        } memcpy(&this->addr, addr, sizeof(struct sockaddr_stage));
    }

    virtual ~ROUTE__KV()
    {
        for (SUBSCRIBE_allowSet::iterator a_it = allows.begin(); a_it != allows.end(); ++a_it)
        {
        	for (SUBSCRIBE_idSet::iterator
        	        n_it = a_it->second.begin(); n_it != a_it->second.end(); ++n_it)
		        n_it->second->Unref();
        }
    }

	virtual ROUTE__KV *Ref() { ObjectRef::Ref(); return this; }
protected:
	virtual void Dispose() { delete this; }
}; typedef std::deque<ROUTE__KV *> ROUTE_readySet;


struct ROUTE__STAGE: public ObjectRef
		           , public ROUTE_nameSet {
    ROUTE__STAGE(const char *s, Json::Value &V)
            : ObjectRef("ROUTE__STAGE")
            , ROUTE_nameSet()
            , sign((s && *(s + 0)) ? s: "route") {
        INIT_RWLOCK(&rwlock);
    }
    virtual ~ROUTE__STAGE() { DESTROY_RWLOCK(&rwlock); }

    virtual ROUTE__STAGE *Ref() { ObjectRef::Ref(); return this; }

    ROUTE_readySet    readys;
    SUBSCRIBE_nameSet subscribe;

    RWLOCK_T          rwlock;
    std::string       sign;
protected:
    virtual void Dispose() { delete this; }
};

/* */
auto bson_append_args = [](bson *b_o, const char *k, ROUTE__KV *kv)
{
    bson_append_start_array(b_o, k); {
        bson_append_string(b_o, "#0", kv->k.c_str());
        switch (kv->t)
        {
            case BSON_ARRAY:
            {
                bson_iterator arg_it;

                for (bson_iterator_from_buffer(&arg_it, kv->data());
                     bson_iterator_next(&arg_it) != BSON_EOO; )
                {
                    const char *k = bson_iterator_key(&arg_it);

                    if (*(k + 0) == '#')
                    {
                        long i = strtol(k + 1, NULL, 10);

                        if ((i == LLONG_MIN) || (i == LLONG_MAX));
                        else bson_append_element(b_o, std::format("#%d",i + 1).c_str(), &arg_it);
                    }
                }
            } break;
            default: bson_append_value(b_o, "#1", kv->t, kv->data(), kv->size());
        }
    } bson_append_finish_object(b_o);
};


static auto eraseSubscribe = [](ROUTE__STAGE *stage, uintptr_t id, const char *k)
{
    SUBSCRIBE_nameSet::iterator n_it = stage->subscribe.find(k);
    if (n_it == stage->subscribe.end())
        return false;
    else {
        SUBSCRIBE_addrSet::iterator a_it = n_it->second->find(id);
        if (a_it != n_it->second->end())
        {
            a_it->second->Unref();
            n_it->second->erase(a_it);
            if (n_it->second->empty())
                stage->subscribe.erase(n_it);
        }
    }
    return true;
};

static auto failSubscribe = [](ROUTE__KV *kv)
{
    const char *x__r = strchr(kv->k.c_str(), '!');

    if (x__r != NULL)
    {
        bson_scope b_o;

        ++x__r; {
            std::string f__uid = kv->addr.ss_stage;

            f__uid.append(":");
            {
                const char *x__e = strchr(x__r, '=');

                if (x__e)
                    f__uid.append(x__r, x__e - x__r);
                else f__uid.append(x__r);
            } bson_append_args(&b_o, f__uid.c_str(), kv);
        } bson_finish(&b_o);
        if (stage_signal(0, NULL, &b_o, 0, &kv->addr) < 0)
            _WARN("%d: %s", __LINE__, strerror(errno));
    }
};

static auto stableSubscribe = [](ROUTE__STAGE *stage)
{
    utime_t timeNow = utimeNow(NULL);

#define FAIL_TIMEOUT            300
    ENTER_RWLOCK(&stage->rwlock);
    while (stage->readys.empty() == false)
    {
        ROUTE__KV *kv = stage->readys.front();

        if ((timeNow - kv->timeUpdate) < FAIL_TIMEOUT)
            break;
        else {
            failSubscribe(kv);
            kv->Unref();
        } stage->readys.pop_front();
    }

#define STABLE_TIMEOUT          (30 * 1000)
    for (ROUTE_nameSet::iterator i_it = stage->begin(); i_it != stage->end(); )
    {
        ROUTE *v = i_it->second;

        if ((timeNow - v->timeUpdate) < STABLE_TIMEOUT)
            ++i_it;
        else {
            for (READY_nameSet::iterator r_it = v->begin(); r_it != v->end(); ++r_it)
                eraseSubscribe(stage, (uintptr_t)v, r_it->first.c_str());
            v->Unref();

            i_it = stage->erase(i_it);
        }
    }
    LEAVE_RWLOCK(&stage->rwlock);
#undef STABLE_TIMEOUT
#undef FAIL_TIMEOUT
};

static int messageCallback(utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr, ROUTE__STAGE **__stage)
{
    if (addr == NULL) (*__stage)->Unref();
    else if (bson_it == NULL) stableSubscribe(*__stage);
    else if (utimeNow(NULL) < timeBreak)
    {
        /*
         * "@": {
         *    "SUBSCRIBE_ID": CALLBACK_ID,   -- ADD
         *    "SUBSCRIBE_ID": nil,           -- DELETE
         * }
         *
         * "@": null,      -- CLEAR
         * "SUBSCRIBE_ID?CHECK_KEY!FAIL_CALLBACK": VALUE
         *
         *     ==> "CALLBACK_ID": VALUE 로 전달
         *
         * 1) "SUBSCRIPT_ID" 로 데이터 전달 요청
         * 2) 해당 STAGE에 응답을 받을 수 있는지 확인 요청 전송
         *     "@SUBSCRIBE_ID": random_key
         *
         * 3) 응답을 먼저 전송한 "@SUBSCRIBE_ID": random_key 를 먼저 수신한 stage에 데이터 전달
         */
        const char *__i = bson_iterator_key(bson_it);

        if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) ||
                (*(__i += (*__stage)->sign.length()) != ':'))
            return 0;

        bson_scope b_i; auto kv_signal = [&b_i](ROUTE__KV *kv, const char *filter, SUBSCRIBE_idSet &entry) {
            bson b_s = b_i;

            bson_append_start_object(&b_s, "r"); {
                bson_append_binary(&b_s, "i",
                        BSON_BIN_BINARY, (const char *)&kv->addr, kv->addr.ss_length);
                bson_append_string(&b_s, "s", kv->addr.ss_stage);
            } bson_append_finish_object(&b_s);

        	std::stringset x__filter; (filter ? std::tokenize(filter, x__filter, ","): 0);
        	for (SUBSCRIBE_idSet::iterator i_it = entry.begin(); i_it != entry.end(); ++i_it)
	        {
        		SUBSCRIBE *s = i_it->second;

        		if (x__filter.empty() ||
        		        (std::find(x__filter.begin(), x__filter.end(), s->id.c_str()) != x__filter.end()))
		        {
			        bson b_o = b_s;

			        bson_append_start_object(&b_o, "v"); {
				        std::string id = s->id.c_str();
				        {
					        const char *x__r = strchr(kv->k.c_str(), '=');
					        if (x__r != NULL) /* id에 설정된 return_callback을 교체한다. */
					        {
						        const char *x__e = strchr(id.c_str(), '=');
						        if (x__e != NULL) id.resize(x__e - id.c_str());

						        if (!(x__e = strchr(x__r, '!'))) id.append(x__r);
						        else id.append(x__r, x__e - x__r);
					        }
				        }
				        if (id.find(":") != std::string::npos);
				        else id.insert(0, ":").insert(0, s->route->addr.ss_stage);
				        bson_append_args(&b_o, id.c_str(), kv);
			        } bson_append_finish_object(&b_o);

			        bson_finish(&b_o);
			        if (stage_signal(-1, NULL, &b_o, 0, &s->route->addr) < 0)
			            _WARN("%d: %s", __LINE__, strerror(errno));
		        }
	        }
        };

        bson_ensure_space(&b_i, BUFSIZ);
        switch (*(++__i))
        {
            default:
            {
                auto x__split = [](std::string &v) {
                    char *x__r[] = {
                            (char *)strchr(v.c_str(), '?'),
                            (char *)strchr(v.c_str(), '!'),
                            (char *)strchr(v.c_str(), '=')
                    }, *p = x__r[0];

                    for (int __x = sizeof(x__r) / sizeof(x__r[0]); (--__x) > 0; )
                        if (x__r[__x] && ((p == NULL) || (p > x__r[__x]))) p = x__r[__x];

                    if (p == NULL)
                        return v.length();
                    else {
                        *(p + 0) = 0;
                    } return v.length() - (p - v.c_str());
                };

                ROUTE__KV *kv = new ROUTE__KV(bson_it, addr);
                std::string x__f = __i + (strchr("*=+", *(__i + 0)) != NULL);
                {
                    utime_t timeNow = utimeNow(NULL);

                    std::stringset x__any; std::tokenize(x__f.c_str(), x__any, ",");
                    ENTER_RDLOCK(&(*__stage)->rwlock);
                    for (std::stringset::iterator x__it = x__any.begin(); x__it != x__any.end(); ++x__it)
                    {
	                    int x__i = x__split((*x__it));

_gW:                    SUBSCRIBE_nameSet::iterator s_it = (*__stage)->subscribe.find(x__it->c_str());
                        if (s_it == (*__stage)->subscribe.end())
_gR:                        for (char *x__p = (char *)x__it->data(); (--x__i) >= 0; )
                            {
                                if (*(x__p + x__i) == '.')
                                {
                                    *(x__p + (x__i + 1)) = 0;
                                    goto _gW;
                                }
                            }
                        else {
                            for (SUBSCRIBE_addrSet::iterator
                                         a_it = s_it->second->begin(); a_it != s_it->second->end(); ++a_it)
                            {
                                SUBSCRIBE *s = a_it->second;

                                if (s->route->timeUpdate == 0);
                                else if (s->route->timeUpdate <= timeNow)
                                {
                                    READY_nameSet::iterator r_it = s->route->find(x__it->c_str());
                                    if (r_it == s->route->end());
                                    else if ((r_it->second == 0) || (r_it->second > timeNow));
                                    else {
	                                    SUBSCRIBE_allowSet::iterator a_it = kv->allows.find(&s->route->addr);

                                    	if (a_it == kv->allows.end())
                                    		a_it = kv->allows.insert(
				                                    SUBSCRIBE_allowSet::value_type(&s->route->addr, SUBSCRIBE_idSet())
		                                    ).first;

                                    	if (a_it->second.find(s->id.c_str()) == a_it->second.end())
                                    		a_it->second.insert(SUBSCRIBE_idSet::value_type(s->id.c_str(), s->Ref()));
                                    }
                                }
                            }

                            if (kv->allows.empty()) goto _gR;
                        }
                    } LEAVE_RDLOCK(&(*__stage)->rwlock);
                }

                switch (*(__i + 0))
                {
                    case '*':
                    {
                        for (SUBSCRIBE_allowSet::iterator
                                a_it = kv->allows.begin(); a_it != kv->allows.end(); ++a_it)
                            kv_signal(kv, NULL, a_it->second);

_gD:                    kv->Unref();
                    } break;

                    default:
                    {
                        /* 메시지를 처리할 수 있는 stage를 찾기위해 등록된 모든 stage로 전송 */
                        for (SUBSCRIBE_allowSet::iterator a_it = kv->allows.begin(); a_it != kv->allows.end(); )
                        {
                            bson b_o = b_i;

                            bson_append_start_array(&b_o, std::format("%s:%s:@=%s:=", a_it->first->ss_stage,
                                    (*__stage)->sign.c_str(), (*__stage)->sign.c_str()).c_str()); {
                                bson_append_string(&b_o, "#1", __i); /* 발생된 id */
                                bson_append_string(&b_o, "#2", std::format("%p+%d",
                                        kv, std::distance(kv->allows.begin(), a_it)).c_str());
                            } bson_append_finish_array(&b_o);
                            bson_finish(&b_o);

                            if (stage_signal(1, NULL, &b_o, 0, a_it->first) >= 0)
                                ++a_it;
                            else {
                            	for (SUBSCRIBE_idSet::iterator
	                            	    i_it = a_it->second.begin(); i_it != a_it->second.end(); ++i_it)
                                    i_it->second->Unref();

                                a_it = kv->allows.erase(a_it);
                                _WARN("%d: %s", __LINE__, strerror(errno));
                            }
                        }

                        if (kv->allows.empty() == false)
                            ENTER_RWLOCK(&(*__stage)->rwlock);
                        else {
                            failSubscribe(kv);
                            goto _gD;
                        }

                        (*__stage)->readys.push_back(kv->Ref());
                    } LEAVE_RWLOCK(&(*__stage)->rwlock);
                }
            } break;

            /* 처리 여부 조회에 대한 stage의 반환 결과: "=ROUTE_KV_POINT" */
            case '=': if (bson_iterator_type(bson_it) == BSON_STRING)
            {
                char *p__i = NULL;
                ROUTE__KV *kv = (ROUTE__KV *)strtoul(bson_iterator_string(bson_it), &p__i, 16);
                if ((p__i == NULL) || (*(p__i + 0) != '+')) break;
                {
                    ENTER_RWLOCK(&(*__stage)->rwlock);
                    for (ROUTE_readySet::iterator
                                 r_it = (*__stage)->readys.begin(); r_it != (*__stage)->readys.end(); ++r_it)
                        if (kv == (*r_it))
                        {
                            SUBSCRIBE_allowSet::iterator a_it = kv->allows.begin();
                            char *p__f = NULL;

                            std::advance(a_it, strtol(p__i + 1, &p__f, 10));
                            if (a_it != kv->allows.end())
                            {
                            	(*__stage)->readys.erase(r_it);
                                LEAVE_RWLOCK(&(*__stage)->rwlock); {
                                    kv_signal(kv, (*(p__f + 0) == '?') ? (p__f + 1): NULL, a_it->second);
                                } kv->Unref();

                                goto _gO;
                            }
                        }
                } LEAVE_RWLOCK(&(*__stage)->rwlock);
            } break;

            /* 전달 받을 이벤트를 등록한다. */
            case '@': ENTER_RWLOCK(&(*__stage)->rwlock);
            {
                ROUTE *v = NULL;

                ROUTE_nameSet::iterator it = (*__stage)->find(addr);
                if (it == (*__stage)->end())
                    switch (bson_iterator_type(bson_it))
                    {
                        case BSON_OBJECT:
                        {
                            if (*(addr->ss_stage + 0) == 0)
                        default: goto _gE;
                            else {
                                (v = new ROUTE(addr))->Ref();

                                it = (*__stage)->insert(
                                        ROUTE_nameSet::value_type(&v->addr, v)
                                ).first;
                            }
                        } break;
                    }
                else v = it->second;

                v->timeUpdate = utimeNow(NULL);
                switch (bson_iterator_type(bson_it))
                {
	                default: break;

                    case BSON_NULL:
                    {
                        for (READY_nameSet::iterator r_it = v->begin(); r_it != v->end(); ++r_it)
                            eraseSubscribe(*__stage, (uintptr_t)v, r_it->first.c_str());
                    } break;

                    case BSON_INT:
                    case BSON_LONG:
                    {
                        v->timeUpdate += bson_iterator_int(bson_it);
                    } break;
#if 0
                    case BSON_BOOL:
                    {
                        if (bson_iterator_bool(bson_it))
                            v->timeUpdate = 0;
                    } break;
#endif
                    case BSON_OBJECT:
                    {
                        bson_iterator sub_it; bson_iterator_subiterator(bson_it, &sub_it);
                        while (bson_iterator_next(&sub_it) != BSON_EOO)
                        {
                            const char *k = bson_iterator_key(&sub_it);

                            switch ((int)bson_iterator_type(&sub_it))
                            {
                                case BSON_NULL:
                                {
                                    READY_nameSet::iterator r_it = v->find(k);
                                    if (r_it == v->end())
                                        break;
                                    else {
                                        eraseSubscribe(*__stage, (uintptr_t)v, k);
                                    }
                                    v->erase(r_it);
                                } break;

                                case BSON_BOOL: /* on/off */
                                {
                                    READY_nameSet::iterator r_it = v->find(k);
                                    if (r_it != v->end())
                                        r_it->second = bson_iterator_bool(&sub_it) ? v->timeUpdate: 0;
                                } break;

                                case BSON_INT:
                                case BSON_LONG:
                                {
                                    READY_nameSet::iterator r_it = v->find(k);
                                    if (r_it != v->end())
                                        r_it->second = v->timeUpdate + bson_iterator_int(&sub_it);
                                } break;

                                case BSON_STRING:
                                {
                                    READY_nameSet::iterator r_it = v->find(k);
                                    if (r_it != v->end())
                                        break;
                                    else {
                                        SUBSCRIBE_nameSet::iterator a_it = (*__stage)->subscribe.find(k);
                                        if (a_it == (*__stage)->subscribe.end())
                                            a_it = (*__stage)->subscribe.insert(
                                                    SUBSCRIBE_nameSet::value_type(k, new SUBSCRIBE_addrSet())
                                            ).first;

                                        {
                                            SUBSCRIBE *s = new SUBSCRIBE(v, bson_iterator_string(&sub_it));

                                            a_it->second->insert(
                                                    SUBSCRIBE_addrSet::value_type((uintptr_t)v, s->Ref())
                                                 );
                                        }
                                    } v->insert(READY_nameSet::value_type(k, v->timeUpdate));
                                } break;
                            }
                        }
                    } break;
                }

                if (v->empty())
                {
                    (*__stage)->erase(it);
                    v->Unref();
                }
_gE:            ;
            } LEAVE_RWLOCK(&(*__stage)->rwlock);
        }

_gO:    ;
#define IDLE_TIMEOUT            100
    } return IDLE_TIMEOUT;
}

/* */
static pthread_once_t ROUTE__initOnce = PTHREAD_ONCE_INIT;

static void ROUTE__atExit() { }
static void ROUTE__atInit()
{
    try {
        ;
    } catch (...) { }
    atexit(ROUTE__atExit);
}

};

EXPORT_STAGE(route)
{
    pthread_once(&route__stage::ROUTE__initOnce, route__stage::ROUTE__atInit); {
        Json::Value &startup = *((Json::Value *)j__config);
        char *__i = (char *)strchr(stage_id, '+');
        {
            route__stage::ROUTE__STAGE *stage =
                    new route__stage::ROUTE__STAGE(__i ? __i + 1: NULL, startup);

            (*udata) = stage->Ref();
        }
    } (*r__callback) = (stage_callback)route__stage::messageCallback;
    return 0;
}