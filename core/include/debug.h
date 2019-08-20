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
#ifndef __DEBUG_H
#  define __DEBUG_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief 디버깅을 위한 함수
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <stdarg.h>
#include <assert.h>

/*! \addtogroup core_debug
 *  @{
 */
#ifndef EXTRN
#  ifdef __cplusplus
#    define EXTRN					extern "C"
#  else
#    define EXTRN					extern
#  endif
#endif	/* EXTRN */

EXTRN const char *__progname;

#  ifdef NDEBUG
#    define assertf(expr, args...)		(__ASSERT_VOID_CAST(0))
#    define assertl(args...)			(__ASSERT_VOID_CAST(0))
#    define dumpBuffer( buf, size)		(0)
#  else
EXTRN const char *__lprintf( const char *format, ...);

#    if __GNUC_PREREQ( 3, 0)
#       define assertf( expr, args...)	\
			(__ASSERT_VOID_CAST( __builtin_expect( !!(expr), 1) ? 0:	\
				(__assert_fail( __lprintf( args), __FILE__, __LINE__, 	\
						__ASSERT_FUNCTION), 0)))
#    else
#       define assertf( expr, args...)	\
			(__ASSERT_VOID_CAST( ((expr) ? 0:			\
				(__assert_fail( __lprintf( args), __FILE__, __LINE__, 	\
						__ASSERT_FUNCTION), 0)))
#    endif

#    define assertl( args...)	\
	    __assert_fail( __lprintf( args), __FILE__, __LINE__, __ASSERT_FUNCTION)
EXTRN void  dumpBuffer( const char *, int);
#endif

EXTRN void debugTrace( int ndepth);
/* @} */
#endif
