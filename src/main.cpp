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
#include "typedef.h"
#include <signal.h>
#include <unordered_map>

/* log4cxx */
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/xml/domconfigurator.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/logmanager.h>

#include <getopt.h>
#include "c++/string_format.hpp"
#include "c++/object.hpp"
#include "sockapi.h"
#include "lock.h"
#include "eventio.h"
#include "siocp.h"
#include "single.h"

DECLARE_LOGGER("single");

#include <sys/wait.h>
#include <sys/signalfd.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/* */
struct bufferObjectCompare : public std::binary_function<int16_t *, int16_t *, bool>
{
    bool operator()(const int16_t *l, const int16_t *r) const
    {
        if (*(l + 0) == *(r + 0))
            return memcmp(l + 1, r + 1, *l) < 0;
        return (*(l + 0) < *(r + 0));
    }
};

typedef std::map<int16_t *, int, bufferObjectCompare> bufferObjectSet;



/* */
#include <sys/resource.h>

struct INSPECTOR {

    /* */
    struct USAGE {
        int     us_call;    /* call 수 */
        int64_t us_time;    /* 이전 시간과 차이 */

        /* cluster */
        int cu_tickets;     /* 사용중인 객체 수 */

        /* stage */
        int st_callbacks;   /* callback 대기 수 */
        int st_tasks;       /* task 대기 수 */

        /* thread */
        int tr_ready;       /* 처리 대기 중 */
        int tr_executors;   /* 동시 실행 제한 */
        int tr_submit;      /* 쓰레드 처리 대기 수 */

        /* recvfrom */
        int rf_packets;     /* 외부 처리 데이터 수 */
        struct rusage ru;
    };

#pragma pack(push, 1)
    struct MSG {
        int     stage_id;
        utime_t timeNow;

        struct USAGE u;
    };
#pragma pack(pop)

    INSPECTOR(int id): timeNotify_(0) {
        memset(&msg_, 0, sizeof(msg_)); {
            msg_.stage_id = id;
        }
    }

    int  commit(std::function<void(struct USAGE *u)> callback = NULL, utime_t timeNow = utimeNow(NULL));

    void begin() { ++msg_.u.rf_packets; }
    void end() { }

    /* */
    static int init() {
        int mqid;

        if ((mqid = msgget(IPC_PRIVATE, IPC_CREAT|0600)) < 0)
            switch (errno)
            {
                default: _EXIT("%s: %s", __LINE__, strerror(errno));
                case ENOSYS: _WARN("USAGE: IGNORE");
            }
        else _INFO("USAGE: READY")

        return mqid;
    }

    static void exit(int mqid) {
        if (mqid >= 0)
            msgctl(mqid, IPC_RMID, NULL);
    }

    static void log(struct MSG *msg) {
        struct USAGE &usage = msg->u;

        LOG4CXX_INFO(log4cxx::Logger::getLogger("usage"),
                     std::format(
                             "USAGE %d: CPU time %ld.%06ld, user %ld.%06ld, IDLE %u,%lu, TICKET %u, STAGE %u,%u, SUBMIT %u,%u,%u, IN %u, RSS %ld kb",
                             msg->stage_id,
                             usage.ru.ru_utime.tv_sec, usage.ru.ru_utime.tv_usec,
                             usage.ru.ru_stime.tv_sec, usage.ru.ru_stime.tv_usec,
                             usage.us_call, usage.us_time,
                             usage.cu_tickets,
                             usage.st_callbacks, usage.st_tasks,
                             usage.tr_ready, usage.tr_executors, usage.tr_submit,
                             usage.rf_packets,
                             usage.ru.ru_maxrss
                     ).c_str());
    }
private:
    utime_t    timeNotify_;
    struct MSG msg_;
};


struct STAGE__SYS {
    bufferObjectSet af;
    RWLOCK_T rwlock;

    char **p__argv;
    int    p__length;

    char **l__argv;
    int    l__argc;

    std::string f__suspend;

    /* */
    int    mqid;
    STAGE__SYS(int argc, char **argv, char **envp)
        : p__argv(NULL)
        , p__length(0)
        , l__argc(0) {
        if (!(this->l__argv = (char **)malloc(sizeof(char *) * argc))) _EXIT("%d: %s", __LINE__, strerror(errno));
        {
            int i;

            /*
             * Move the environment so we can reuse the memory.
             * (Code borrowed from sendmail.)
             * WARNING: ugly assumptions on memory layout here;
             *          if this ever causes problems, #undef DO_PS_FIDDLING
             */
            for (i = 0; envp[i] != NULL; i++);

            if (!(environ = (char **)malloc(sizeof(char *) * (i + 1))))
                _EXIT("%d: %s", __LINE__, strerror(errno));
            else {
                char *l__argv = NULL;

                for (i = 0; i < argc; ++i)
                {
                    if ((l__argv == NULL) || ((l__argv + 1) == argv[i]))
                        l__argv = argv[i] + strlen(argv[i]);
                }

                for (i = 0; envp[i] != NULL; ++i)
                {
                    if (!(environ[i] = strdup(envp[i])))
                        _EXIT("%d: %s", __LINE__, strerror(errno));
                    else if ((l__argv + 1) == envp[i])
                        l__argv = envp[i] + strlen(envp[i]);
                }
                environ[i] = NULL;
                if ((p__length = (l__argv - argv[0]) - 1) > 0) p__argv = argv;
            }
        } INIT_RWLOCK(&rwlock);

        if (!(this->l__argv[this->l__argc++] = strdup(argv[0]))) _EXIT("%d: %s", __LINE__, strerror(errno));
        this->mqid = INSPECTOR::init();
    }

    bool suspend() {
        return (access(f__suspend.c_str(), 0) == 0);
    }
    /* */
#pragma pack(push, 1)
    struct IPC_HEAD {
        long     mtype;
        uint16_t op;
    };
#pragma pack(pop)

#if !defined(MSGMAX)
#  define MSGMAX                8192
#endif
    template <typename T> int dispatch(std::function<int (uint16_t op, T *v, ssize_t l)> callback)
    {
        if (this->mqid < 0)
            errno = ENOSYS;
        else {
            static char __buffer[MSGMAX];

            int r = msgrcv(this->mqid, __buffer, sizeof(__buffer) - sizeof(long), 0, IPC_NOWAIT);
            if (r > 0)
            {
                struct IPC_HEAD *msg = (struct IPC_HEAD *)__buffer;
                return callback(msg->op, (T *)(msg + 1), ((r + sizeof(long)) - sizeof(IPC_HEAD)) / sizeof(T));
            }
        } return -1;
    }

    template <typename T> int notify(uint16_t op, std::function<int (T *v)> callback)
    {
        if (this->mqid < 0)
            errno = ENOSYS;
        else {
            static char __buffer[MSGMAX];
            struct IPC_HEAD *msg = (struct IPC_HEAD *)__buffer;

            msg->mtype = getpid();
            msg->op = op;
            int r = callback((T *)(msg + 1));
            return (r >= 0)
                ? msgsnd(this->mqid, __buffer,
                        (r * sizeof(T)) + (sizeof(IPC_HEAD) - sizeof(long)), IPC_NOWAIT): r;
        } return -1;
    }
} *ESYS = NULL;


extern "C" int stage_args(const char ***args)
{
    if (args != NULL)
        (*args) = (const char **)ESYS->l__argv;
    return ESYS->l__argc;
}


extern "C" struct addrinfo *af_addrinfo(int socktype, const char *host_or_ip, const char *port, int flags)
{
    std::string __ip;
    struct addrinfo hints, *entry;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = flags /* AI_PASSIVE */;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socktype /* SOCK_STREAM */;
    if (host_or_ip && (*(host_or_ip + 0) == '['))
    {
        const char *x = strchr(host_or_ip, ']');
        if (x != NULL)
            __ip.assign(host_or_ip + 1, (x - host_or_ip) - 1);
        else {
            errno = EINVAL;
            return NULL;
        }
        host_or_ip = __ip.c_str();
    }

    return (getaddrinfo(host_or_ip, port, &hints, &entry) < 0) ? NULL: entry;
}


/* */
extern "C" int af_socket( struct addrinfo *entry, struct addrinfo **a__i)
{
    int fd = -1;

    if (a__i != NULL) (*a__i) = NULL;
    {
        char     x__buffer[BUFSIZ];
        int16_t *x__addr = (int16_t *)x__buffer;

        struct addrinfo *bi = NULL;
        std::deque<struct addrinfo *> addrs;

        {
            guard::rwlock __rwlock(&ESYS->rwlock);

            for (struct addrinfo *ai = entry; ai != NULL; ai = ai->ai_next)
            {
                *(x__addr + 0) = ai->ai_addrlen;
                memcpy(x__addr + 1, ai->ai_addr, ai->ai_addrlen);

                bufferObjectSet::iterator it = ESYS->af.find(x__addr);
                if (it == ESYS->af.end()) ;
                else if (fd < 0)
                {
                    if (a__i != NULL)
                        (*a__i) = ai;
                    return it->second;
                }

                if ((bi == NULL) || (bi->ai_family < ai->ai_family)) addrs.push_back(bi = ai);
            }

            try {
                if (bi == NULL) { errno = ENOENT; throw ((int)__LINE__); }
                if ((fd = socket(bi->ai_family, bi->ai_socktype, bi->ai_protocol)) < 0)
                    throw ((int)__LINE__);

                try {
                    if (set_sockattr(fd, SOCK_NDELAY) < 0) throw ((int) __LINE__);
                    else if (bind(fd, bi->ai_addr, bi->ai_addrlen) < 0) throw ((int)__LINE__);
                    /* else if ((bi->ai_socktype == SOCK_STREAM) && (listen(fd, 0) < 0)) throw ((int)__LINE__); */

                    for (; addrs.empty() == false; addrs.pop_front())
                    {
                        struct addrinfo *ai = addrs.front();

                        int16_t *addr = (int16_t *) malloc(sizeof(int16_t) + ai->ai_addrlen);
                        if (addr == NULL)
                            throw ((int)__LINE__);
                        else {
                            *(addr + 0) = ai->ai_addrlen;
                            memcpy(addr + 1, ai->ai_addr, ai->ai_addrlen);
                        }
                        ESYS->af.insert(bufferObjectSet::value_type(addr, fd));
                    }
                    if (a__i != NULL) (*a__i) = bi;
                } catch (int __l) { close(fd); throw __l; }
            } catch (int __l) { fd = -__l; }
        }
    }
    return fd;
}


static void af__socket(int socktype, Json::Value &v)
{
    std::string __x;
    const char *host_or_ip = NULL;
    const char *port = NULL;

    if (v.isInt()) __x = std::format("0.0.0.0:%d", v.asInt()).c_str();
    else if (v.isString()) __x = v.asCString();
    {
        char *__p = (char *)strrchr(host_or_ip = __x.c_str(), ':');
        if (__p != NULL)
        {
            *(__p + 0) = 0;
            port = __p + 1;
        }
    }

    _INFO("SERVICES %d.[%s:%s]", socktype, host_or_ip, port)
    {
        struct addrinfo *ai = af_addrinfo(socktype, host_or_ip, port, AI_PASSIVE);
        if (ai != NULL)
        {
            if (af_socket(ai, NULL) < 0)
                _WARN("'%s' - %s", v.asCString(), strerror(errno));
            freeaddrinfo(ai);
        }
    }
}

/* */
extern "C" void stage_setproctitle( const char *format, ...)
{
    if (ESYS->p__argv != NULL)
    {
        va_list args;

        memset(ESYS->p__argv[0], 0, ESYS->p__length);
        va_start( args, format); {
            std::string __f = format;

            std::replace(__f, "${program}", ESYS->l__argv[0]);
            vsnprintf(ESYS->p__argv[0], ESYS->p__length - 1, __f.c_str(), args);
        } va_end( args);
        ESYS->p__argv[1] = NULL;
    }
}


#include <atomic>

static struct THREAD__EXECUTOR {

    static int create(std::function<int (pthread_attr_t *attr)> callback)
    {
        pthread_attr_t attr;
        int r;

        if ((r = pthread_attr_init(&attr)) < 0)
            ;
        else {
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_attr_setschedpolicy(&attr, SCHED_RR);
            while ((r = callback(&attr)) > 0)
                ;
            pthread_attr_destroy(&attr);
        } return r;
    }

    struct CONTEXT: public ObjectRef {
        uintptr_t id;

        utime_t timeClosure; /* 마지막 실행 시간 */
        CONTEXT(uintptr_t i)
                : ObjectRef("THREAD__EXECUTOR::CONTEXT")
                , id(i)
                , timeClosure(0) { }

        virtual CONTEXT *Ref() { ObjectRef::Ref(); return this; }
        bool Used() const { return (this->Count() > 1); }  /* executors에 등록할때 ref_가 증가 되므로 */
    protected:
    	virtual void Dispose() { delete this; }
    }; typedef std::unordered_map<uintptr_t, CONTEXT *> ContextSet;

    struct OBJECT { /* sobject->o */
        CONTEXT *c;
        utime_t  timeStart;

        std::function<int(utime_t)> executor;
    }; typedef std::deque<sobject> READYObjectSet;

    typedef std::deque<pthread_t> ThreadSet;

    struct API: public ObjectRef {
        API(void *(*x__alloc)(uintptr_t, uintptr_t *), uintptr_t u)
            : ObjectRef("THREAD__EXECUTOR::API")
            , f(x__alloc)
            , udata(u) { }
        virtual ~API() { }

        void *operator ()(uintptr_t arg) { return f(arg, &udata); }
        virtual API *Ref() { ObjectRef::Ref(); return this; }
    protected:
    	virtual void Dispose() { delete this; }

        std::function<void *(uintptr_t, uintptr_t *)> f;
        uintptr_t udata;
    }; typedef std::unordered_map<std::string, API *> APISet;

    APISet         apis;

    READYObjectSet readys;      /* 대기중인 객체 */
    ContextSet     executors;   /* 사용중인 객체 */
    LOCK_T         lock;

    THREAD__EXECUTOR(int ncpus)
    {
        INIT_LOCK(&lock);
        if (!(this->scp = scp_attach(std::format(".__%d.%u", getpid(), pthread_self()).c_str(), 0, NULL, NULL)))
            _EXIT("%d: %s", __LINE__, strerror(errno));
        else {
            if (ncpus <= 0) ncpus = sysconf(_SC_NPROCESSORS_ONLN) * 2;

            ENTER_LOCK(&lock);
            if (create([&](pthread_attr_t *attr) {
                pthread_t thread;

                if (pthread_create(&thread, attr,
                        (void *(*)(void *))__worker, this) < 0) return -1;
                return ((--ncpus) > 0) ? 1: 0;
            }) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
        } LEAVE_LOCK(&lock);
    }

    void adjust(utime_t adjustTime = 180 * 1000); /* 3분 이상 사용되지 않는 context는 제거 */
    int  dispatch();

    siocp scp;
private:

    static void __worker(THREAD__EXECUTOR *_this);
} *THREAD_EXECUTOR = NULL;


int THREAD__EXECUTOR::dispatch()
{
    sobject sobj = NULL;

    if (scp_catch(this->scp, &sobj, SOBJ_WEAK) < 0) return -1;
    else if (sobj->o == NULL) scp_release(this->scp, sobj);
    else {
        OBJECT *o = (OBJECT *)sobj->o;
        int r;

        if (o->c == NULL)
        {
            if (o->timeStart > utimeNow(NULL));
            else if ((r = o->executor(0)) > 0) o->timeStart = utimeNow(NULL) + r;
            else {
                delete o;
                return scp_release(this->scp, sobj);
            }

            ENTER_LOCK(&this->lock); goto _gR;
        }

        ENTER_LOCK(&this->lock);
        if ((o->c->timeClosure == UINT64_MAX) || (o->timeStart > utimeNow(NULL)))
_gR:        this->readys.push_back(sobj);
        else {
            utime_t timeClosure = o->c->timeClosure;

            o->c->timeClosure = UINT64_MAX;

            LEAVE_LOCK(&this->lock);
            if ((r = o->executor(timeClosure)) <= 0)
                scp_release(this->scp, sobj);
            else {
                o->timeStart = timeClosure + r;
                ENTER_LOCK(&this->lock); {
                    o->c->timeClosure = timeClosure;
                } goto _gR;
            } ENTER_LOCK(&this->lock);

            o->c->timeClosure = utimeNow(NULL);
            o->c->Unref();

            delete o;
        } LEAVE_LOCK(&this->lock);
    }
    return 0;
}


void THREAD__EXECUTOR::__worker(THREAD__EXECUTOR *_this)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
#define MAX_TIMEWAIT            1000
    for (int r; (r = scp_try(_this->scp, MAX_TIMEWAIT)) >= 0; )
    {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); {
            pthread_testcancel();
        } pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if (r == 0);
        else if (_this->dispatch() < 0) break;
    }
#undef MAX_TIMEWAIT
}

void THREAD__EXECUTOR::adjust(utime_t adjustTime)
{
    utime_t timeNow = utimeNow(NULL);

    ENTER_LOCK(&this->lock);
    if (adjustTime > 0)
        for (ContextSet::iterator it = this->executors.begin(); it != this->executors.end(); )
        {
            CONTEXT *c = it->second;

            if ((c->timeClosure == UINT64_MAX) || c->Used() ||
                    ((timeNow - c->timeClosure) < adjustTime))
                ++it;
            else {
                it = this->executors.erase(it);
                c->Unref();
            }
        }

    for (READYObjectSet::iterator it = this->readys.begin(); it != this->readys.end(); )
    {
        OBJECT *o = ((OBJECT *)(*it)->o);

        if ((o->timeStart > timeNow) ||
                (o->c && (o->c->timeClosure == UINT64_MAX)) ||
                (scp_signal(this->scp, &(*it), 0) < 0))
            ++it;
        else {
            it = this->readys.erase(it);
        }
    } LEAVE_LOCK(&this->lock);
}


extern "C" int stage_cancel(uintptr_t submit_id)
{
    guard::lock __lock(&THREAD_EXECUTOR->lock);

    THREAD__EXECUTOR::ContextSet::iterator it = THREAD_EXECUTOR->executors.find(submit_id);
    if (it == THREAD_EXECUTOR->executors.end())
        return -1;
    else {
        THREAD__EXECUTOR::CONTEXT *c = it->second;

        THREAD_EXECUTOR->executors.erase(it);
        c->Unref();
    }
    return 1;
}

extern void *stage_api(const char *name, uintptr_t arg)
{
    ENTER_LOCK(&THREAD_EXECUTOR->lock); {
        THREAD__EXECUTOR::APISet::iterator a_it = THREAD_EXECUTOR->apis.find(name);
        if (a_it == THREAD_EXECUTOR->apis.end())
            ;
        else {
            THREAD__EXECUTOR::API *api = a_it->second;
            void *r;

            api->Ref();
            LEAVE_LOCK(&THREAD_EXECUTOR->lock); {
                r = (*api)(arg);
            } api->Unref();
            return r;
        }
    } LEAVE_LOCK(&THREAD_EXECUTOR->lock);
    return NULL;
}

extern int stage_setapi(const char *name, void *(*x_alloc)(uintptr_t, uintptr_t *), uintptr_t udata)
{
    ENTER_LOCK(&THREAD_EXECUTOR->lock); {
        THREAD__EXECUTOR::APISet::iterator a_it = THREAD_EXECUTOR->apis.find(name);
        if (a_it != THREAD_EXECUTOR->apis.end())
            a_it->second->Unref();
        else {
            if (x_alloc == NULL);
            else if (x_alloc != NULL)
_gI:        {
                THREAD__EXECUTOR::API *api = new THREAD__EXECUTOR::API(x_alloc, udata);

                a_it = THREAD_EXECUTOR->apis.insert(
                        THREAD__EXECUTOR::APISet::value_type(name, api->Ref())
                ).first;
            } LEAVE_LOCK(&THREAD_EXECUTOR->lock);
            return 1;
        }

        THREAD_EXECUTOR->apis.erase(a_it);
        if (x_alloc != NULL) goto _gI;
    } LEAVE_LOCK(&THREAD_EXECUTOR->lock);
    return 0;
}

extern "C" int stage_dispatch(int timeout)
{
    int r;

    if ((r = scp_try(THREAD_EXECUTOR->scp, timeout)) <= 0)
        return r;
    return THREAD_EXECUTOR->dispatch();
}

extern "C" int stage_submit(uintptr_t submit_id, int (*execute)(utime_t, void */* udata */), void *arg)
{
    sobject sobj;

    if (sobj_alloc(&sobj) < 0)
        return -1;
    else {
        THREAD__EXECUTOR::OBJECT *o = new THREAD__EXECUTOR::OBJECT();

        sobj->o = o;
        sobj->priority = 0;

        o->executor = std::bind(execute, std::placeholders::_1, arg);
        ENTER_LOCK(&THREAD_EXECUTOR->lock); {
            bool ready = false;

            if (submit_id == 0)
                o->c = NULL;
            else {
                THREAD__EXECUTOR::ContextSet::iterator it = THREAD_EXECUTOR->executors.find(submit_id);
                if (it == THREAD_EXECUTOR->executors.end())
                    it = THREAD_EXECUTOR->executors.insert(
                            THREAD__EXECUTOR::ContextSet::value_type(
                            		submit_id, (new THREAD__EXECUTOR::CONTEXT(submit_id))->Ref()
                            	)
                    ).first;

                (o->c = it->second)->Ref();
                ready = (o->c->timeClosure == UINT64_MAX);
            }

            if ((ready + (THREAD_EXECUTOR->readys.empty() == false)) ||
                    (scp_signal(THREAD_EXECUTOR->scp, &sobj, 0) < 0))
                THREAD_EXECUTOR->readys.push_back(sobj);
        } LEAVE_LOCK(&THREAD_EXECUTOR->lock);
    }

#define EDGE_EXECUTOR               0.8
#define WAIT_TIMEOUT                10
#define BREAK_TIMEOUT               400
    if (scp_isover(THREAD_EXECUTOR->scp, EDGE_EXECUTOR))
    {
        utime_t timeStart = utimeNow(NULL);

        do {
            int r;

            if ((r = scp_try(THREAD_EXECUTOR->scp, WAIT_TIMEOUT)) < 0) break;
            else if (r == 0); /* TIMEOUT */
            else if (THREAD_EXECUTOR->dispatch() < 0) break;
        } while (scp_isover(THREAD_EXECUTOR->scp, EDGE_EXECUTOR) &&
                    ((utimeNow(NULL) - timeStart) < BREAK_TIMEOUT));
    }
#undef BREAK_TIMEOUT
#undef WAIT_TIMEOUT
#undef EDGE_EXECUTOR
    return 0;
}

/* */
#include "pqueue.h"

struct STAGE__CTRL {
    typedef std::unordered_map<sa_family_t, int> IO_fdSet;

    struct TASK: public ObjectRef {
        utime_t timeUpdate;

        void *udata;
        stage_callback callback;

        TASK(utime_t t, stage_callback c, void *u)
                : ObjectRef("STAGE__CTRL::TASK")
                , timeUpdate(t)
                , udata(u)
                , callback(c) { }
        virtual ~TASK() { callback(UINT64_MAX, NULL, NULL, &this->udata); }

        int operator ()(utime_t timeBreak, bson_iterator *bson_in, struct sockaddr_stage *addr) {
            return callback(timeBreak, bson_in, addr, &this->udata);
        }

        virtual TASK *Ref() { ObjectRef::Ref(); return this; }
    protected:
    	virtual void Dispose() { delete this; }
    }; typedef std::deque<TASK *> TASK_CallbackSet;

    typedef std::unordered_map<std::string, int> MODULE_addSet;

    MODULE_addSet modules;
    TASK_CallbackSet callbacks;
    pq_pState tasks;

    IO_fdSet  fd;
    RWLOCK_T  rwlock;

    int       timeNotify;
    const int stage_id;

    /* */
    struct sockaddr_stage c__addr, l__addr, r__addr;
#define MAX_TASK_QUEUE          2048
#define KEEP_TIMEOUT            300
    STAGE__CTRL(int id, struct sockaddr_stage *a)
            : tasks(pq_init(MAX_TASK_QUEUE, __lessThen, this))
            , timeNotify(KEEP_TIMEOUT)
            , stage_id(id) {
        memset(&l__addr, 0, sizeof(l__addr));   /* local */
        memset(&r__addr, 0, sizeof(r__addr));   /* remote */

        if (a == NULL) memset(&c__addr, 0, sizeof(c__addr));   /* manager */
        else memcpy(&c__addr, a, sizeof(c__addr));

        INIT_RWLOCK(&rwlock);
    }
#undef KEEP_TIMEOUT

    virtual ~STAGE__CTRL() {
        pq_exit(tasks, [](void *__x, void *__u) {
            ((TASK *)__x)->Unref();
        }, this);

        for (;this->callbacks.empty() == false; this->callbacks.pop_front())
            this->callbacks.front()->Unref();
        DESTROY_RWLOCK(&rwlock);
    }
#undef MAX_TASK_QUEUE

    int commit(std::function<int (struct sockaddr_stage *addr, bson *)> callback) {
        if (c__addr.ss_length <= 0)
            return -1;
        else {
            int r;
            {
                bson_scope b_o;

                int l = b_o.cur - b_o.data;
                if ((r = callback(&this->c__addr, &b_o)) < 0);
                else if ((b_o.cur - b_o.data) > l)
                {
                    bson_finish(&b_o);
                    r = stage_signal(-1, NULL, &b_o, 0, &this->c__addr);
                }
            } return r;
        }
    }

private:
    static int __lessThen(const void *l, const void *r, void *udata) {
        int64_t delta = ((TASK *)l)->timeUpdate - ((TASK *)r)->timeUpdate;
        return (delta == 0) ? (((intptr_t)l) < ((intptr_t)r)): (delta < 0);
    }
} *ECTRL = NULL;

static int bson_append_modules(bson *b_o, const char *k, STAGE__CTRL *_this)
{
    ENTER_RDLOCK(&_this->rwlock);
    if (_this->modules.empty())
        bson_append_int(b_o, k, getpid());
    else {
        bson_append_start_array(b_o, k);
        for (STAGE__CTRL::MODULE_addSet::iterator
                     m_it = _this->modules.begin();m_it != _this->modules.end(); ++m_it)
            bson_append_string(b_o, std::format("%d", m_it->second).c_str(), m_it->first.c_str());
        bson_append_finish_array(b_o);
    } LEAVE_RDLOCK(&_this->rwlock);
    return 0;
}

/* */
int INSPECTOR::commit(std::function<void(struct INSPECTOR::USAGE *u)> callback, utime_t timeNow)
{
    ++msg_.u.us_call;
#define NOTIFY_TIMEOUT                  1000
    if ((msg_.u.us_time = (timeNow - timeNotify_)) < NOTIFY_TIMEOUT)
        return 0;
    else {
        msg_.timeNow = timeNow;
        if (ESYS->notify<MSG>(0, [&](MSG *v) {

            memcpy(v, &msg_, sizeof(MSG));
            ENTER_LOCK(&THREAD_EXECUTOR->lock); {
                v->u.tr_ready = THREAD_EXECUTOR->readys.size();
                v->u.tr_executors = THREAD_EXECUTOR->executors.size();
            } LEAVE_LOCK(&THREAD_EXECUTOR->lock);
            v->u.tr_submit = scp_look(THREAD_EXECUTOR->scp);

            if (callback != NULL) callback(&v->u);
            return (getrusage(RUSAGE_SELF, &v->u.ru) < 0) ? -1: 1;
        }) < 0) switch (errno)
        {
            default: return -1;
            case ENOSYS: INSPECTOR::log(&msg_);
        }
        msg_.u.us_call = 0;
    } timeNotify_ = timeNow;
    return 1;
#undef NOTIFY_TIMEOUT
}

/* */
extern "C" int stage_id() { return ECTRL->stage_id; }

#include <list>
#include <net/if.h>

static int bson_iterator_sockaddr(bson_iterator *bson_a, struct sockaddr_stage *addr)
{
    /* [ "ipv4_ip", port ] */
    while (bson_iterator_next(bson_a))
    {
        long x__i = strtol(bson_iterator_key(bson_a), NULL, 10);
        if ((x__i != LONG_MIN) && (x__i != LONG_MAX))
            switch ((int)bson_iterator_type(bson_a))
            {
                case BSON_BINDATA: switch (bson_iterator_bin_type(bson_a))
                {
                    case BSON_BIN_BINARY: addr->ss_length = bson_iterator_bin_len(bson_a);
                    {
                        memcpy(addr, bson_iterator_bin_data(bson_a), addr->ss_length);
                    } return (addr->ss_family != AF_UNSPEC) ? 0: -1;

                    default: break;
                } break;

                case BSON_STRING:
                {
                    const char *v = bson_iterator_string(bson_a);

                    if (addr->ss_family == AF_UNSPEC)
                        switch (addr->ss_family =
                                (*(v + 0) == 0) ? AF_UNIX: ((strchr(v, ':') != NULL) ? AF_INET6: AF_INET))
                        {
                            case AF_UNIX: addr->ss_length = sizeof(struct sockaddr_un); break;
                            case AF_INET: addr->ss_length = sizeof(struct sockaddr_in); break;
                            default: addr->ss_length = sizeof(struct sockaddr_in6);
                            {
                                struct sockaddr_in6 *i6_addr = (struct sockaddr_in6 *)addr;
                                struct if_nameindex *if_ni = if_nameindex();

                                if (if_ni != NULL)
                                {
                                    for (struct if_nameindex *i = if_ni; i->if_name != NULL; ++i)
                                        if (i->if_index > 0)
                                        {
                                            i6_addr->sin6_scope_id = i->if_index;
                                            break;
                                        }
                                    if_freenameindex(if_ni);
                                }
                                i6_addr->sin6_flowinfo = 0;
                            } break;
                        }

                    if (*(v + 0) != 0) switch (addr->ss_family)
                    {
                        case AF_UNIX: strcpy(((struct sockaddr_un *)addr)->sun_path, v); break;
                        default:
                        {
                            void *i__r = (addr->ss_family == AF_INET)
                                         ? (void *)&((struct sockaddr_in *)addr)->sin_addr
                                         : (void *)&((struct sockaddr_in6 *)addr)->sin6_addr;
                            if (inet_pton(addr->ss_family, v, i__r) < 0) return -1;
                        } break;
                    }
                } break;

                case BSON_INT:
                case BSON_LONG: if (addr->ss_family != AF_UNSPEC)
                {
                    in_port_t i_port = htons(bson_iterator_int(bson_a));

                    switch (addr->ss_family)
                    {
                        case AF_INET: ((struct sockaddr_in *)addr)->sin_port = i_port; break;
                        default     : ((struct sockaddr_in6 *)addr)->sin6_port = i_port; break;
                    }
                } break;

                default: break;
            }
    }
    return (addr->ss_family != AF_UNSPEC) ? 0: -1;
};


struct X__Adapter {
    struct Buffer: public std::basic_string<uint8_t> {
        struct sockaddr_stage addr;

        Buffer(size_t length = 0)
            : std::basic_string<uint8_t>() { this->resize(length); }
        virtual ~Buffer() { }
    }; typedef std::list<Buffer *> BufferSet;

    eadapter        eadp;

    BufferSet       buffers;
    pthread_cond_t  wait;
    pthread_mutex_t lock;

    X__Adapter()
        : eadp(NULL)
        , wait(PTHREAD_COND_INITIALIZER)
        , lock(PTHREAD_MUTEX_INITIALIZER) { }

    virtual ~X__Adapter() {
        {
            for (;buffers.empty() == false; buffers.pop_front())
                delete buffers.front();
        } if (this->eadp) eadapter_destroy(this->eadp);
    }

    static void worker(void *x__u);
private:
    static void __cleanup(void *x__u) { delete ((X__Adapter *)x__u); }
};

void X__Adapter::worker(void *x__u)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    pthread_cleanup_push(X__Adapter::__cleanup, x__u);
    {
        X__Adapter *_this = (X__Adapter *)x__u;
        struct eadapter_event r_e;
#define EPOLL_TIMEOUT               100
_gW:    {
            size_t nwait = 0;

            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); {
                pthread_testcancel();
            } pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

_gB:        switch (eadapter_wait(_this->eadp, &r_e, &nwait, EPOLL_TIMEOUT))
            {
                case -1: _EXIT("%d: CANCEL", __LINE__);

                default      : break;
                case EPOLLRAW:
                {
                    struct Buffer *r_in = new Buffer(nwait);

                    r_in->addr.ss_length = sizeof(struct sockaddr_storage);
                    if (::recvfrom(efile_fileno(r_e.f), (void *)r_in->data(), r_in->size(),
                                   MSG_DONTWAIT, (struct sockaddr *)&r_in->addr, &r_in->addr.ss_length) < 0)
                        _WARN("%d: %s", __LINE__, strerror(errno))
                    else {
                        pthread_mutex_lock(&_this->lock); {
                            _this->buffers.push_back(r_in);
                            pthread_cond_signal(&_this->wait);
                        } pthread_mutex_unlock(&_this->lock);
                        goto _gB;
                    }
                    delete r_in;
                } break;
                case EPOLLHUP:
                case EPOLLERR:
                {
                    int fd = efile_fileno(r_e.f);
                    _WARN("%d: %d - '%s'", __LINE__, fd, strerror(get_sockerr(fd)))
                } break;
            }
        } goto _gW;
#undef EPOLL_TIMEOUT
    }
    pthread_cleanup_pop(0);
}

static void PROCESS_mainStage(struct STAGE__CTRL *_this)
{
#define MAX_POLLFD              16
    /* */
    X__Adapter *x_eadp = new X__Adapter();

    if (!(x_eadp->eadp = eadapter_create(MAX_POLLFD, NULL))) _EXIT("%d: %s", __LINE__, strerror(errno));
    {
        auto efile_link = [](eadapter eadp, int fd)
        {
            if (fd <= 0)
                return -1;
            else {
                efile f;

                if (!(f = efile_open(fd, EPOLLIN|EPOLLERR, NULL))) _EXIT("%d: %s", __LINE__, strerror(errno));
                else if (efile_setadapter(f, eadp) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
            } return 0;
        };

        efile_link(x_eadp->eadp, _this->fd[AF_UNIX]);
        if (_this->fd.find(AF_INET) != _this->fd.end())
            efile_link(x_eadp->eadp, _this->fd[AF_INET]);
    } if (THREAD__EXECUTOR::create([&x_eadp](pthread_attr_t *attr) {
        pthread_t thread;

        return pthread_create(&thread, attr, (void *(*)(void *))X__Adapter::worker, x_eadp);
    }) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));

    auto stage_closure = [](STAGE__CTRL::TASK_CallbackSet &clone,
            utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr)
    {
        for (STAGE__CTRL::TASK_CallbackSet::iterator m_it = clone.begin(); m_it != clone.end(); )
        {
            bson_iterator __it = *bson_it;

            if ((*m_it)->operator()(timeBreak, &__it, addr) >= 0)
                ++m_it;
            else {
                (*m_it)->Unref();
                m_it = clone.erase(m_it);
            }
        }
    };

#define DEF_TIMEOUT             100
    stage_addtask(utimeNow(NULL) - DEF_TIMEOUT, [](utime_t timeBreak, bson_iterator *x__it, struct sockaddr_stage *addr, void **udata) {
        STAGE__CTRL *_this = *((STAGE__CTRL **)udata);

        if (ESYS->suspend())
            _TRACE("SUSPEND %d: STAGE [%d msec]", getpid(), _this->timeNotify)
        else {
            _TRACE("REGISTER %d: STAGE [%d msec]", getpid(), _this->timeNotify);
            _this->commit([&](struct sockaddr_stage *x__addr, bson *b_o) {
                return bson_append_modules(b_o, "", _this);
            });
        }
        return _this->timeNotify;
    }, _this);

    INSPECTOR inspector(_this->stage_id);
    struct X__CurrentStage: public X__Adapter::BufferSet {
        utime_t timeBreak;      /* 메시지 처리 제한 시간 */
        utime_t timeSuspend;    /* 과부하로 인한 사용 제한 시작시간 */

        utime_t timeStart;       /* 모니터링 시작시간 */
        size_t  lastUpdate;     /* 마지막 갱신된 메시지 갯수 */
        X__CurrentStage()
            : X__Adapter::BufferSet()
            , timeBreak(0)
            , timeSuspend(0)
            , timeStart(0)
            , lastUpdate(0) { }
    } x__stage;

_gW:THREAD_EXECUTOR->adjust();
    {
        int timeOut = DEF_TIMEOUT;
        utime_t timeNow = utimeNow(NULL);

        if ((x__stage.timeSuspend == 0) ||
                ((int)(timeNow - x__stage.timeSuspend) <= _this->timeNotify))
        {
            std::list<STAGE__CTRL::TASK *> ready;

_gT:        ENTER_RWLOCK(&_this->rwlock);
            {
                STAGE__CTRL::TASK *task = (STAGE__CTRL::TASK *)pq_top(_this->tasks);

                if (task == NULL) /* timeOut = DEF_TIMEOUT */;
                else if (task->timeUpdate > timeNow)
                    timeOut = std::min((int)(task->timeUpdate - timeNow), DEF_TIMEOUT);
                else {
                    pq_pop(_this->tasks); LEAVE_RWLOCK(&_this->rwlock);
                    {
                        int r = (*task)(UINT64_MAX, NULL, &_this->c__addr);

                        timeNow = utimeNow(NULL);
                        if (r < 0)
                            task->Unref();
                        else {
#define LOW_LATENCY             10
                            task->timeUpdate = timeNow + std::max(r, LOW_LATENCY);
                            ready.push_back(task);
#undef LOW_LATENCY
                        }
                    } goto _gT;
                }

                x__stage.timeBreak = timeNow + timeOut;
                for (;ready.empty() == false; ready.pop_front())
                    if ((task = (STAGE__CTRL::TASK *)pq_push(_this->tasks, ready.front())) != NULL)
                    {
                        _WARN("TASK: OVERFLOW");
                        task->Unref();
                    }
            } LEAVE_RWLOCK(&_this->rwlock);
        }
        else x__stage.timeBreak = timeNow + timeOut;
#undef BREAK_TIMEOUT

        inspector.commit([&](struct INSPECTOR::USAGE *u) {
            u->st_callbacks = _this->callbacks.size();
            u->st_tasks = pq_size(_this->tasks);
        });

        if (x__stage.empty())
        {
            pthread_mutex_lock(&x_eadp->lock); {
                struct timespec ts; utimeSpec(NULL, timeOut, &ts);

                for (int r; x_eadp->buffers.empty(); )
                    switch (r = pthread_cond_timedwait(&x_eadp->wait, &x_eadp->lock, &ts))
                    {
                        default: _EXIT("%d: %s", __LINE__, strerror(r));
                        case ETIMEDOUT: pthread_mutex_unlock(&x_eadp->lock);
                        {
                            if (x__stage.timeSuspend == 0);
                            else if (x__stage.lastUpdate > 1)
                                x__stage.lastUpdate = (x__stage.lastUpdate / 2);
                            else {
                                x__stage.timeSuspend = 0;
                                _WARN("%d: Restart processing. - %d", __LINE__, getpid());
                            }
                        } goto _gW;
                        case 0: break;
                    }

                x__stage.swap(x_eadp->buffers);
            } pthread_mutex_unlock(&x_eadp->lock);

            {
                utime_t timeNow = utimeNow(NULL);
                int delta = x__stage.size() - x__stage.lastUpdate;

                if ((x__stage.timeStart == 0) || (delta < 0)) x__stage.timeStart = timeNow;
                else if ((delta > 0) && /* 지속적으로 메시지가 증가하고 있음. */
                            ((int)(timeNow - x__stage.timeStart) >= _this->timeNotify))
                {
                    if (x__stage.timeSuspend == 0)
                        x__stage.timeSuspend = timeNow;
                    _WARN("%d: The processing delay is expected. [+%d 's] - %d", __LINE__, delta, getpid());
                }
            } x__stage.lastUpdate = x__stage.size();
        }
#undef DEF_TIMEOUT
    }

    /* */
    {
        STAGE__CTRL::TASK_CallbackSet clone;

        ENTER_RWLOCK(&_this->rwlock); {
            _this->callbacks.swap(clone);
        } LEAVE_RWLOCK(&_this->rwlock);

_gB:    inspector.begin(); {
            struct X__Adapter::Buffer *r_buffer = x__stage.front();
            struct sockaddr_stage *r__addr = &r_buffer->addr;
            struct sockaddr_stage x__addr;

            std::string r__stage;
            utime_t     r__timeBreak = UINT64_MAX;

#if 0
            fprintf(stderr, " STAGE_IN.%d: - %d\n", getpid(), r_buffer->addr.ss_family); bson_print_raw((char *)r_buffer->data(), 0);
#endif
            bson_iterator bson_v; memset(&bson_v, 0, sizeof(bson_v));
            {
                bson_iterator bson_r; memset(&bson_r, 0, sizeof(bson_r));
                bson_iterator bson_it; bson_iterator_from_buffer(&bson_it, (char *)r_buffer->data());
                while (bson_iterator_next(&bson_it) != BSON_EOO)
                {
                    const char *k = bson_iterator_key(&bson_it);

                    if (strcmp(k, "r") == 0) /* 반환 주소 */
                        switch ((int)bson_iterator_type(&bson_it))
                        {
                            case BSON_OBJECT: r__addr = (struct sockaddr_stage *)-1;
                            {
                                bson_iterator bson_r; bson_iterator_subiterator(&bson_it, &bson_r);
                                while (bson_iterator_next(&bson_r))
                                    switch (bson_iterator_type(&bson_r))
                                    {
                                        case BSON_ARRAY:
                                        {
                                            bson_iterator bson_a; bson_iterator_subiterator(&bson_r, &bson_a);
                                            struct sockaddr_stage u__addr;

                                            u__addr.ss_family = AF_UNSPEC;
                                            if (bson_iterator_sockaddr(&bson_a, &u__addr) < 0);
                                            else if ((r__addr == (struct sockaddr_stage *)-1) ||
                                                        (u__addr.ss_family == r_buffer->addr.ss_family))
                                            {
                                                (r__addr = &x__addr)->ss_length = u__addr.ss_length;
                                                memcpy(r__addr, &u__addr, r__addr->ss_length);
                                            }
                                        } break;

                                        case BSON_BINDATA:
                                        {
                                            struct sockaddr *v = (struct sockaddr *)bson_iterator_bin_data(&bson_r);

                                            if ((r__addr == (struct sockaddr_stage *)-1) ||
                                                    (v->sa_family == r_buffer->addr.ss_family))
                                            {
                                                (r__addr = &x__addr)->ss_length = bson_iterator_bin_len(&bson_r);
                                                memcpy(r__addr, v, r__addr->ss_length);
                                            }
                                        } break;

                                        case BSON_STRING: r__stage = bson_iterator_string(&bson_r);
                                        default: break;
                                    }
                            } break;

                            case BSON_ARRAY:
                            {
                                bson_iterator bson_a; bson_iterator_subiterator(&bson_it, &bson_a);

                                x__addr.ss_family = AF_UNSPEC;
                                if (bson_iterator_sockaddr(&bson_a, &x__addr) >= 0) r__addr = &x__addr;
                            } break;

                            case BSON_BINDATA:
                            {
                                struct sockaddr *v = (struct sockaddr *)bson_iterator_bin_data(&bson_it);

                                if (v->sa_family != r_buffer->addr.ss_family)
                                    r__addr = (struct sockaddr_stage *)-1;
                                else {
                                    (r__addr = &x__addr)->ss_length = bson_iterator_bin_len(&bson_it);
                                    memcpy(r__addr, v, r__addr->ss_length);
                                }
                            } break;

                            case BSON_STRING: r__stage = bson_iterator_string(&bson_r); break;
                            case BSON_NULL: r__addr = (struct sockaddr_stage *)-1;
                        }
                    else if (strcmp(k, "v") == 0) bson_iterator_subiterator(&bson_it, &bson_v); /* 데이터 */
                    else if (strcmp(k, "$") == 0) bson_iterator_subiterator(&bson_it, &bson_r); /* 처리 결과 */
                    else if (strcmp(k, ".") == 0) r__timeBreak = bson_iterator_long(&bson_it);
                }

                if (r__addr && (r__addr != (struct sockaddr_stage *)-1))
                {
                    memcpy(r__addr->ss_stage, r__stage.data(), std::min((int)r__stage.length(), MAX_STAGE_LEN));
                    if (MAX_STAGE_LEN > r__stage.length())
                        memset(r__addr->ss_stage + r__stage.length(), 0, MAX_STAGE_LEN - r__stage.length());
                }

                if (bson_r.cur != NULL) while (bson_iterator_next(&bson_r) != BSON_EOO)
                {
                    const char *k = bson_iterator_key(&bson_r);

                    if (*(k + 0) == 0)
                        switch ((int)bson_iterator_type(&bson_r))
                        {
                            case BSON_INT: _INFO("%d: REGISTER %d", getpid(), bson_iterator_int(&bson_r));
                            {
                                ;
                            } break;
                            case BSON_STRING: _INFO("%d: ALLOW '%s'", getpid(), bson_iterator_string(&bson_r));
                            case BSON_OBJECT: break;
                        }
                    else stage_closure(clone, r__timeBreak, &bson_r, r__addr);
                }
            }

            if (bson_v.cur != NULL) while (bson_iterator_next(&bson_v) != BSON_EOO)
            {
                const char *k = bson_iterator_key(&bson_v);

                /* lua:__signal()에서
                 *   bson_append_string(&b_i, "", stage->sign.c_str());
                 * 보낸 정보를 얻는다.
                 */
                if (*(k + 0) != 0)
                    stage_closure(clone, r__timeBreak, &bson_v, r__addr);
            }

            delete r_buffer;
        } inspector.end();

        x__stage.pop_front();
        if (x__stage.empty() || clone.empty() || (utimeNow(NULL) >= x__stage.timeBreak))
            ;
        else {
            inspector.commit([&](struct INSPECTOR::USAGE *u) {
                u->st_callbacks = _this->callbacks.size();
                u->st_tasks = pq_size(_this->tasks);
            });
            goto _gB;
        }

        ENTER_RWLOCK(&_this->rwlock); {
            _this->callbacks.insert(_this->callbacks.begin(), clone.begin(), clone.end());
        } LEAVE_RWLOCK(&_this->rwlock);
    } goto _gW;
}



extern "C" void stage_addtask(utime_t timeStart, stage_callback callback, void *udata)
{
    STAGE__CTRL::TASK *task = new STAGE__CTRL::TASK(timeStart, callback, udata);

    ENTER_RWLOCK(&ECTRL->rwlock); {
        if (timeStart == 0)
            ECTRL->callbacks.push_back(task->Ref());
        else {
            if (timeStart == (utime_t)-1)
            {
                ECTRL->callbacks.push_back(task->Ref());
                task->timeUpdate = utimeNow(NULL);
            }
            pq_push(ECTRL->tasks, task->Ref());
        }
    } LEAVE_RWLOCK(&ECTRL->rwlock);
}

extern "C" int stage_call(bson_iterator *i)
{
    STAGE__CTRL::TASK_CallbackSet clone;
    int r = 0;

    ENTER_RWLOCK(&ECTRL->rwlock); {
        clone.insert(clone.end(), ECTRL->callbacks.begin(), ECTRL->callbacks.end());
    } LEAVE_RWLOCK(&ECTRL->rwlock);

    while (bson_iterator_next(i) != BSON_EOO)
    {
        const char *k = bson_iterator_key(i);

        if (*(k + 0) != 0)
        /* {
            bson_append_start_object(o, k); */
            for (STAGE__CTRL::TASK_CallbackSet::iterator m_it = clone.begin(); m_it != clone.end(); ++r)
            {
                bson_iterator __it = *i;

                if ((*m_it)->operator()(UINT64_MAX, &__it, &ECTRL->l__addr) >= 0)
                    ++m_it;
                else {
                    (*m_it)->Unref();
                    m_it = clone.erase(m_it);
                }
            }
            /* bson_append_finish_object(o);
        } */
    }
    return r;
}

static void bson_append_sockaddr(bson *b_o, const char *k, struct sockaddr_stage *addr)
{
    if (addr->ss_length <= 0) return;

    bson_append_start_array(b_o, k);
    switch (addr->ss_family)
    {
        case AF_UNIX: bson_append_string(b_o, "0", "");
        {
            bson_append_string(b_o, "1", ((struct sockaddr_un *)addr)->sun_path);
        } break;

        default: bson_append_binary(b_o, "0", BSON_BIN_BINARY, (const char *)addr, addr->ss_length);
        {
            void *s__addr = NULL;
            in_port_t s__port = 0;

            switch (addr->ss_family)
            {
                case AF_INET:
                {
                    struct sockaddr_in *i_addr = (struct sockaddr_in *)addr;

                    s__addr = &(i_addr->sin_addr);
                    s__port = ntohs(i_addr->sin_port);
                } goto _gI;

                case AF_INET6:
                {
                    struct sockaddr_in6 *i_addr = (struct sockaddr_in6 *)addr;

                    s__addr = &(i_addr->sin6_addr);
                    s__port = ntohs(i_addr->sin6_port);
                }

_gI:            {
                    char x__ip[INET6_ADDRSTRLEN + 1];

                    bson_append_string(b_o, "1",
                                       inet_ntop(addr->ss_family, s__addr, x__ip, INET6_ADDRSTRLEN));
                } bson_append_int(b_o, "2", s__port);
            }
        }
    } bson_append_finish_array(b_o);
}

extern "C" int stage_signal(int feedback, const char *stage_id, bson *o, utime_t timeBreak, struct sockaddr_stage *addr)
{
    auto __sendto = [](int fd, bson *o, struct sockaddr_stage *addr)
    {
        int r = -1;

#define SENDTO_MAXSIZE          65000
#if 0
        switch (addr->ss_family)
        {
            case AF_UNIX:
            {
                struct sockaddr_un *u = (struct sockaddr_un *)addr;
                fprintf(stderr, " * SIGNAL.%d: [%s]", getpid(), u->sun_path);
            } break;
            case AF_INET:
            {
                struct sockaddr_in *i = (struct sockaddr_in *)addr;
                fprintf(stderr, " * SIGNAL.%d: [%s:%d]", getpid(), inet_ntoa(i->sin_addr), ntohs(i->sin_port));
            } break;
            default: fprintf(stderr, " * SIGNAL.%d: ", getpid());
        }
        bson_print(o);
#endif
        if (bson_size(o) < SENDTO_MAXSIZE)
            while ((r = ::sendto(fd, (const void *)bson_data(o), bson_size(o),
                    MSG_NOSIGNAL, (struct sockaddr *)addr, addr->ss_length)) < 0)
                switch (errno)
                {
                    case EINTR:
                    case EAGAIN: break;
                    default: /* _WARN("%d: %s", __LINE__, strerror(errno)); */return r;
                }
        else errno = EMSGSIZE;
#undef SENDTO_MAXSIZE
        return r;
    };

    if (addr == ((struct sockaddr_stage *)-1))
        return -1;
    else {
        if (addr == NULL) addr = &ECTRL->c__addr;
        if (addr->ss_length == 0)
            return -1;
    }

    int fd;
    {
        guard::rwlock __rwlock(&ECTRL->rwlock);

        STAGE__CTRL::IO_fdSet::iterator s_it = ECTRL->fd.find(addr->ss_family);
        if (s_it != ECTRL->fd.end())
            fd = s_it->second;
        else {
            if ((fd = socket(addr->ss_family, SOCK_DGRAM, 0)) < 0)
                return -1;
            else {
                int f;

                if (((f = fcntl(fd, F_GETFL, 0)) < 0) ||
                        (fcntl(fd, F_SETFL, f | O_NONBLOCK) < 0))
                {
                    close(fd);
                    return -1;
                }
            }
            ECTRL->fd.insert(STAGE__CTRL::IO_fdSet::value_type(addr->ss_family, fd));
        }
    }

    if (feedback < 0)
        return __sendto(fd, o, addr);
    else {
        struct BSON__X {
            std::function<int (bson *, const char *k)> begin;
            std::function<int (bson *)> end;
        };

        static BSON__X B__X[] = {
                { [](bson *o, const char *k) { return 0; }, [](bson *o) { return 0; } },
                {
                    [](bson *o, const char *k) {
                        if ((k != NULL) && (*(k + 0) != ':'))
                        {
                            const char *p = strchr(k, ':');
                            return bson_append_start_object(o, p ? std::string(k, p - k).c_str(): k);
                        }
                        return bson_append_start_object(o, "=");
                    },
                    [](bson *o) { return bson_append_finish_object(o); }
                },
        };
        BSON__X *B = B__X + ((addr == &ECTRL->c__addr) || (stage_id != NULL));

        bson_scope b_o; B->begin(&b_o, stage_id); {
            if (feedback & 0x80) bson_append_null(&b_o, "r");
            else if ((feedback & 0x7F) != 0) /* 결과 반환이면 리턴주소를 설정하지 않는다. */
            {
                bson_append_start_object(&b_o, "r"); {
                    if (stage_id != NULL) /* 메시지를 보내는 stage를 설정 */
                    {
                        const char *p = strchr(stage_id, ':');
                        if (p != NULL)
                            bson_append_string(&b_o, "s", p + 1);
                    }

                    bson_append_sockaddr(&b_o, "u", &ECTRL->l__addr); /* AF_UNIX */
                    if (ECTRL->r__addr.ss_length > 0)
                        bson_append_sockaddr(&b_o, "i", &ECTRL->r__addr); /* AF_INET */
                } bson_append_finish_object(&b_o);
#undef bson_append_sockaddr
                if (timeBreak) bson_append_long(&b_o, ".", timeBreak);
            } bson_append_bson(&b_o, ((feedback & 0x7F) == 0) ? "$": "v", o);
        } B->end(&b_o);
        bson_finish(&b_o);
        return __sendto(fd, &b_o, addr);
    }
}

/* ]]----------------------------- STAGE__MAIN */
#include <ifaddrs.h>

struct addrStageCompare : public std::binary_function<struct sockaddr_stage *, struct sockaddr_stage *, bool>
{
    bool operator()(const struct sockaddr_stage *l, const struct sockaddr_stage *r) const
    {
        if (l->ss_length == r->ss_length)
            return memcmp(l, r, l->ss_length) < 0;
        return (l->ss_length < r->ss_length);
    }
};

typedef std::deque<struct addrinfo *> CLUSTER_entrySet;

static int PROCESS_mainCluster(Json::Value &JsonConfig, struct sockaddr_stage *f__addr)
{
    typedef std::unordered_map<std::string, utime_t> UID_timeSet;

    struct CLUSTER: public ObjectRef
    		      , public UID_timeSet {
        utime_t timeStart;

        struct sockaddr_stage addr;
        std::atomic_bool      enable;

        std::atomic<std::uint64_t> timeUpdate;
        CLUSTER(struct sockaddr_stage *addr)
                : ObjectRef("CLUSTER")
                , timeStart(utimeNow(NULL))
                , enable(true)
                , timeUpdate(timeStart) {
            if (addr == NULL)
                memset(&this->addr, 0, sizeof(struct sockaddr_stage));
            else memcpy(&this->addr, addr, sizeof(struct sockaddr_stage));
        }
        virtual ~CLUSTER() { }

        virtual CLUSTER *Ref() { ObjectRef::Ref(); return this; }
    protected:
    	virtual void Dispose() { delete this; }
    }; typedef std::map<sockaddr_stage *, CLUSTER *, addrStageCompare> CLUSTER_addrSet;

    typedef std::deque<CLUSTER *> CLUSTER_useSet;
    typedef std::unordered_map<std::string, CLUSTER_useSet> CLUSTER_uidSet;

    CLUSTER_addrSet c__entry;
    CLUSTER_uidSet  c__uids;

    auto resetCluster = [&](CLUSTER *v, utime_t timeNow)
    {
        for (UID_timeSet::iterator u_it = v->begin(); u_it != v->end();)
            if (u_it->second >= timeNow)
                ++u_it;
            else {
                /* 등록시간이 timeNow보다 이전이면 제거한다. */
                CLUSTER_uidSet::iterator v_it = c__uids.find(u_it->first.c_str());
                if (v_it != c__uids.end())
                {
                    CLUSTER_useSet::iterator f_it =
                            std::find_if(v_it->second.begin(), v_it->second.end(), [&](CLUSTER *c) {
                                return c == v;
                            });
                    if (f_it != v_it->second.end())
                    {
                        v_it->second.erase(f_it);
                        if (v_it->second.empty())
                            c__uids.erase(v_it);

                        _INFO("UNREGISTER STAGE: %p - '%s'", v, u_it->first.c_str());
                        v->Unref();
                    }
                }
                u_it = v->erase(u_it);
            }

        return v;
    };

    /* 실행에 대한 베타적 실행 보장 */
    struct TICKET: public ObjectRef {
        utime_t timeUpdate;

        struct TASK: public ObjectRef
        		   , public std::string {
            utime_t     timeReady;
            std::string stage_id;

            struct sockaddr_stage addr;
            TASK(struct sockaddr_stage *addr)
                    : ObjectRef("TICKET::TASK")
                    , timeReady(-1) {
                memcpy(&this->addr, addr, sizeof(struct sockaddr_stage));
            }
            virtual ~TASK() { }

            virtual int signal(int feedback, const char *id, bson *o, uint64_t timeBreak = 0) {
                return stage_signal(feedback, id, o, timeBreak, &this->addr);
            }

            virtual TASK *Ref() { ObjectRef::Ref(); return this; }
        protected:
	        virtual void Dispose() { delete this; }
        }; std::deque<TASK *> readys;

        TICKET(): ObjectRef("TICKET"), timeUpdate(0) { }
        virtual ~TICKET() {
        	for (;readys.empty() == false; readys.pop_front())
        		readys.front()->Unref();
        }

	    virtual TICKET *Ref() { ObjectRef::Ref(); return this; }
    protected:
    	virtual void Dispose() { delete this; }
    }; typedef std::unordered_map<std::string, TICKET *> TICKET_lockSet;

    TICKET_lockSet tickets;

    /* */
    struct CLUSTER__R: public CLUSTER_entrySet {
        int fd;

        CLUSTER__R(): CLUSTER_entrySet(), fd(-1) { }
        virtual ~CLUSTER__R()
        {
            for (CLUSTER_entrySet::iterator a_it = this->begin(); a_it != this->end(); ++a_it)
                freeaddrinfo(*a_it);
        }
    };

    CLUSTER__R *cluster = new CLUSTER__R();
    X__Adapter *x_eadp = new X__Adapter();

    if (!(x_eadp->eadp = eadapter_create(MAX_POLLFD, NULL))) _EXIT("%d: %s", __LINE__, strerror(errno));
    {
        auto efile_link = [](eadapter eadp, int fd)
        {
            if (fd <= 0)
                return -1;
            else {
                efile f;

                if (!(f = efile_open(fd, EPOLLIN|EPOLLERR, NULL)))
                    _EXIT("%d: %s", __LINE__, strerror(errno));
                else if (efile_setadapter(f, eadp) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
            } return 0;
        };

        efile_link(x_eadp->eadp, ECTRL->fd[AF_UNIX]);
        if (JsonConfig.isMember("#cluster"))
        {
            Json::Value v = JsonConfig["#cluster"];
            struct sockaddr_in c__addr;

            c__addr.sin_port = 0;
            if (v.isArray())
            {
                /* 클러스터링 연결할 서버의 주소를 얻는다. */
                struct addrinfo hints;

                memset(&hints, 0, sizeof(hints));
                hints.ai_flags = AI_PASSIVE;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                /*
                 * [
                 *   ":DEVICE_NAME",
                 *   LISTEN_PORT,
                 *   "=SERVER_IP:PORT", -- stage가 없는 경우 해당 서버로 전달
                 *   "ADDR:PORT"
                 * ]
                 */
                for (int __i = 0; __i < (int)v.size(); ++__i)
                {
                    Json::Value i__v = v[__i];

                    if (i__v.isString()) switch (*(i__v.asCString()) + 0)
                    {
                        case '=': /* clustering */
                        {
                            std::string host_or_ip = i__v.asCString() + 1;
                            char *port = (char *)strchr(host_or_ip.c_str(), ':');

                            if (port == NULL)
                                _WARN("%d: IGNORE - '%s'", __LINE__, host_or_ip.c_str())
                            else {
                                struct addrinfo *entry = NULL;

                                *port++ = 0;
                                if (getaddrinfo(host_or_ip.c_str(), port, &hints, &entry) >= 0)
                                    cluster->push_back(entry);
                            }
                        } break;
                        default: break;
                    }
                    else if (i__v.isInt()) c__addr.sin_port = htons(i__v.asInt());
                }
            }
            else if (v.isInt()) c__addr.sin_port = htons(v.asInt());

            if (c__addr.sin_port <= 0)
                ;
            else {
                c__addr.sin_family = AF_INET;
                c__addr.sin_addr.s_addr = INADDR_ANY;
                if ((cluster->fd = udp_server(&c__addr)) < 0)
                    _EXIT(strerror(errno));
                else {
                    efile_link(x_eadp->eadp, cluster->fd);
                } ECTRL->fd.insert(STAGE__CTRL::IO_fdSet::value_type(c__addr.sin_family, cluster->fd));
                _INFO("READY %d: CLUSTER - %d port", getpid(), ntohs(c__addr.sin_port));
            }
        }
    } if (THREAD__EXECUTOR::create([&x_eadp](pthread_attr_t *attr) {
        pthread_t thread;

        return pthread_create(&thread, attr, (void *(*)(void *))X__Adapter::worker, x_eadp);
    }) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));

    stage_setproctitle("${program}: cluster");

#pragma pack(push, 1)
    struct BSON_HEAD {
        int       length;
        bson_type type;
    };
#pragma pack(pop)

    auto T__SwitchLock = [](TICKET *o, const char *k)
    {
        for (TICKET::TASK *r;o->readys.empty() == false; o->readys.pop_front())
            if ((r = o->readys.front())->timeReady == UINT64_MAX)
                break;
            else {
                bson_scope b_v;

                r->timeReady = UINT64_MAX;
                if (r->length() == 0)
                    bson_append_null(&b_v, k);
                else {
                    BSON_HEAD *v = (BSON_HEAD *)r->data();

                    switch (v->type)
                    {
                        case BSON_OBJECT: bson_append_string(&b_v, "%", k);
                        {
                            bson_iterator b_it;

                            bson_iterator_from_buffer(&b_it, (char *)(v + 1));
                            bson_append_element(&b_v, NULL, &b_it);
                        } break;
                        default: bson_append_value(&b_v, k, v->type, (char *)(v + 1), v->length);
                    }
                } bson_finish(&b_v);

                _TRACE("SWITCH LOCK: %s", k);
                if (r->signal(-1, NULL, &b_v) >= 0)
                {
                    o->timeUpdate = utimeNow(NULL);
                    break;
                }
                r->Unref();
            }

        return (o->readys.empty() == false);
    };

#define DROP_TIMEOUT            500
    int r__timeExpire = DROP_TIMEOUT; /* 라우팅 허용 시간 */

    if (JsonConfig.isMember("#router"))
    {
        Json::Value v = JsonConfig["#router"];
        if (v.isArray())
            for (int __i = 0; __i < (int)v.size(); ++__i)
            {
                Json::Value i__v = v[__i];

                switch (__i)
                {
                    case 0: r__timeExpire = (int)(i__v.asInt() * 1.9);
                    default: break;
                }
            }
    }
#undef DROP_TIMEOUT

    INSPECTOR inspector(0);
    X__Adapter::BufferSet x__buffers;
    utime_t x__timeBreak;

_gW:THREAD_EXECUTOR->adjust();
#define DEF_TIMEOUT             100
    /* 일정 시간 이상 락이 해제되지 않으면 강제로 전환한다. */
    {
        utime_t timeNow = utimeNow(NULL);

        for (TICKET_lockSet::iterator o_it = tickets.begin(); o_it != tickets.end();)
        {
            TICKET *o = o_it->second;

            if ((timeNow < o->timeUpdate) ||
                    ((timeNow - o->timeUpdate) < DEF_TIMEOUT))
                ++o_it;
            else {
                for (TICKET::TASK *r;o->readys.empty() == false;)
                    if ((r = o->readys.front())->timeReady != UINT64_MAX)
                        break;
                    else {
                        _WARN("LOCK: RELEASE %s", o_it->first.c_str());
                        o->timeUpdate = timeNow;
                        {
                            r->Unref();
                            o->readys.pop_front();
                        }
                    }

                if (T__SwitchLock(o, o_it->first.c_str()))
                    ++o_it;
                else {
                    o->Unref();
                    o_it = tickets.erase(o_it);
                }
            }
        }

        x__timeBreak = timeNow + DEF_TIMEOUT;
        inspector.commit([&](struct INSPECTOR::USAGE *u) {
            u->cu_tickets = tickets.size();
        }, timeNow);
    }

    if (x__buffers.empty())
    {
        pthread_mutex_lock(&x_eadp->lock); {
            struct timespec ts; utimeSpec(NULL, DEF_TIMEOUT, &ts);

            for (int r; x_eadp->buffers.empty(); )
                switch (r = pthread_cond_timedwait(&x_eadp->wait, &x_eadp->lock, &ts))
                {
                    default: _EXIT("%d: %s", __LINE__, strerror(r));
                    case ETIMEDOUT: pthread_mutex_unlock(&x_eadp->lock);
                    {
                        ;
                    } goto _gW;
                    case 0: break;
                }

            x__buffers.swap(x_eadp->buffers);
        } pthread_mutex_unlock(&x_eadp->lock);
    }

    /* */
_gB:inspector.begin(); {
        struct X__Adapter::Buffer *r_buffer = x__buffers.front();

#if 0
        fprintf(stderr, " CLUSTER_IN.%d:\n", r_buffer->addr.ss_family); bson_print_raw((char *)r_buffer->data(), 0);
#endif
        bson_iterator bson_it; bson_iterator_from_buffer(&bson_it, (char *)r_buffer->data());
        while (bson_iterator_next(&bson_it) != BSON_EOO)
        {
            const char *k = bson_iterator_key(&bson_it);

            switch (*(k + 0))
            {
                case 0: switch ((int)bson_iterator_type(&bson_it))
                {
                    case BSON_NULL:
                    {
                        CLUSTER_addrSet::iterator c_it = c__entry.find(&r_buffer->addr);
                        if (c_it == c__entry.end())
                            break;
                        else {
                            resetCluster(c_it->second, UINT64_MAX)->Unref();
                        } c__entry.erase(c_it);
                    } break;

                    case BSON_BOOL: /* _TRACE("RESUME STAGE"); */
                    {
                        CLUSTER_addrSet::iterator c_it = c__entry.find(&r_buffer->addr);
                        if (c_it == c__entry.end())
                            break;
                        else {
                            CLUSTER *v = c_it->second;

                            v->timeUpdate = utimeNow(NULL);
                            v->enable = v->empty() ? false: bson_iterator_bool(&bson_it);
                        }
                    } break;

                    case BSON_INT: _INFO("CLUSTER: READY - %d", bson_iterator_int(&bson_it));
                    {
                        if (kill(bson_iterator_int(&bson_it), 0) < 0)
                            _WARN("%d: %s", bson_iterator_int(&bson_it), strerror(errno))
                        else {
                            CLUSTER_addrSet::iterator c_it = c__entry.find(&r_buffer->addr);
                            if (c_it != c__entry.end())
                                c_it->second->timeUpdate = utimeNow(NULL);
                            else {
                                CLUSTER *v = new CLUSTER(&r_buffer->addr);
                                c__entry.insert(CLUSTER_addrSet::value_type(&v->addr, v->Ref()));
                            }
                        }
                    } break;

                    case BSON_ARRAY:
                    {
                        char ip_buf[BUFSIZ] = { 0, };

                        switch (r_buffer->addr.ss_family)
                        {
                            default:
                            {
                                struct sockaddr_in *in_addr = (struct sockaddr_in *)&r_buffer->addr;

                                if (!inet_ntop(in_addr->sin_family,
                                              &in_addr->sin_addr.s_addr, ip_buf, sizeof(ip_buf)))
                                    goto _gC;
                                sprintf(ip_buf + strlen(ip_buf), ":%d", ntohs(in_addr->sin_port));
                            } break;

                            case AF_UNIX: strcpy(ip_buf, ((struct sockaddr_un *)&r_buffer->addr)->sun_path);
                        }

                        CLUSTER *v;
                        {
                            CLUSTER_addrSet::iterator c_it = c__entry.find(&r_buffer->addr);
                            if (c_it != c__entry.end())
                                (v = c_it->second)->timeUpdate = utimeNow(NULL);
                            else {
                                v = new CLUSTER(&r_buffer->addr);
                                c__entry.insert(CLUSTER_addrSet::value_type(&v->addr, v->Ref()));
                            }
                        }

                        /* 사용되는 stage의 등록시간을 갱신한다. */
                        bson_iterator v_it; bson_iterator_subiterator(&bson_it, &v_it);
                        while (bson_iterator_next(&v_it) != BSON_EOO)
                            switch (bson_iterator_type(&v_it))
                            {
                                case BSON_STRING:
                                {
                                    const char *i = bson_iterator_string(&v_it);

                                    if (v->find(i) != v->end())
                                        ;
                                    else {
                                        CLUSTER_uidSet::iterator u_it = c__uids.find(i);
                                        if (u_it == c__uids.end())
                                            u_it = c__uids.insert(
                                                    CLUSTER_uidSet::value_type(i, CLUSTER_useSet())
                                            ).first;

                                        _INFO("CLUSTER: REGISTER %s - [%s]", i, ip_buf);
                                        u_it->second.push_back(v->Ref());
                                    }

                                    (*v)[i] = v->timeUpdate;
                                } break;
                                default: break;
                            }
                        resetCluster(v, v->timeUpdate); /* timeUpdate이전 stage 제거 */
                    } break;

                    default: break; /* BSON_STRING == stage_id */
                } break;

                /* */
                case '>': /* lock */
                case '<': _INFO("LOCK: %s", k); /* unlock */
                {
                    auto lockSign = [](const char *k) { /* ">>sign" => "sign" */
                        const char *x = k + 1;
                        while (*(x + 0) == *(k + 0))
                            ++x;
                        return x;
                    };

                    /* ">lock_object.stage_id": value */
                    std::string __k = lockSign(k);
                    char *stage_id = (char *)strchr(__k.c_str(), '.');

                    if (stage_id == NULL) break;
                    *stage_id++ = 0;
                    {
                        TICKET *o;

                        TICKET_lockSet::iterator o_it = tickets.find(__k.c_str());
                        if (o_it != tickets.end())
                            o = o_it->second;
                        else {
                            if (*(k + 0) != '>')
                                break;

                            o_it = tickets.insert(
                                    TICKET_lockSet::value_type(__k.c_str(), (o = new TICKET())->Ref())
                            ).first;
                        }

                        switch (*(k + 0))
                        {
                            case '>': switch (*(k + 1))
                            {
                                /* 사용 시간 연장: ">>lock_id" */
                                case '>': if (o->readys.empty() == false)
                                {
                                    TICKET::TASK *r = o->readys.front();

                                    if (r->stage_id.compare(stage_id))
                                        _WARN("LOCK: UNDEFINED '%s'", __k.c_str())
                                    else {
                                        int timeExpand = 0;

                                        switch (bson_iterator_type(&bson_it))
                                        {
                                            case BSON_INT:
                                            case BSON_LONG:
                                            {
                                                timeExpand = bson_iterator_int(&bson_it);
                                            } break;
                                            default: break;
                                        } o->timeUpdate = utimeNow(NULL) + timeExpand;
                                        _INFO("LOCK: RENEWAL %s - %d msec", __k.c_str(), timeExpand);
                                    }
                                } break;

                                default:
                                {
                                    TICKET::TASK *r = new TICKET::TASK(&r_buffer->addr);

                                    r->timeReady = utimeNow(NULL);
                                    r->stage_id = stage_id;
                                    /* lock arg를 저장한다. */
                                    {
                                        int length = bson_iterator_size(&bson_it);

                                        r->resize(length + sizeof(struct BSON_HEAD)); {
                                            BSON_HEAD *v = (BSON_HEAD *)r->data();

                                            v->length = length;
                                            v->type = bson_iterator_type(&bson_it);
                                            memcpy(v + 1, bson_iterator_value(&bson_it), length);
                                        }
                                    }
                                    o->readys.push_back(r->Ref());
                                }

_gO:                            if (T__SwitchLock(o, __k.c_str()) == false)
                                {
                                    o->Unref();
                                    tickets.erase(o_it);
                                } break;
                            } break;

                            default:
                            {
                                TICKET::TASK *r = o->readys.front();

                                if (r->stage_id.compare(stage_id))
                                    break;
                                else {
                                    o->timeUpdate = utimeNow(NULL);
                                    {
                                        r->Unref();
                                        o->readys.pop_front();
                                    }
                                }
                            } goto _gO;
                        }
                    }
                } break;

                /* */
                default: _TRACE("ROUTE: '%s'", k);
                {
                    // std::string kv = k__p ? (k__p + 1): k;
                    auto createClusterBuffer = [](const char *kv, bson_iterator &bson_it)
                    {
                        bson b_o; bson_init(&b_o);
                        switch (bson_iterator_type(&bson_it))
                        {
                            case BSON_ARRAY:
                            case BSON_OBJECT: /* object 정보를 제거한다. */
                            {
                                bson_iterator pass_it;

                                bson_iterator_subiterator(&bson_it, &pass_it);
                                if (*(kv + 0) != 0)
                                    bson_append_string(&b_o, "%", kv);
                                bson_append_element(&b_o, NULL, &pass_it);
                            } break;
                            default: bson_append_element(&b_o, kv, &bson_it);
                        } bson_finish(&b_o);
                        return b_o;
                    };

                    std::string k__v = k;

                    char *k__p = (char *)strchr(k = k__v.c_str(), '$'); /* "stage_id$stage_value" */
                    if (k__p != NULL) *(k__p + 0) = 0;

#if 0
                    fprintf(stderr, " ROUTE:"); bson_print_raw((char *)r_buffer.data(), 0);
#endif
                    while (*(k + 0) && strchr("*=+", *(k + 0))) ++k; /* */
                    switch (*(k + 0))
                    {
                        case 0: /* "event_id" 만 존재한다면 데이터를 보낸 서버로 다시 보낸다. */
                        {
                            CLUSTER_addrSet::iterator c_it = c__entry.find(&r_buffer->addr);
                            if (c_it != c__entry.end())
                            {
                                CLUSTER *v = c_it->second;

                                bson b_o = createClusterBuffer(k__p ? (k__p + 1): k, bson_it);
                                if (stage_signal(-1, NULL, &b_o, 0, &v->addr) >= 0)
                                    ;
                                else {
                                    v->enable = false;
                                    _WARN("ROUTE: FEEDBACK.%d - %s", v->addr.ss_family, strerror(errno))
                                } bson_destroy(&b_o);
                            }
                        } break;

                        default: /* route_id 를 찾는다. */
                        {
                            CLUSTER_addrSet allows;
                            utime_t timeNow = utimeNow(NULL);
                            int i__signal = 0;

                            for (CLUSTER_addrSet::iterator c_it = c__entry.begin(); c_it != c__entry.end(); ++c_it)
                            {
                                CLUSTER *v = c_it->second;

                                if (v->enable && ((int)(timeNow - v->timeUpdate) < r__timeExpire))
                                    switch (*(k__v.c_str() + 0))
                                    {
                                        case '+': /* 외부 연결만 전달 */
                                        {
                                            if (v->addr.ss_family != AF_UNIX)
_gI:                                            allows.insert(CLUSTER_addrSet::value_type(c_it->first, v));
                                        } break;
                                        case '=': if (v->addr.ss_family == AF_UNIX)  /* 동일 네트워크 */
                                        default :    goto _gI;  /* 전체 */
                                    }
                            }

                            if (allows.empty() == false)
                            {
                                struct CLUSTER_useScope: public CLUSTER_useSet {
                                    CLUSTER_useScope(CLUSTER_useSet &i)
                                        : CLUSTER_useSet()
                                        , u_v(i) { }
                                    virtual ~CLUSTER_useScope() {
                                        u_v.insert(u_v.end(), this->begin(), this->end());
                                    }

                                    CLUSTER_useSet *operator ->() { return &u_v; }
                                private:
                                    CLUSTER_useSet &u_v;
                                };

                                bool i__balance = (*(k__v.c_str() + 0) != '*');
                                bson b_o = createClusterBuffer(k__p ? (k__p + 1): k, bson_it);

                                std::stringset k__u; std::tokenize(k, k__u, ",");
                                for (std::stringset::iterator s_it = k__u.begin(); s_it != k__u.end(); ++s_it)
                                {
                                    CLUSTER_uidSet::iterator u_it = c__uids.find(*s_it);
                                    if (u_it != c__uids.end())
                                    {
                                        CLUSTER_useScope u(u_it->second);

                                        for (CLUSTER_useSet::iterator c_it = u->begin(); c_it != u->end();)
                                        {
                                            CLUSTER *v = (*c_it);

                                            if (allows.find(&v->addr) == allows.end()) ++c_it;
                                            else if (stage_signal(-1, NULL, &b_o, 0, &v->addr) < 0)
                                            {
                                                c_it = u->erase(c_it); {
                                                    v->enable = false;
                                                    u.push_back(v);
                                                } _WARN("ROUTE: TARGET.%d - %s", v->addr.ss_family, strerror(errno))

                                                allows.erase(&v->addr);
                                                if (allows.empty()) goto _gE;
                                            }
                                            else {
                                                ++i__signal;
                                                if (i__balance == false)
                                                    ++c_it;
                                                else {
                                                    c_it = u->erase(c_it); {
                                                        u.push_front(v);
                                                    } break;
                                                }
                                            }
                                        }
                                    }
                                } /* for */

_gE:                            bson_destroy(&b_o);
                            }
                            else _WARN("ROUTE: NOT ALLOW - '%s'", k)

                            if (i__signal > 0);
                            else if ((cluster->fd > 0) && (cluster->empty() == false))
                            {
                                bson_scope b_o;
                                {
                                    /* route 정보가 포함되어 하므로 bson_it을 그대로 전달한다. */
                                    std::string k = bson_iterator_key(&bson_it);

                                    /* 외부 네트워크 주소로만 전달 처리 */
                                    if (*(k.c_str() + 0) != '+') k.insert(0, "+");
                                    bson_append_element(&b_o, k.c_str(), &bson_it);
                                } bson_finish(&b_o);

                                for (CLUSTER_entrySet::iterator
                                             a_it = cluster->begin(); a_it != cluster->end(); ++a_it)
                                {
                                    if (sendto(cluster->fd, bson_data(&b_o), bson_size(&b_o),
                                               MSG_NOSIGNAL, (*a_it)->ai_addr, (*a_it)->ai_addrlen) < 0)
                                        _WARN("ROUTE: CLUSTER CHAIN - %s", __LINE__, strerror(errno));
                                }
                            }
                            else _WARN("ROUTE: CANCEL - '%s'", k)
                        } break;
                    }
                } break;
            }
_gC:        ;
        } inspector.end();

        delete r_buffer; x__buffers.pop_front();
        if (x__buffers.empty() || (utimeNow(NULL) >= x__timeBreak))
            goto _gW;
    } goto _gB;
}

/* */
#include <fstream>
#include <iostream>
#include <dlfcn.h>

#include <net/if.h>
#include <setjmp.h>
#include <sys/stat.h>

static sigjmp_buf      __gSignalBreak;
static std::atomic_int __iSignalCHLD;

static void SIG__BREAK(int signo)
{
    signal(signo, SIG_IGN);
    siglongjmp(__gSignalBreak, signo);
}

static void SIG__CHLD(int signo)
{
    for (int status; waitpid(-1, &status, WNOHANG) > 0; )
        ++__iSignalCHLD;
}


struct LOG4CXX_WATCH: public std::string {
    time_t mtime;

    LOG4CXX_WATCH(const char *f)
            : std::string(f)
            , mtime(0) {
    }

    int update(std::function<bool(const char *f)> callback)
    {
        struct stat st;

        if (lstat(this->c_str(), &st) < 0);
        else if (S_ISREG(st.st_mode))
        {
            if (mtime == st.st_mtime)
                return 0;
            else {
                mtime = st.st_mtime;
            } return (callback ? callback(this->c_str()): true);
        }
        return -1;
    }

    typedef std::deque<LOG4CXX_WATCH> WatchSet;

    static void __worker(WatchSet *_this)
    {
        utime_t timeUpdate = 0;

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

        pthread_cleanup_push((void (*)(void *))__destroy, _this);
#define MAX_TIMEWAIT            1000
        do {
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); {
                pthread_testcancel();
            } pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

            if ((utimeNow(NULL) - timeUpdate) > MAX_TIMEWAIT)
            {
                for (WatchSet::iterator w_it = _this->begin(); w_it != _this->end(); ++w_it)
                {
                    if ((*w_it).update([](const char *f) {
                        try {
                            if (strstr(f, ".xml") != NULL)
                                log4cxx::xml::DOMConfigurator::configure(f);
                            else log4cxx::PropertyConfigurator::configure(f);

                            _INFO("LOG4CXX %d: UPDATE - '%s'", getpid(), f);
                        } catch (log4cxx::helpers::Exception &e) { _WARN("LOG4CXX: %s", e.what()); }
                        return 0;
                    }) >= 0)
                        break;
                }

                timeUpdate = utimeNow(NULL);
            }
        } while (stage_dispatch(MAX_TIMEWAIT) >= 0);
#undef MAX_TIMEWAIT
        pthread_cleanup_pop(0);
    }

private:
    static void __destroy(WatchSet *_this) { delete _this; }
};


int main(int argc, char *argv[], char **envp)
{
    static LOG4CXX_WATCH::WatchSet *log4cxx_watcher = new LOG4CXX_WATCH::WatchSet();
    std::string json_config = std::format("%s.json", argv[0]).c_str();
    std::string suspend_config = std::format("%s.stop", argv[0]).c_str();

    setlocale(LC_ALL, ""); // Hacking log4cxx
    {
        static struct option long_options[] = {
                {"config",  required_argument, NULL, 'c'},
                {"log4cxx", required_argument, NULL, 'l'},
                {"suspend", required_argument, NULL, 's'},
                {"help",    no_argument,       NULL, 'h'},
                {"version", no_argument,       NULL, 'v'},
                {NULL,      0,                 NULL, 0}
        };
        std::string log4cxx_config = std::format("%s.log4cxx", argv[0]).c_str();

        int r;
        do {
            int option_index = 0;

            switch (r = getopt_long(argc, argv, "c:l:s:vh?", long_options, &option_index))
            {
                case -1: break;
                case 'c': json_config = optarg; break;
                case 'l': log4cxx_config = optarg; break;
                case 's': suspend_config = optarg; break;
                case 'v': _exit(fprintf(stderr, "%s BUILD [%s %s]\n", argv[0], __DATE__, __TIME__));

                default : _exit(fprintf(stderr, "%s: invalid option -- '%c'\n"
                                                "Try '%s --help' for more information.\n", argv[0], r, argv[0]));
                case 'h':
                {
                    fprintf(stderr,
                            "Usage: %s [OPTION]... [arguments] \n"
                            "\n"
                            "Mandatory arguments to long options are mandatory for short options too.\n"
                            "  -c, --config=CONFIG.json[:SECOND.json]  configuration file required for execution.\n"
                            "  -l, --log4cxx=config.xml[:second.xml]   log4cxx configuration file.\n"
                            "  -s, --suspend=filename                  File for scale down setting.\n"
                            "      --help                              display this help and exit\n"
                            "      --version                           output version information and exit\n"
                            "\n", argv[0]);
                } _exit(0);
            }
        } while (r != -1);
        {
            std::stringset files;

            std::tokenize(log4cxx_config, files, ":");
            for (std::stringset::iterator f_it = files.begin(); f_it != files.end(); ++f_it)
            {
                log4cxx_watcher->push_front(LOG4CXX_WATCH((*f_it).c_str()));
                if (log4cxx_watcher->front().update([](const char *f) {
                    try {
                        if (strstr(f, ".xml") != NULL)
                            log4cxx::xml::DOMConfigurator::configure(f);
                        else log4cxx::PropertyConfigurator::configure(f);

                        _INFO("LOG4CXX: CONFIGURATION - '%s'", f);
                    } catch (log4cxx::helpers::Exception &e) { _WARN("LOG4CXX: %s", e.what()); }
                    return 0;
                }) >= 0) goto _gS;
            }
            log4cxx::BasicConfigurator::configure();
        }
_gS:    ;
    }

    static Json::Value JsonConfig;
    try {
        std::stringset files;
        Json::Reader reader;

        std::tokenize(json_config, files, ":");
        for (std::stringset::iterator f_it = files.begin(); f_it != files.end(); ++f_it)
        {
            if ((*f_it) == "-") reader.parse(std::cin, JsonConfig);
            else if (access((*f_it).c_str(), 0) == 0)
            {
                std::ifstream json_file((*f_it).c_str());

                if (json_file.is_open())
                {
                    Json::Value root;

                    if (reader.parse(json_file, root))
                        for (const auto &k : root.getMemberNames())
                        {
                            /* 지정되지 않은 경우에만 설정 */
                            if (JsonConfig.isMember(k) == false)
                                JsonConfig[k] = root[k];
                        }
                }
                json_file.close();
            }
        }

        if (JsonConfig.empty()) _EXIT("Config file does not exist.");
        else {
            if (unlink(suspend_config.c_str()) < 0)
                switch (errno)
                {
                    case ENOENT: break;
                    default: _EXIT("%d: %s", __LINE__, strerror(errno));
                }
        } (ESYS = new STAGE__SYS(argc, argv, envp))->f__suspend = suspend_config.c_str();
        for (; optind < argc; ++ESYS->l__argc)
        {
            if (!(ESYS->l__argv[ESYS->l__argc] = strdup(argv[optind++])))
                _EXIT("%d: %s", __LINE__, strerror(errno));
        }
        /* */
    } catch (Json::RuntimeError e) { _WARN(e.what()); }

    if (JsonConfig.isMember("#startup") == false) _EXIT("Initialization settings are not defined.");
    {
        Json::Value V = JsonConfig["#services"];

        /*
         * "#services": {
         *    "tcp": "0.0.0.0:8080"
         * }
         */
        if (V.isObject())
        {
            Json::Value::Members members = V.getMemberNames();
            for (Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it)
            {
                Json::Value v = V[*it];
                int socktype = SOCK_STREAM;

                if (*it == "tcp");
                else if (*it == "udp") socktype = SOCK_DGRAM;
                else _EXIT("'%s' - Not supported", v.asCString());

                if (v.isArray() == false) af__socket(socktype, v);
                else for (int __x = 0; __x < (int)v.size(); ++__x) af__socket(socktype, v[__x]);
            }
        }
    }

    /* */
    struct PROCESS: public ObjectRef
    		      , public Json::Value {
        int         stage_id;
        utime_t     timeStart;

        struct sockaddr_stage addr;
        PROCESS(int __i, Json::Value k)
            : ObjectRef("PROCESS")
            , Json::Value(k)
            , stage_id(__i)
            , timeStart(utimeNow(NULL))
            , pid_(getpid()) {
            {
                struct sockaddr_un *addr = (struct sockaddr_un *)&this->addr;

                addr->sun_family = AF_UNIX;
                sprintf(addr->sun_path, ".__%d.%d", getpid(), __i);
            } this->addr.ss_length = sizeof(struct sockaddr_un);
        }
        virtual ~PROCESS() {
            if (pid_ == getpid()) this->reset();
        }

        virtual PROCESS *Ref() { ObjectRef::Ref(); return this; }

        void reset() { unlink(((struct sockaddr_un *)&this->addr)->sun_path); }
    protected:
    	virtual void Dispose() { delete this; }
    private:
        pid_t pid_;
    }; typedef std::unordered_map<pid_t, PROCESS *> PROCESS_idSet;

    static PROCESS_idSet preforks;

    /* */
    auto PROCESS_initStage = [](PROCESS *f__i, struct sockaddr_stage *p__addr) {
#define MAX_SIOCP               64
#define MAX_MESSAGE_OBJECTS     100000
        for (PROCESS_idSet::iterator p_it = preforks.begin(); p_it != preforks.end(); )
        {
            if (p_it->first == 0)
                ++p_it;
            else {
                p_it->second->Unref();
                p_it = preforks.erase(p_it);
            }
        }

        if (eio_init(0) < 0) _EXIT("%d: %d - %s", __LINE__, getpid(), strerror(errno));
        {
            int ncpus = 0;
            int nobjects = MAX_MESSAGE_OBJECTS;

            if (JsonConfig.isMember("#threads"))
            {
                Json::Value V = JsonConfig["#threads"];

                if (V.isInt()) ncpus = V.asInt();
                else if (V.isArray())
                {
                    ncpus = V[0].asInt();
                    if (V.size() > 1)
                        nobjects = V[1].asInt();
                }
            }

            if (sio_init(MAX_SIOCP, nobjects) < 0)
                _EXIT("%d: %d - %s", __LINE__, getpid(), strerror(errno));

            _INFO("READY %d: THREAD - %d cpu's, %d object's", getpid(), ncpus, nobjects);
            try {
                THREAD_EXECUTOR = new THREAD__EXECUTOR(ncpus);
            } catch (...) { _EXIT("%d: %s", __LINE__, strerror(errno)); }
            if (THREAD__EXECUTOR::create([](pthread_attr_t *attr) {
                pthread_t thread;

                return pthread_create(&thread, attr,
                                      (void *(*)(void *))LOG4CXX_WATCH::__worker, (void *)log4cxx_watcher);
            }) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
        };
#undef MAX_MESSAGE_OBJECTS
#undef MAX_SIOCP

        signal(SIGHUP, SIG_DFL);
        try {
            ECTRL = new STAGE__CTRL(f__i->stage_id, p__addr);
            {
                int fd;

                if ((fd = socket(f__i->addr.ss_family, SOCK_DGRAM, 0)) < 0)
                    _EXIT("%d: %d - %s", __LINE__, getpid(), strerror(errno));
                {
                    int f;

                    if (((f = fcntl(fd, F_GETFL, 0)) < 0) ||
                            (fcntl(fd, F_SETFL, f | O_NONBLOCK) < 0))
                        _EXIT("%d: %d - %s", __LINE__, getpid(), strerror(errno));
                }

                f__i->reset(); {
                    if (bind(fd, (struct sockaddr *)&f__i->addr, f__i->addr.ss_length) < 0)
                        _EXIT("%d: %d - %s", __LINE__, getpid(), strerror(errno));

                    memcpy(&ECTRL->l__addr, &f__i->addr, sizeof(struct sockaddr_stage));
                } ECTRL->fd.insert(STAGE__CTRL::IO_fdSet::value_type(f__i->addr.ss_family, fd));
            }
        } catch (...) { _EXIT("%d: %s", __LINE__, strerror(errno)); }

        /* ECTRL->commit([](struct sockaddr_stage *x__addr, bson *b_o) {
            bson_append_int(b_o, "", getpid());
            return 0;
        }); */
    };

    auto PROCESS_localStage = [&PROCESS_initStage, &argc, &argv](PROCESS *f__i, struct sockaddr_stage *p__addr, Json::Value modules) {
        int (*stage__main)(int, char **, struct sockaddr_stage *, void *) = NULL;

        auto importPlugIn = [&](struct sockaddr_stage *addr, int i__id, bson *b_o, Json::Value &v)
        {
            void *dl = RTLD_DEFAULT;
            std::string f = v.asCString();

            _INFO("BEGIN %d: STAGE - %d ['%s']", getpid(), f__i->stage_id, v.asCString());
            {
                char *__p = (char *)strrchr(f.c_str(), '+'); /* name+UID */
                if (__p != NULL)
                {
                    ENTER_RWLOCK(&ECTRL->rwlock); {
                        ECTRL->modules.insert(
                                STAGE__CTRL::MODULE_addSet::value_type(__p + 1, i__id)
                        );
                    } LEAVE_RWLOCK(&ECTRL->rwlock);
                    if (b_o != NULL)
                        bson_append_string(b_o, std::format("#%d", i__id).c_str(), __p + 1);
                    *(__p + 0) = 0;
                }
            }

            if (*(f.c_str() + 0) != '=')
            {
                char *__x = (char *)strrchr(f.c_str(), '@');

                if (__x != NULL) *(__x + 0) = 0;    /* "startup.so@main" => "startup.so" */
                if (!(dl = dlopen(f.c_str(), RTLD_NOW|RTLD_LAZY|RTLD_GLOBAL)))
                    _EXIT("%d - %s: %s", f__i->stage_id, v.asCString(), dlerror());
                else {
                    if (__x != NULL) /* "startup.so@main" => "main" */
                        *((void **) (&stage__main)) = dlsym(dl, __x + 1);
                }
            }

            {
                char *__x = (char *)strrchr(f.c_str(), '/');

                /* "path/__startup.so" => "startup" OR "=lua" => "lua" */
                for (__x = (__x ? (__x + 1): (char *)f.c_str());
                        isalpha(*(__x + 0)) == false; ++__x)
                    ;
                {
                    char *__p = strchr(__x, '.');   /* startup.so */
                    if (__p != NULL) *(__p + 0) = 0;    /* startup */
                }

                {
                    utime_t timeNow = utimeNow(NULL);
                    int (*__stage)(const char *k, struct sockaddr_stage *,
                            void * /* Json::Value * */, stage_callback *, void **u);

                    *((void **) (&__stage)) = dlsym(dl, std::format("__%s__stage__", __x).c_str());
                    if (__stage == NULL) _EXIT("%d: %d - '%s': %s", __LINE__, f__i->stage_id, __x, dlerror());
                    {
                        Json::Value c;
                        {
                            std::string c_i = v.asCString();

                            if (JsonConfig.isMember(c_i.c_str()))
                                c = JsonConfig[c_i.c_str()];
                            else {
                                char *c__e = (char *)strchr(c_i.c_str(), '+');
                                if (c__e != NULL)
                                {
                                    *(c__e + 0) = 0;
                                    c = JsonConfig.get(c_i.c_str(), Json::Value());
                                }
                            }
                        }

                        stage_callback closure = NULL;
                        void *udata = NULL;

                        int r = (*__stage)(v.asCString(), addr, &c, &closure, &udata);
                        if (closure != NULL)
                        {
                            STAGE__CTRL::TASK *task = new STAGE__CTRL::TASK(timeNow, closure, udata);

                            ENTER_RWLOCK(&ECTRL->rwlock); {
                                ECTRL->callbacks.push_back(task->Ref());
                                if (r >= 0)
                                {
                                    task = (STAGE__CTRL::TASK *)pq_push(ECTRL->tasks, task->Ref());
                                    if (task != NULL)
                                    {
                                        _WARN("TASK: OVERFLOW");
                                        task->Unref();
                                    }
                                }
                            } LEAVE_RWLOCK(&ECTRL->rwlock);
                        }
                    }
                }
            }
        };

        PROCESS_initStage(f__i, p__addr);
        if (JsonConfig.isMember("#router"))
        {
            Json::Value v = JsonConfig["#router"];
            if (v.isArray())
                for (int __i = 0; __i < (int)v.size(); ++__i)
                {
                    Json::Value i__v = v[__i];
                    switch (__i)
                    {
                        case 0:
                        case 1: ECTRL->timeNotify = i__v.asInt();
                        default: break;
                    }
                }
        }

        /* ECTRL->commit([&](struct sockaddr_stage *x__addr, bson *b_o) */{

            /* bson_append_start_array(b_o, ""); */
            if (modules.isArray())
                for (int __x = 0; __x < (int)modules.size(); ++__x)
                {
                    Json::Value &c = modules[__x];
                    if (c.isString())
                        importPlugIn(p__addr, __x, NULL /* b_o */, c);
                }
            else if (modules.isString()) importPlugIn(p__addr, 0, NULL /* b_o */, modules);
            /* bson_append_finish_array(b_o); */

            if (JsonConfig.isMember("#cluster"))
            {
                struct CLUSTER__K: public std::string {
                    CLUSTER_entrySet entry;
                    int fd;

                    CLUSTER__K(): fd(-1) { }
                    virtual ~CLUSTER__K() {
                        for (CLUSTER_entrySet::iterator a_it = entry.begin(); a_it != entry.end(); ++a_it)
                            freeaddrinfo(*a_it);
                    }
                };

                Json::Value v = JsonConfig["#cluster"];
                if (v.isArray())
                {
                    std::string r__ifa;
                    CLUSTER__K *k = new CLUSTER__K();

                    /* 클러스터링 연결할 서버의 주소를 얻는다. */
                    {
                        struct addrinfo hints;

                        memset(&hints, 0, sizeof(hints));
                        hints.ai_flags = AI_PASSIVE;
                        hints.ai_family = AF_INET;
                        hints.ai_socktype = SOCK_DGRAM;
                        /*
                         * [
                         *   ":DEVICE_NAME",
                         *   LISTEN_PORT,
                         *   "=SERVER_IP:PORT", -- stage가 없는 경우 해당 서버로 전달
                         *   "ADDR:PORT"
                         * ]
                         */
                        for (int __i = 0; __i < (int)v.size(); ++__i)
                        {
                            Json::Value i__v = v[__i];

                            if (i__v.isString()) switch (*(i__v.asCString()) + 0)
                            {
                                case ':': r__ifa = i__v.asCString() + 1;
                                case '=': break; /* clustering */

                                default: /* "device" OR "ip:port" */
                                {
                                    std::string host_or_ip = i__v.asCString();
                                    char *port = (char *)strchr(host_or_ip.c_str(), ':');

                                    if (port == NULL)
                                        _WARN("%d: IGNORE - '%s'", __LINE__, host_or_ip.c_str())
                                    else {
                                        struct addrinfo *entry = NULL;

                                        *port++ = 0;
                                        if (getaddrinfo(host_or_ip.c_str(), port, &hints, &entry) >= 0)
                                        {
                                            {
                                                char ip_buf[BUFSIZ];
                                                struct sockaddr_in *addr = (struct sockaddr_in *)entry->ai_addr;

                                                inet_ntop(addr->sin_family,
                                                          &addr->sin_addr.s_addr, ip_buf, sizeof(ip_buf));
                                                _INFO("CLUSTER %d: MEMBER - [%s:%d]", getpid(), ip_buf, ntohs(addr->sin_port));
                                            } k->entry.push_back(entry);
                                        }
                                    }
                                } break;
                            }
                        }
                    }

                    /* 클러스터링을 통해 데이터를 받을 수 있도록 udp 서버를 생성 (sin_port == 0 으로 랜덤 생성) */
                    {
                        struct sockaddr_in *addr = (struct sockaddr_in *)&ECTRL->r__addr;

                        addr->sin_family = AF_INET;
                        addr->sin_addr.s_addr = INADDR_ANY;
                        addr->sin_port = 0;
                        if ((k->fd = udp_server(addr)) < 0)
                            _EXIT("%d: %s", __LINE__, strerror(errno));
                        else {
                            ECTRL->r__addr.ss_length = sizeof(struct sockaddr_in);
                            if (getsockname(k->fd, (struct sockaddr *) addr, &ECTRL->r__addr.ss_length) < 0)
                                _EXIT("%d: %s", __LINE__, strerror(errno));
                        }

                        /* 자신의 네트워크 주소를 얻는다. */
                        if (isdigit(*(r__ifa.c_str() + 0)))
                        {
                            if (inet_pton(addr->sin_family, r__ifa.c_str(), (void *)&addr->sin_addr) < 0)
                                _EXIT("%d: %s", __LINE__, strerror(errno));
                            _INFO("CLUSTER %d: NETWORK CUSTOM - [%s]", getpid(), r__ifa.c_str());
                        }
                        else {
                            struct ifaddrs *if_addr = NULL;

                            if (getifaddrs(&if_addr) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
                            for (struct ifaddrs *ifa = if_addr; ifa != NULL; ifa = ifa->ifa_next)
                                if (ifa->ifa_addr == NULL);
                                else if (ifa->ifa_addr->sa_family == addr->sin_family)
                                {
                                    if ((r__ifa.length() == 0) ||
                                            (strncmp(ifa->ifa_name, r__ifa.c_str(), r__ifa.length()) == 0))
                                    {
                                        if (ifa->ifa_flags & IFF_LOOPBACK);
                                        else if (ifa->ifa_flags & (IFF_UP|IFF_RUNNING))
                                        {
                                            addr->sin_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                                            {
                                                char ip_buf[BUFSIZ];

                                                inet_ntop(addr->sin_family,
                                                        &addr->sin_addr.s_addr, ip_buf, sizeof(ip_buf));
                                                _INFO("CLUSTER %d: NETWORK %s - [%s]", getpid(), ifa->ifa_name, ip_buf);
                                            }
                                        }
                                    }
                                }
                            freeifaddrs(if_addr);
                        }

                        ECTRL->fd.insert(STAGE__CTRL::IO_fdSet::value_type(addr->sin_family, k->fd));
                        if (k->entry.empty())
                            delete k;
                        else {
                            /* k->timeUpdate 주기로 클러스터링 정보를 갱신한다. */
                            stage_addtask(utimeNow(NULL) - DEF_TIMEOUT, [](utime_t timeBreak, bson_iterator *x__it, struct sockaddr_stage *addr, void **udata) {
                                if (ESYS->suspend())
                                    _TRACE("SUSPEND %d: CLUSTER [%d msec]", getpid(), ECTRL->timeNotify)
                                else {
                                    CLUSTER__K *k = *((CLUSTER__K **)udata);
                                    bson_scope b_o;

                                    {
                                        bson_append_modules(&b_o, "", ECTRL);
                                    } bson_finish(&b_o);

                                    _TRACE("REGISTER %d: CLUSTER [%d msec]", getpid(), ECTRL->timeNotify);
                                    for (CLUSTER_entrySet::iterator a_it = k->entry.begin(); a_it != k->entry.end(); ++a_it)
                                    {
                                        if (sendto(k->fd, bson_data(&b_o), bson_size(&b_o),
                                                   MSG_NOSIGNAL, (*a_it)->ai_addr, (*a_it)->ai_addrlen) < 0)
                                        _WARN("%d: %s", __LINE__, strerror(errno));
                                    }
                                }
                                return ECTRL->timeNotify;
                            }, k);
                        }
                        _INFO("READY %d: CLUSTER [%d port]", getpid(), ntohs(addr->sin_port));
                    }
                }
            }
            /* return 0; */
        } /* ); */

        stage_setproctitle("${program}: %d - %s", f__i->stage_id, modules.toStyledString().c_str());
        if (stage__main == NULL)
            PROCESS_mainStage(ECTRL);
        else {
            if (THREAD__EXECUTOR::create([](pthread_attr_t *attr) {
                pthread_t thread;

                return pthread_create(&thread, attr, (void *(*)(void *))PROCESS_mainStage, ECTRL);
            }) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
            return (*stage__main)(argc, argv, p__addr, &JsonConfig);
        }
        return 0;
    };

    std::atexit([]() {
        static std::list<pid_t> waits;

        {
            for (PROCESS_idSet::iterator p_it = preforks.begin(); p_it != preforks.end(); ++p_it)
            {
                if ((p_it->first > 0) && (kill(p_it->first, SIGQUIT) == 0))
                    waits.push_back(p_it->first);

                p_it->second->Unref();
            }
            preforks.clear();
        } INSPECTOR::exit(ESYS->mqid);

        signal(SIGINT, [](int sig) {
            _WARN("USER BREAK");
            for (;waits.empty() == false; waits.pop_front())
                kill(waits.front(), SIGKILL);
            _exit(0);
        });

        for (int status; waits.empty() == false; waits.pop_front())
            if (waitpid(-1, &status, 0) < 0)
                switch (errno)
                {
                    case ECHILD: break;
                    default: _WARN("wait: %s", strerror(errno));
                }
        _INFO("SHUTDOWN %d", getpid());
        log4cxx::LogManager::shutdown();
    });

    /* init */
    static struct {
        PROCESS *f;
        jmp_buf  jmp;
    } X__F = { NULL, };

    switch (setjmp(X__F.jmp))
    {
        case 0:
        {
            Json::Value V = JsonConfig["#startup"];
            if (V.isArray() == false) return PROCESS_localStage(NULL, NULL, V);

            preforks.insert(
            		PROCESS_idSet::value_type(0, (X__F.f = new PROCESS(0, Json::Value::null))->Ref())
            	);
            {
                int p__fd[2], p__x = 0;

                if (pipe(p__fd) < 0) _EXIT(strerror(errno));
                {
                    pid_t pid = fork();

                    if (pid < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
                    else if (pid > 0)
                        preforks.insert(PROCESS_idSet::value_type(pid, preforks[0]->Ref()));
                    else { /* cluster manager */
                        close(p__fd[0]); {
                            PROCESS_initStage(X__F.f, NULL);
                            if (write(p__fd[1], &p__x, sizeof(p__x)) < 0)
                                _EXIT("%d: %s", __LINE__, strerror(errno));
                        } /* close(p__fd[1]); */

                        longjmp(X__F.jmp, 1);
                    } close(p__fd[1]);

                    if (read(p__fd[0], &p__x, sizeof(p__x)) < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
                } close(p__fd[0]);
            }

            /* fork manager */
            for (int __i = 0, __x = 0; __i < (int)V.size(); ++__i)
            {
                Json::Value &v = V[__i];

                if (v.isInt()) utimeSleep(v.asInt());
                else if (v.isString() || v.isArray())
                {
                    X__F.f = new PROCESS(++__x, v);
                    pid_t pid = fork();

                    if (pid < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
                    else if (pid == 0) longjmp(X__F.jmp, 1);
                    else preforks.insert(PROCESS_idSet::value_type(pid, X__F.f->Ref()));
                }
            }
        } break;

        default: _INFO("READY %d: STAGE - %d", getpid(), X__F.f->stage_id);
        {
            signal(SIGINT, SIG_IGN);
            signal(SIGCHLD, SIG_DFL);

            signal(SIGQUIT, [](int s) { _exit(0); });
            if (X__F.f->stage_id)
                return PROCESS_localStage(X__F.f, &preforks[0]->addr, *X__F.f);
        } return PROCESS_mainCluster(JsonConfig, &preforks[0]->addr);
    }

    {
        struct STAGE__USAGE {
            pid_t   pid;
            utime_t timeUpdate;

            struct INSPECTOR::USAGE u;
            STAGE__USAGE(pid_t pid, struct INSPECTOR::MSG *m)
                    : pid(pid)
                    , timeUpdate(m->timeNow) { memcpy(&u, &m->u, sizeof(u)); }
        }; typedef std::deque<STAGE__USAGE> STAGE_usageIn;
        typedef std::unordered_map<int, STAGE_usageIn> STAGE_usageSet;

        STAGE_usageSet S__usage;

        signal(SIGTERM, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        signal(SIGINT, SIG__BREAK);
        signal(SIGQUIT, SIG__BREAK);
        signal(SIGCHLD, SIG__CHLD);
        switch (sigsetjmp(__gSignalBreak, 1))
        {
            case SIGQUIT:
            case SIGINT: _EXIT("BREAK");

            case -1: _EXIT("%d: %d - KILL", __LINE__, errno);
            case 0:

#define IDLE_TIMEOUT                    100
_gW:        if (ESYS->dispatch<INSPECTOR::MSG>([&](uint16_t op, INSPECTOR::MSG *msg, int l) {
                STAGE__SYS::IPC_HEAD *head = ((STAGE__SYS::IPC_HEAD *)msg) - 1;

                if (preforks.find(head->mtype) != preforks.end())
                {
                    STAGE_usageSet::iterator m_it = S__usage.find(head->mtype);
                    if (m_it == S__usage.end())
                        m_it = S__usage.insert(
                                STAGE_usageSet::value_type(msg->stage_id, STAGE_usageIn())
                        ).first;
#define USAGE_TIMEOUT       (10 * 1000)
                    {
                        STAGE_usageIn *u = &(m_it->second);

                        if (u->empty() == false)
                        {
                            struct STAGE__USAGE &l = u->back();

                            if (l.pid != head->mtype) u->clear();
                            else if (l.u.ru.ru_maxrss < msg->u.ru.ru_maxrss)
                            {
                                struct INSPECTOR::USAGE &usage = msg->u;

                                _WARN("USAGE %d: CPU time %ld.%06ld, user %ld.%06ld, RSS %ld kb",
                                      msg->stage_id,
                                      usage.ru.ru_utime.tv_sec, usage.ru.ru_utime.tv_usec,
                                      usage.ru.ru_stime.tv_sec, usage.ru.ru_stime.tv_usec,
                                      usage.ru.ru_maxrss)
                            }
                        }

                        u->push_back(STAGE__USAGE(head->mtype, msg));
                        while ((u->back().timeUpdate - u->front().timeUpdate) > USAGE_TIMEOUT)
                            u->pop_front();
                    }
#undef USAGE_TIMEOUT
                    INSPECTOR::log(msg);
                }
                return 0;
            }) < 0) switch (errno)
            {
                case ENOSYS:
                case ENOMSG:
                {
                    struct timeval timeOut = { 0, IDLE_TIMEOUT * 100 };

                    switch (select(0, NULL, NULL, NULL, &timeOut))
                    {
                        case -1: switch (errno)
                        {
                            case EINTR: goto _gW;
                            default: break;
                        } _EXIT("%d: %s", __LINE__, strerror(errno));
                        default: break;
                    }
                } break;

                default: _EXIT("%d: %s", __LINE__, strerror(errno));
            }

            /* */
            {
                utime_t timeNow = utimeNow(NULL);

                /* 하위 프로세서의 변경을 확인한다. */
                if (__iSignalCHLD > 0)
                {
                    utime_t timeNow = utimeNow(NULL);
                    std::deque<PROCESS *> ready;

#define TIME_BREAK          1000
                    for (PROCESS_idSet::iterator p_it = preforks.begin(); p_it != preforks.end(); )
                        if ((p_it->first == 0) || (kill(p_it->first, 0) == 0))
                            ++p_it;
                        else {
                            if ((timeNow - p_it->second->timeStart) > TIME_BREAK)
                                ready.push_back(p_it->second);
                            else {
                                _WARN("TIME BREAK: %d", p_it->first);
                                p_it->second->Unref();
                            }

                            p_it = preforks.erase(p_it);
                        }
#undef TIME_BREAK
                    if (preforks.size() > 1)
                        for (--__iSignalCHLD;ready.empty() == false; ready.pop_front())
                        {
                            X__F.f = ready.front();

                            X__F.f->timeStart = timeNow; {
                                pid_t pid = fork();

                                if (pid < 0) _EXIT("%d: %s", __LINE__, strerror(errno));
                                else if (pid > 0)
                                    preforks.insert(PROCESS_idSet::value_type(pid, X__F.f));
                                else {
                                    _INFO("RESTART STAGE - %d", X__F.f->stage_id);
                                    if (X__F.f->stage_id == 0)
                                        PROCESS_initStage(X__F.f, NULL);

                                    longjmp(X__F.jmp, 1);
                                }
                            }
                        }
                    else _EXIT("CRITICAL BREAK");
                }

#define FAIL_TIMEOUT            3100
                /* 정보 동기화가 3초 이상 되지 않은 경우, 프로세서를 강제로 재 시작 시킨다. */
                for (STAGE_usageSet::iterator m_it = S__usage.begin(); m_it != S__usage.end();)
                {
                    STAGE_usageIn *u = &(m_it->second);

                    if ((int64_t)(timeNow - u->back().timeUpdate) < FAIL_TIMEOUT);
                    else if (kill(u->back().pid, SIGKILL) == 0)
                        _WARN("USAGE %d: %d - BREAK [%ld msec]",
                              u->back().pid, m_it->first, (timeNow - u->back().timeUpdate))
                    else {
                        _WARN("USAGE %d: %d - RESET", u->back().pid, m_it->first)
                        {
                            m_it = S__usage.erase(m_it);
                        } continue;
                    } ++m_it;
                }
#undef FAIL_TIMEOUT
            } goto _gW;
#undef IDLE_TIMEOUT
            default: _WARN("IGNORE SIGNAL")
        }
    } return 0;
}
