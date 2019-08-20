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
#ifndef __WELL512_H
#  define __WELL512_H

#include "typedef.h"

#define WELL512_MAX             16

struct _well512 {
    unsigned long state[WELL512_MAX];
    unsigned int  index;
};


EXTRN void well512_srand(struct _well512 *r, unsigned long seed, long delta);

EXTRN unsigned long well512_rand(struct _well512 *r);
EXTRN unsigned long well512_range(struct _well512 *r, unsigned long l, unsigned long m);
EXTRN unsigned long well512_max(struct _well512 *r, unsigned long m);

EXTRN float well512_randf(struct _well512 *r);

#endif
