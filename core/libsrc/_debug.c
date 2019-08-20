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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include "debug.h"


/*! \defgroup core_debug 디버깅을 위한 함수
 *  @{
 */
#ifndef NDEBUG

#pragma GCC diagnostic ignored "-Wunused-result"

#  undef __assert_fail
void 
__assert_fail( const char *assertion, const char *file, unsigned int line,
		       const char *function)
{
	char *buf;


	if (__asprintf(&buf, "%s%s%s%s%u: %s%s%s\n    Assertion '%s' failed.\n",
				__progname, __progname[0] ? ": " : "",
				file ?:"", file ? ":": "", line, 
				function ?"[":"", function ?: "", function ?"]": "",
				assertion) >= 0)
	{
		fputs( buf, stderr); {
			fflush( stderr);
		} free( buf);
	}
	else {
		static const char errstr[] = "Unexpected error.\n";
		write( fileno(stderr), errstr, sizeof(errstr) - 1);
	} abort();
}


const char *__lprintf( const char *format, ...)
{
#define MAX_LOGSIZE					BUFSIZ
	static char tmp[MAX_LOGSIZE]; {
		va_list args;

		va_start( args, format); {
			assert( vsnprintf( tmp, sizeof(tmp) - 1, format, args) > 0);
		} va_end( args);
	} return tmp;
}


#include <ctype.h>

void dumpBuffer( const char *buf, int len)
{
#define DUMP_ASCII					16
#define DUMP_HALF					8
	static const char BUFFER_BLANKS[] = "       ";
	static       char tmp[DUMP_ASCII + 1];



	fprintf( stderr, "%s0000: ", BUFFER_BLANKS); {
		register int  nofs = 0;
		unsigned char c;


_gWhile:c = *(buf + nofs); {
			tmp[ nofs++ % DUMP_ASCII] = isprint( c) ? c: '.';
		} fprintf( stderr, "%02X ", c);
		if ((nofs % DUMP_ASCII) == 0)
		{
			fprintf( stderr, "  %s\n", tmp);
			if (nofs < len)
				fprintf( stderr, "%s%04X: ", BUFFER_BLANKS, nofs);
			else {
_gBreak:		if ((nofs % DUMP_ASCII) == 0)
					;
				else {
					tmp[nofs % DUMP_ASCII] = 0; {
						if ((nofs % DUMP_ASCII) <= DUMP_HALF)
							fprintf( stderr, " ");
					} while ((nofs++ % DUMP_ASCII)) fprintf( stderr, "   ");
					fprintf( stderr, "  %s\n", tmp);
				} return;
			} goto _gWhile;
		} if (nofs >= len) goto _gBreak;
		if ((nofs % DUMP_HALF) == 0) fprintf( stderr, " ");
		goto _gWhile;
	}
}
#endif

#include <execinfo.h>

void debugTrace( int ndepth)
{
	void  *array[ndepth];
	int    size = backtrace( array, ndepth);
	char **symbols = backtrace_symbols( array, size);

	while ((--size) >= 0)
		fprintf( stderr, " %2.i: %s\n", size + 1, symbols[size]);
}

/* @} */