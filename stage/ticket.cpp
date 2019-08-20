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
#include <list>
#include <pqueue.h>
#include "c++/object.hpp"

DECLARE_LOGGER("ticket");

/*
 * ticket
 *   처리가 가능한 이벤트를 할당 받아 진행. 이를 통해 데이터의 처리량을 통제
 */
namespace ticket__stage {

struct TICKET__STAGE: public ObjectRef {

    struct Ticket;
    struct Category;

    struct Callback: public ObjectRef
    		       , public std::string {

        static std::string toString(struct sockaddr_stage *addr, const char *k)
        {
            std::string id = k;
            if (id.find(":") == std::string::npos)
                id.insert(0, ":").insert(0, addr->ss_stage);
            return id;
        }

        Callback(struct sockaddr_stage *addr, bson_iterator bson_it, const char *id = NULL)
            : ObjectRef("TICKET__STAGE::Callback")
            , std::string()
            , timeUpdate(0)
            , ticket_(NULL)
            , category_(NULL) {

            {
                int size = bson_iterator_size(&bson_it);

                arg_.resize(sizeof(bson_type) + size); {
                    bson_type *b = (bson_type *)arg_.data();

                    *(b + 0) = bson_iterator_type(&bson_it);
                    memcpy(b + 1, bson_iterator_value(&bson_it), size);
                } this->assign((id ? id: toString(addr, bson_iterator_key(&bson_it))));
            } memcpy(&this->addr, addr, sizeof(struct sockaddr_stage));
        }
        virtual ~Callback() { }

        virtual int Pump(TICKET__STAGE *stage, Category *category)
        {
            if (ticket_) return -1; /* 이미 할당 됨 */
            else if (category_ != NULL)
            {
                if (category_->maxTickets == 0) return -1;
                else if (category && (category != category_))
                    return -1;
            }
            else if (category == NULL) return -1;
            else {
                (category_ = category)->Ref();
	            category_->readys.insert(
	                    ActiveSet::value_type((uintptr_t)this, this->Ref())
	            );
            }

            if (category_->tickets.empty()) return (category_->maxTickets > 0) ? 0: -1;
            {
                {
                    (this->ticket_ = category_->tickets.front())->Ref();
                } category_->tickets.pop_front();

                _INFO("TICKET %d: %s GRANT - '%s'", getpid(), ticket_->c_str(), this->c_str());
            } return this->signal(stage->sign.c_str());
        }

        virtual int Cancel(TICKET__STAGE *stage, struct sockaddr_stage *addr = NULL)
        {
            if (category_ == NULL) return -1;
            else if (addr && (*this != addr)) return 0;
            {
                this->timeUpdate = 0;
                if (ticket_)
                {
                    _INFO("TICKET %d: %s REVOKE - '%s'", getpid(), ticket_->c_str(), this->c_str());
                    if ((utimeNow(NULL) < category_->timeExpire) &&
                            ((int)category_->tickets.size() < std::abs(category_->maxTickets)))
                        category_->tickets.push_back(ticket_);
                    else {
                        if (stage->tickets.erase(ticket_->c_str()))
                            ticket_->Unref();
                    } ticket_->Unref();

                    this->ticket_ = NULL;
                }

                if (category_->readys.erase((uintptr_t)this))
                    this->Unref();
                category_->Unref();
            } category_ = NULL;
            return 1;
        }

        utime_t timeUpdate; /* 갱신 시간 */
        struct sockaddr_stage addr;

        virtual int signal(const char *stage_id) {
            bson_scope b_o;
            {
                bson_type *b_type = (bson_type *)this->arg_.data();

                bson_append_start_array(&b_o, std::format("%s=%s:@%p", this->c_str(), stage_id, this).c_str()); {
                    bson_append_string(&b_o, "._TICKET", std::format("@%p", this).c_str());
                    switch (*(b_type + 0))
                    {
                        case BSON_ARRAY:
                        {
                            bson_iterator arg_it; bson_iterator_from_buffer(&arg_it, (char *)b_type + 1);
                            while (bson_iterator_next(&arg_it) != BSON_EOO)
                            {
                                const char *k = bson_iterator_key(&arg_it);
                                if (*(k + 0) == '#')
                                    bson_append_element(&b_o, k, &arg_it);
                            }
                        } break;
                        default: bson_append_value(&b_o, "#0",
                                *(b_type + 0), (char *)(b_type + 1), arg_.length() - sizeof(bson_type));
                    }
                } bson_append_finish_array(&b_o);
            } bson_finish(&b_o);
            return stage_signal(1, NULL, &b_o, 0, &this->addr);
        }

        virtual Callback *Ref() { ObjectRef::Ref(); return this; }

        /* */
        virtual bool operator == (struct sockaddr_stage *addr) {
            if (this->addr.ss_length != addr->ss_length);
            else if (memcmp(&this->addr, addr, addr->ss_length) == 0)
                return strcmp(this->addr.ss_stage, addr->ss_stage) == 0;
            return false;
        }

        virtual bool operator != (struct sockaddr_stage *addr) { return !this->operator==(addr); }

        static int __lessThen(const void *l, const void *r, void *u) {
            int delta = ((Callback *)l)->timeUpdate - ((Callback *)r)->timeUpdate;
            return (delta == 0) ? (((intptr_t)l) < ((intptr_t)r)): (delta < 0);
        }
    protected:
        virtual void Dispose() { delete this; }

    private:
        Ticket   *ticket_;     /* 할당 받은 티켓 */
        Category *category_;

        std::string arg_;
    };

    typedef std::deque<TICKET__STAGE::Callback *> CallbackQueue;

    /* */
    struct Ticket: public ObjectRef
    		     , public std::string {
        Ticket(Category *c, const char *i = "")
                : ObjectRef("TICKET__STAGE::Ticket")
                , std::string(i)
                , category(c) { category->Ref(); }
        virtual ~Ticket() { category->Unref(); }

        virtual Ticket *Ref() { ObjectRef::Ref(); return this; }

	    Category *category;
    protected:
        virtual void Dispose() { delete this; }
    }; typedef std::deque<Ticket *> TicketQueue;

    /* */
    typedef std::unordered_map<uintptr_t, Callback *> ActiveSet;

    struct Category: public ObjectRef {
        Category(utime_t expire = UINT64_MAX)
            : ObjectRef("TICKET__STAGE::Category")
            , timeExpire(expire)
            , maxTickets(0) {
        }
        virtual ~Category() {
            for (TicketQueue::iterator r_it = tickets.begin(); r_it != tickets.end(); ++r_it)
                (*r_it)->Unref();
            for (ActiveSet::iterator a_it = readys.begin(); a_it != readys.end(); ++a_it)
                a_it->second->Unref();
        }

        TicketQueue tickets;    /* 사용 가능한 티켓 */
        ActiveSet   readys;

        std::atomic<std::uint64_t> timeExpire;
        std::atomic<std::int32_t> maxTickets;
    protected:
        virtual void Dispose() { delete this; }
    }; typedef std::unordered_map<std::string, Category *> CategorySet;

    typedef std::unordered_map<std::string, Ticket *> TicketSet;

    ActiveSet   actives;

    pq_pState   reverts;            /* 발급 해제 예정 티켓 */
    TicketSet   tickets;            /* 전체 티켓 정보 */
    CategorySet categorys;          /* 접근 카테고리 */

    virtual bool createCategory(const char *k, int maxTickets, utime_t timeExpire)
    {
        Category *category;

        CategorySet::iterator c_it = this->categorys.find(k);
        if (c_it != categorys.end())
            (category = c_it->second)->timeExpire = timeExpire;
        else {
            {
	            (category = new Category(timeExpire))->Ref();
            } categorys.insert(CategorySet::value_type(k, category));
        }

        {
            int32_t delta = std::abs(maxTickets) - std::abs(category->maxTickets);

            category->maxTickets = maxTickets;
            if (delta >= 0) while ((--delta) >= 0)
            {
                Ticket *t = new Ticket(category);

#define MAX_TICKET_LENGTH               12
                do {
                    t->assign(std::random(MAX_TICKET_LENGTH));
                } while (tickets.find(t->c_str()) != tickets.end());
#undef MAX_TICKET_LENGTH
                category->tickets.push_back(t->Ref());
                tickets.insert(TicketSet::value_type(t->c_str(), t));
            }
            else while (category->tickets.empty() == false)
            {
                Ticket *t = category->tickets.front();

                {
                    tickets.erase(t->c_str());
                } t->Unref();
                category->tickets.pop_front();
                if ((++delta) >= 0) break;
            }
        }
        _INFO("TICKET %d: READY - ['%s' %d]", getpid(), k, (int)category->tickets.size());
        return true;
    }

#define MAX_ACTIVITY_QUEUE          10000
    TICKET__STAGE(const char *s, Json::Value &V)
            : ObjectRef("TICKET__STAGE")
            , sign((s && *(s + 0)) ? s: "ticket") {
        /*
         * [ ACTIVITY_QUEUE, { "category": LIMIT, ... } ]
         */
        int n_activity = MAX_ACTIVITY_QUEUE;

        INIT_RWLOCK(&rwlock);
        if (V.isArray())
            for (int __x = 0; __x < (int)V.size(); ++__x)
            {
                Json::Value &v = V[__x];

                if (v.isObject())
                    for (auto k: v.getMemberNames())
                    {
                        Json::Value &c = v[k];

                        if (c.isInt()) createCategory(k.c_str(), c.asInt(), UINT64_MAX);
                    }
                else if (v.isInt()) n_activity = v.asInt();
            }
        else if (V.isInt()) n_activity = V.asInt();
        if (!(reverts = pq_init(n_activity, Callback::__lessThen, this))) throw __LINE__;
    }
#undef MAX_ACTIVITY_QUEUE
    virtual ~TICKET__STAGE() { DESTROY_RWLOCK(&rwlock); }

    RWLOCK_T    rwlock;
    std::string sign;
protected:
    virtual void Dispose() { delete this; }
};

/* */
struct addrStageCompare : public std::binary_function<struct sockaddr_stage *, struct sockaddr_stage *, bool>
{
    bool operator()(const struct sockaddr_stage *l, const struct sockaddr_stage *r) const
    {
        if (l->ss_length == r->ss_length)
            return memcmp(l, r, l->ss_length) < 0;
        return (l->ss_length < r->ss_length);
    }
};


static void Cleanup(TICKET__STAGE *stage)
{
    typedef std::map<struct sockaddr_stage *, TICKET__STAGE::CallbackQueue, addrStageCompare> X__StageSet;

    utime_t timeNow = utimeNow(NULL);
    struct X__SUBMIT: public X__StageSet {
        TICKET__STAGE *stage;

        X__SUBMIT(TICKET__STAGE *stage)
            : X__StageSet()
            , stage(stage) { stage->Ref(); }
        virtual ~X__SUBMIT() { stage->Unref(); }
    } *entry = new X__SUBMIT(stage);

#define TICKET_EXPIRE           1000
    ENTER_RWLOCK(&stage->rwlock);
	{
		TICKET__STAGE::Callback *u;

	    for (pq_adjust(stage->reverts);
	            (u = (TICKET__STAGE::Callback *)pq_top(stage->reverts)) != NULL;)
	    {
	        if (u->timeUpdate > timeNow) break;
	        else if (u->timeUpdate == 0) pq_pop(stage->reverts);
	        else {
	            pq_pop(stage->reverts);
	            {
	                TICKET__STAGE::ActiveSet::iterator u_it = stage->actives.find((uintptr_t)u);
	                if (u_it == stage->actives.end())
	                    ;
	                else {
	                    if (u->Pump(stage, NULL) < 0)
	                        u->Cancel(stage);
	                    else {
	                        {
	                            u->timeUpdate = timeNow + TICKET_EXPIRE;
	                            pq_push(stage->reverts, u);
	                        } continue;
	                    }

	                    u->Unref();
	                    stage->actives.erase(u_it);
	                }
	            }

	            {
	                X__SUBMIT::iterator c_it = entry->find(&u->addr);
	                if (c_it == entry->end())
	                    c_it = entry->insert(
	                            X__SUBMIT::value_type(&u->addr, TICKET__STAGE::CallbackQueue())
	                         ).first;

	                c_it->second.push_back(u);
	            } continue;
	            /* _WARN("TICKET %d: CANCEL - '%s'", getpid(), u->c_str()); */
	        } u->Unref();
	    }
	}

    for (TICKET__STAGE::CategorySet::iterator c_it = stage->categorys.begin(); c_it != stage->categorys.end(); )
    {
        TICKET__STAGE::Category *category = c_it->second;

        if (category->timeExpire > timeNow)
            ++c_it;
        else {
            category->maxTickets = 0;
            while (category->tickets.empty() == false)
            {
                TICKET__STAGE::Ticket *t = category->tickets.front();

                {
                    stage->tickets.erase(t->c_str());
                } t->Unref();
                category->tickets.pop_front();
            }
            category->Unref();

            _WARN("CATEGORY %d: DESTROY - '%s'", getpid(), c_it->first.c_str());
            c_it = stage->categorys.erase(c_it);
        }
    } LEAVE_RWLOCK(&stage->rwlock);
#undef TICKET_EXPIRE

	if (entry->empty() == false)
		stage_submit((uintptr_t)stage, [](utime_t x__i, void *x__u) {
			X__SUBMIT *entry = (X__SUBMIT *)x__u;
			std::string k = std::format("%s:@", entry->stage->sign.c_str()).c_str();

			TICKET__STAGE::CallbackQueue dq;
			for (X__SUBMIT::iterator f_it = entry->begin(); f_it != entry->end(); ++f_it)
			{
				struct sockaddr_stage *addr = f_it->first;

				bson_scope b_o;
				{
					std::string sign = *(addr->ss_stage + 0) ? std::format("%s:", addr->ss_stage): "";

					bson_append_start_array(&b_o, (sign + k).c_str()); {
						TICKET__STAGE::CallbackQueue &cq = f_it->second;

						bson_append_string(&b_o, "1", "cancel");
						bson_append_start_object(&b_o, "2");
						for (;cq.empty() == false; cq.pop_front())
						{
							TICKET__STAGE::Callback *c = cq.front();

							bson_append_string(&b_o, std::format("@%p", c).c_str(), c->c_str());
							dq.push_back(c);
						}
						bson_append_finish_object(&b_o);
					} bson_append_finish_array(&b_o);
				} bson_finish(&b_o);
				if (stage_signal(0, NULL, &b_o, 0, addr) < 0)
					_WARN("%d: %s", __LINE__, strerror(errno));
			}

			for (;dq.empty() == false; dq.pop_front())
				dq.front()->Unref();
			delete entry;
			return 0;
		}, entry);
	else delete entry;
}

static int messageCallback(utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr, TICKET__STAGE **__stage)
{
    if (addr == NULL) (*__stage)->Unref();
    else if (bson_it == NULL) Cleanup(*__stage);
    else if (utimeNow(NULL) < timeBreak)
    {
        struct BSON__X {
            std::function<int (bson *, const char *k)> begin;
            std::function<int (bson *, const char *k, int r, TICKET__STAGE::Callback *u)> set;
            std::function<int (bson *)> end;
        };

        /*
         * 제한된 티켓을 할당 받아 작업을 수행하도록 한다. 예를 들어, 대량의 시간이 예상되는 로그인의 처리와 같은..
         *
         * 필요한 티켓을 받는다.
         *
         *    생성
         *       "@categroy_id": [LIMIT, CLOSE_TIME]
         *
         *       LIMIT: > 0 - 티켓이 없는 경우 대기
         *              < 0 - 티켓이 없는 경우 취소
         *
         *       CLOSE_TIME: 카테고리가 활성화 되는 시간
         *
         *    방송
         *       "=category_id": value
         *
         *    신청
         *       CASE.1 "categroy_id?feed_callback": {
         *          "callback_id": value,
         *          ...
         *       }
         *
         *       CASE.2 "category_id=callback_id?feed_callback": value
         *
         *    취소
         *       "ticket_id": 0
         *
         *    사용 연장
         *       "ticket_id": expand_msec
         *
         *    stage.waitfor("feedback_callback", function (result)
         *          --- result
         *          ---    true   이미  실행 중
         *          ---    false  실행 할수 없음
         *          ---    @0x... 대기열 아이디
         *    end)
         *
         *    -> 순서가 되면 티켓을 발행 한다.
         *    stage.waitfor("callback_id", function (value)
         *          -- _TICKET   할당된 티켓 번호
         *          stage.signal(nil, 1000);   -- 사용 시간 연장
         *    end)
         *
         *    사용 신호가 1초 이상 없는 경우 자동 해제
         */
        const char *__i = bson_iterator_key(bson_it);

        if ((*__stage)->sign.length() == 0);
        else if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) != 0) return 0; /* NOT */
        else if (*(__i += (*__stage)->sign.length()) != ':') return 0;
        else ++__i;

        auto createTicket = [&addr, &__stage](TICKET__STAGE::Category *category, utime_t timeUpdate,
                const char *callback_id, bson_iterator *callback_value, TICKET__STAGE::Callback **u)
        {
            int r;

            ((*u) =
                    new TICKET__STAGE::Callback(addr, *callback_value, callback_id)
               )->timeUpdate = timeUpdate;

            if ((r = (*u)->Pump((*__stage), category)) >= 0)
                pq_push((*__stage)->reverts, (*u)->Ref());
            else {
                {
                    (*u)->Cancel((*__stage));
                } (*u) = NULL;
                return r;
            } (*__stage)->actives.insert(
            		TICKET__STAGE::ActiveSet::value_type((uintptr_t)(*u), (*u)->Ref())
            );
            return r;
        };

        char *callback_id = (char *)strrchr(__i, '=');
#define TICKET_EXPIRE               1000
        if (callback_id == NULL) switch (bson_iterator_type(bson_it))
        {
            case BSON_ARRAY: if (*(__i + 0) == '@') /* create category */
            {
                int maxTicket = 0;
                uint64_t timeExpire = UINT64_MAX;

                bson_iterator bson_a; bson_iterator_subiterator(bson_it, &bson_a);
                for (int x__i = 0; bson_iterator_next(&bson_a) != BSON_EOO; )
                    switch (bson_iterator_type(&bson_a))
                    {
                        case BSON_LONG:
                        case BSON_INT: switch (x__i)
                        {
                            case 0: maxTicket = bson_iterator_long(&bson_a); break;
                            case 1: timeExpire = utimeNow(NULL) + bson_iterator_long(&bson_a);
                        } ++x__i;
                        default: break;
                    }

                ENTER_RWLOCK(&(*__stage)->rwlock); {
		            (*__stage)->createCategory(__i + 1, maxTicket, timeExpire);
                } LEAVE_RWLOCK(&(*__stage)->rwlock);
            } break;

            case BSON_LONG: if (*(__i + 0) == '@') /* keep alive */
            {
	            ENTER_RWLOCK(&(*__stage)->rwlock);
                TICKET__STAGE::ActiveSet::iterator u_it = (*__stage)->actives.find(strtoll(__i + 1, NULL, 16));
                if (u_it == (*__stage)->actives.end())
                    ;
                else {
                    TICKET__STAGE::Callback *u = u_it->second;

                    int timeExpand = bson_iterator_int(bson_it);
                    if (timeExpand > 0)
                        u->timeUpdate = utimeNow(NULL) + timeExpand;
                    else {
                        u->Cancel((*__stage));
                        {
                            (*__stage)->actives.erase(u_it);
                        } u->Unref();
                    }
                } LEAVE_RWLOCK(&(*__stage)->rwlock);
            } break;

            case BSON_OBJECT: /* CASE.1 */
            {
                char *feedback_id = (char *)strrchr(__i, '?');
                static BSON__X B__X[] = {
                    {
                        [](bson *o, const char *k) { return 0; },
                        [](bson *o, const char *k, int r, TICKET__STAGE::Callback *u) { return 0; },
                        [](bson *o) { return 0; }
                    },
                    {
                        [&addr, &bson_it](bson *o, const char *k) {
                            bson_append_start_array(o, std::format("%s:%s", addr->ss_stage, k + 1).c_str());
                            bson_append_string(o, "1", bson_iterator_key(bson_it));
                            return bson_append_start_object(o, "2");
                        },
                        [&addr](bson *o, const char *k, int r, TICKET__STAGE::Callback *u) {
                            return (r > 0) ? bson_append_bool(o, k, true)
                                : (u ? bson_append_string(o, k, std::format("@%p", u).c_str())
                                     : bson_append_bool(o, k, false));
                        },
                        [&addr](bson *o) {
                            bson_append_finish_object(o);
                            bson_append_finish_array(o); bson_finish(o); return stage_signal(0, NULL, o, 0, addr);
                        }
                    }
                }; BSON__X *B = B__X + (feedback_id != NULL);

                bson_scope b_o; B->begin(&b_o, feedback_id);
                {
                    std::string category_id =
                            (feedback_id ? std::string(__i, feedback_id - __i): __i);

	                ENTER_RWLOCK(&(*__stage)->rwlock);
                    TICKET__STAGE::CategorySet::iterator c_it = (*__stage)->categorys.find(category_id.c_str());
                    if ((c_it == (*__stage)->categorys.end()) ||
                            (c_it->second->timeExpire < utimeNow(NULL)))
                        ;
                    else {
                        TICKET__STAGE::Category *category = c_it->second;

                        bson_iterator sub_it; bson_iterator_subiterator(bson_it, &sub_it);
                        for (utime_t timeNow = utimeNow(NULL); bson_iterator_next(&sub_it) != BSON_EOO;)
                        {
                            const char *k = bson_iterator_key(&sub_it);

                            if (*(k + 0) == 0)
                                bson_append_element(&b_o, "", &sub_it);
                            else {
                                TICKET__STAGE::Callback *u;
                                int r = createTicket(category, timeNow + TICKET_EXPIRE, NULL, &sub_it, &u);

                                B->set(&b_o, k, r, u);
                            }
                        }
                    } LEAVE_RWLOCK(&(*__stage)->rwlock);
                } B->end(&b_o);
            } break;

            default: break;
        }
        else if (__i == callback_id) /* 방송 */
        {
	        typedef std::map<struct sockaddr_stage *, struct sockaddr_stage *, addrStageCompare> T__StageSet;
	        struct X__SUBMIT: public T__StageSet {
		        X__SUBMIT(TICKET__STAGE *stage, const char *k, bson_iterator *bson_it)
				        : T__StageSet()
				        , stage(stage) {
			        stage->Ref();

			        this->k = k;
			        this->t = bson_iterator_type(bson_it);
			        this->v.assign(bson_iterator_value(bson_it), bson_iterator_size(bson_it));
		        }
		        virtual ~X__SUBMIT() { stage->Unref(); }

		        TICKET__STAGE *stage;

		        std::string    k;
		        bson_type      t;
		        std::string    v;

		        static struct sockaddr_stage *clone(struct sockaddr_stage *v)
		        {
			        void *x_v = malloc(sizeof(struct sockaddr_stage));
			        if (x_v != NULL)
				        memcpy(x_v, v, sizeof(struct sockaddr_stage));
			        return (struct sockaddr_stage *)x_v;
		        }
	        };

	        ENTER_RDLOCK(&(*__stage)->rwlock);
            TICKET__STAGE::CategorySet::iterator c_it = (*__stage)->categorys.find(++__i);
            if ((c_it == (*__stage)->categorys.end()) ||
                    (c_it->second->timeExpire < utimeNow(NULL)))
	            LEAVE_RDLOCK(&(*__stage)->rwlock);
            else {
	            X__SUBMIT *entry = new X__SUBMIT((*__stage), __i, bson_it);

                TICKET__STAGE::Category *category = c_it->second;
                for (TICKET__STAGE::ActiveSet::iterator
                        a_it = category->readys.begin(); a_it != category->readys.end(); ++a_it)
                {
                    TICKET__STAGE::Callback *c = a_it->second;

                    X__SUBMIT::iterator x_it = entry->find(&c->addr);
                    if (x_it != entry->end())
                        ;
                    else {
                        struct sockaddr_stage *x_v = X__SUBMIT::clone(&c->addr);
                        if (x_v == NULL)
                            _EXIT("%d: %s", __LINE__, strerror(errno));

                        entry->insert(X__SUBMIT::value_type(x_v, x_v));
                    }
                }

                LEAVE_RDLOCK(&(*__stage)->rwlock);
                if (entry->empty() == false)
                    stage_submit((uintptr_t)(*__stage), [](utime_t x__i, void *x__u) {
                        X__SUBMIT *entry = (X__SUBMIT *)x__u;
                        std::string k = std::format("%s:%s",
                                entry->stage->sign.c_str(), entry->k.c_str()).c_str();

                        for (X__SUBMIT::iterator f_it = entry->begin(); f_it != entry->end(); ++f_it)
                        {
                            struct sockaddr_stage *addr = f_it->first;

                            bson_scope b_o;
                            {
                                std::string sign = (*(addr->ss_stage + 0))
                                        ? std::format("%s:", addr->ss_stage).c_str(): "";

                                bson_append_value(&b_o,
                                        (sign + k).c_str(), entry->t, entry->v.c_str(), entry->v.length());
                            } bson_finish(&b_o);
                            if (stage_signal(0, NULL, &b_o, 0, addr) < 0)
                                _WARN("%d: %s", __LINE__, strerror(errno));
                            free(f_it->second);
                        }
                        delete entry;
                       return 0;
                    }, entry);
                else delete entry;
            }
        }
        else { /* CASE.2 */
            std::string category_id(__i, callback_id - __i);

            char *feedback_id = (char *)strrchr(callback_id, '?');
            static BSON__X B__X[] = {
                {
                    [](bson *o, const char *k) { return 0; },
                    [](bson *o, const char *k, int r, TICKET__STAGE::Callback *u) { return 0; },
                    [](bson *o) { return 0; }
                },
                {
                    [](bson *o, const char *k) { return 0; },
                    [&addr, &bson_it](bson *o, const char *__k, int r, TICKET__STAGE::Callback *u) {
                        std::string k = std::format("%s:%s", addr->ss_stage, __k + 1).c_str();

                        bson_append_start_array(o, k.c_str());
                        {
                            bson_append_string(o, "1", bson_iterator_key(bson_it));

                            if (u == NULL) bson_append_bool(o, "2", (r > 0));
                            else bson_append_string(o, "2", std::format("@%p", u).c_str());

                        } return bson_append_finish_array(o);
                    },
                    [&addr](bson *o) { bson_finish(o); return stage_signal(0, NULL, o, 0, addr); }
                }
            }; BSON__X *B = B__X + (feedback_id != NULL);
            bson_scope b_o;

            ENTER_RWLOCK(&(*__stage)->rwlock);
            TICKET__STAGE::CategorySet::iterator c_it = (*__stage)->categorys.find(category_id.c_str());
            if ((c_it == (*__stage)->categorys.end()) ||
                    (c_it->second->timeExpire < utimeNow(NULL)))
                B->set(&b_o, feedback_id, -1, NULL);
            else {
                std::string uid;

                ++callback_id;
                if (feedback_id == NULL)
                    uid = callback_id;
                else {
                    uid.assign(callback_id, feedback_id - callback_id);
                }

                category_id = TICKET__STAGE::Callback::toString(addr, uid.c_str());
                {
                    TICKET__STAGE::Callback *u;
                    int r = createTicket(c_it->second,
                            utimeNow(NULL) + TICKET_EXPIRE, category_id.c_str(), bson_it, &u);

                    B->set(&b_o, feedback_id, r, u);
                }
            } LEAVE_RWLOCK(&(*__stage)->rwlock);
            B->end(&b_o);
        }
#undef TICKET_EXPIRE
    }
    return 100;
}

/* */
static pthread_once_t TICKET__initOnce = PTHREAD_ONCE_INIT;

static void TICKET__atExit() { }
static void TICKET__atInit()
{
    try {
        ;
    } catch (...) { }
    atexit(TICKET__atExit);
}

};

EXPORT_STAGE(ticket)
{
    pthread_once(&ticket__stage::TICKET__initOnce, ticket__stage::TICKET__atInit); {
        Json::Value &startup = *((Json::Value *)j__config);
        char *__i = (char *)strchr(stage_id, '+');
        {
            ticket__stage::TICKET__STAGE *stage =
                    new ticket__stage::TICKET__STAGE(__i ? __i + 1: NULL, startup);

            stage->Ref();
            (*udata) = stage;
        }
    } (*r__callback) = (stage_callback)ticket__stage::messageCallback;
    return 0;
}
