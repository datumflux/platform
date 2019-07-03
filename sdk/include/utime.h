/* COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * ALL RIGHT TO SOURCE CODE ARE IN DATUMFLUX CORP.
 *
 * \brief msec기반의 시간을 처리하기 위한 time_t 대응 함수
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#ifndef __UTIME_H
#  define __UTIME_H

#include "typedef.h"
#include <sys/time.h>

/*! \addtogroup core_utime
 *  @{
 */
typedef uint64_t utime_t;


/* utime: timeNow를 1/1000 sec 로 변환 */
EXTRN utime_t utime( struct timeval timeNow);

/* utimeNow: 현재 시간에 대한 1/1000 sec를 반환한다. */
EXTRN utime_t utimeNow( struct timeval *);

/* utimeDiff: timeEnd시간과 timeStart시간의 차이를 반환 (1/1000 sec) */
EXTRN utime_t utimeDiff( struct timeval timeStart, struct timeval timeEnd);

/* utimeTick: 현재 시간과 timeStart를 비교하여 차이를 반환 (1/1000 sec) */
EXTRN utime_t utimeTick( struct timeval timeStart);

/* utimeSleep: timeSleep 만큼 대기 한다. (1/1000 sec) */
EXTRN int      utimeSleep( int timeSleep);

/* utimeSpec: timeNow + msec = ts */
EXTRN struct timespec *utimeSpec(struct timeval *timeNow, int msec, struct timespec *ts);

/* @} */
#endif
