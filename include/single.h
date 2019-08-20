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
#ifndef __SINGLE_H
#  define __SINGLE_H
/*! COPYRIGHT 2018-2019 DATUMFLUX CORP.
 */
#include <sys/socket.h>
#include "utime.h"
#include "bson.h"

#include "c++/string_format.hpp"
#include <json/json.h>          /* jsoncpp */
#include <log4cxx/logger.h>     /* log4cxx */

/*! \addtogroup stage
 *  @{
 */
#define MAX_STAGE_LEN               40

struct sockaddr_stage : public sockaddr_storage
{
    socklen_t ss_length;
    char      ss_stage[MAX_STAGE_LEN];
};

struct bson_scope: public bson {
    bson_scope() { bson_init((bson *)this); }
    virtual ~bson_scope() { bson_destroy((bson *)this);}
};
/*!
 * \brief stage이 callback 함수
 *
 * \param timeBreak     메시지의 실행이 처리되어야 하는 제한 시간
 * \param it            메시지 데이터
 * \param in_addr       결과에 대한 반환 주소
 * \param udata         stage_addtask() 로 설정된 udata 또는 EXPORT_STAGE()에서 설정된 udata
 *
 * \details
 *
 *        action  | timeBreak |   in  | in_addr |      return
 *      ----------+-----------+-------+---------+-------------------
 *         idle   |     X     |   X   |    O    | next time (msec)
 *       message  |     O     |   O   |    O    |     -
 *       destroy  |     X     |   X   |    X    |     -
 */
typedef int (*stage_callback)(utime_t /* timeBreak */,
        bson_iterator * /* in */, struct sockaddr_stage * /* in_addr */,  void **udata);

#define EXPORT_STAGE(_STAGEID_)    	\
extern "C" int __##_STAGEID_##__stage__(const char *stage_id, struct sockaddr_stage *p__addr, \
		/* Json::Value */void *j__config, stage_callback *r__callback, void **udata)

#if 0
int (*XXXXX__ready)(struct sockaddr *, socklen_t, void * /* Json::Value * */, stage_callback * /* callback */);
int (*main)(int, char **, struct sockaddr *, socklen_t, void *);
#endif

#define DECLARE_LOGGER(_LOGID_) static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger(_LOGID_))

#define _EXIT(...)				do { \
	LOG4CXX_FATAL(logger, std::format(__VA_ARGS__).c_str()); exit(0); \
} while (0)
#define _INFO(...)				LOG4CXX_INFO(logger, std::format(__VA_ARGS__).c_str())
#define _WARN(...)				LOG4CXX_WARN(logger, std::format(__VA_ARGS__).c_str())
#define _DEBUG(...)				LOG4CXX_DEBUG(logger, std::format(__VA_ARGS__).c_str())
#define _TRACE(...)				LOG4CXX_TRACE(logger, std::format(__VA_ARGS__).c_str())

/*!
 * \brief socktype으로 처리할수 있는 서버의 주소를 얻는다.
 *
 * \param socktype          처리가 필요한 소켓의 형태 (socket()의 설정 타입과 동일)
 *                          socktype == SOCK_STREAM     스트림 형태의 소켓
 *                          socktype == SOCK_DGRAM      데이터그램 형태의 소캣
 *
 * \param host_or_ip        IPv4 또는 IPv6 의 주소
 * \param port              포트번호
 * \param flags             flag (ai_flags)
 *
 * \return     freeaddrinfo()로 해제할수 있는 서버의 주소
 */
EXTRN struct addrinfo *af_addrinfo(int socktype, const char *host_or_ip, const char *port, int flags);

EXTRN int af_socket(struct addrinfo *entry, struct addrinfo **a__i);

/* */
EXTRN int stage_id();
EXTRN int stage_args(const char ***args);


/*!
 * \brief api 연결 정보를 얻는디
 *
 * \param name          설정된 이름
 * \param arg           stage_setapi()에 연결된 x_alloc(arg, ..)으로 전달되는 값
 *
 * \return              x_alloc()에서 반환되는 값
 */
EXTRN void *stage_api(const char *name, uintptr_t arg);

/*!
 * \brief api 연결에 필요한 처리를 수행하는 함수를 등록 관리한다.
 *
 * \param name          설정하는 이름
 * \param x_alloc       연결 정보를 반환하는 함수 (연결정 정보 삭제시 null)
 * \param udata         연결 정보는 개발자 데이터
 *
 * \details
 *    stage로 연결되는 라이브러리가 서로 공유되어야 하는 정보가 있다면 해당 함수를 사용해 공유할수 있다.
 *
 *    이러한 형태로 사용하는 것은 stage의 모든 정보가 동적라이브러로 사용될수 있기 때문이며
 *    잘못된 접근으로 인한 오류를 방지하기 위함이다.
 */
EXTRN int  stage_setapi(const char *name,
        void *(*x_alloc)(uintptr_t arg, uintptr_t *), uintptr_t udata);


/*!
 * \brief 쓰레드풀에서 일정 시간 대기한다.
 *
 * \param timeout         이벤트를 기다릴 시간 (msec)
 */
EXTRN int stage_dispatch(int timeout);

/*!
 * \brief 쓰레드풀을 통해 함수를 실행한다.
 *
 * \param submit_id         이벤트의 고유 번호(동일한 이벤트 번호는 순차적으로 실행을 보장한다.)
 *                          submit_id = 0     실행 제한 없음
 *                          submit_id != 0    동일한 고유번호의 실행이 순차적으로 된다.
 *
 * \param executor          실행 할 함수
 *        int (*executor)(utime_t timeLast, void *udata);
 *
 *        timeLast      submit_id != 0 아 아닌 경우에만 마지막 실행된 시간이 설정된다.
 *        return > 0
 *             timeLast + return 이후에 다시 실행하도록 처리
 * \param udata             submit_id 실행시, 전달된 사용자 값
 *
 */
EXTRN int  stage_submit(uintptr_t submit_id,
        int (*execute)(utime_t, void */* udata */), void *udata);

/*!
 * \brief 실행 대기 중인 이벤트를 제거한다.
 *
 * \param submit_id       제거할 이벤트 고유 번호
 */
EXTRN int  stage_cancel(uintptr_t submit_id);

/*!
 * \brief 함수를 등록한다.
 *
 * \param timeStart		callback 함수가 시작할 시간
 *                      timeStart < 0   네트워크로 부터 발생된 메시지도 같이 처리한다.
 *                      timeStart = 0	네트워크로 부터 발생된 데이터만 처리한다.
 *                      timeStart > 0	timeStart 시간에 callback 함수를 호출한다.
 *
 * \param callback		처리할 callback 함수
 * 						callback() 함수가 < 0 을 반환하면 설정된 정보는 제거한다.
 * \param udata			callback 함수의 추가 정보
 *
 * \example
 *
 *    stage_addtask(utimeNow(NULL) + 1000, [](utime_t timeBreak,
 *              bson_iterator *x__it, struct sockaddr_stage *x__addr, void **udata) {
 *         return 1000;
 *    }, NULL);
 */
EXTRN void stage_addtask(utime_t timeStart, stage_callback callback, void *udata);

/*!
 * \brief 메시지를 보낸다.
 *
 * \param feedback      회신이 필요한 데이터 여부
 *                      feedback = 1    회신이 필요한 데이터
 *                      feedback = 0    회신을 필요로 하지 않은 데이터 또는 반환 값
 *
 * \param stage_id      data를 받을 stage_id ("TARGET+SELF")
 * \param data          보낼 데이터 (bson_finish() 처리된 data)
 * \param timeBreak     처리 제한 시간
 * \param addr          메시지를 받을 서버의 주소 (NULL인 경우 상위 라우팅 처리)
 *
 * \details
 *    stage_id에 설정된 target은 로드벨런싱 되어 전달이 됩니다. 만약, stage_id에 모두 전달이 필요한 경우에는
 *    '*'를 추가하여 호출하면 로드밸런싱을 무시하고 모두에게 전달됩니다. (EX. "*lua+example")
 *
 *    만약, EXPORT_STAGE() 에 연결된 messageCallback() 에서 처리 결과를 반환하는 경우에는
 *    messageCallback()의 입력으로 전달된 addr를 설정하고, stage_id == NULL 로 설정해 전달하면 됩니다.
 *
 * \example
 *    bson_scope b_o;
 *
 *    bson_append_string(&b_o, "hello", "Stage");
 *    bson_finish(&b_o);
 *
 *    stage_signal(1, "lua+example", &b_o, 0, NULL);
 */
EXTRN int  stage_signal(int feedback,
        const char *stage_id, bson *data, utime_t timeBreak, struct sockaddr_stage *addr);

/*!
 * \brief 내부에 활성화된 stage로 메시지를 보낸다.
 *
 * \param i             전달하고자 하는 메시지
 *
 * \details
 *      stage_signal()은 상위 프로세서를 통해 전달되지만, stage_call()은 프로세서 내로 제한 된다.
 */
EXTRN int  stage_call(bson_iterator *i);

/*!
 * \brief 프로세서의 이름을 변경한다.
 *
 * \param format        변경할 문자열 포맷 (printf()와 사용법 동일)
 */
EXTRN void stage_setproctitle( const char *format, ...);

/* @} */
#endif