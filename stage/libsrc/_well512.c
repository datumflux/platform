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
#include "include/well512.h"


void well512_srand(struct _well512 *r, unsigned long seed, long delta)
{
    r->index = 0;
    for (int i = 0; i < WELL512_MAX; ++i)
    {
        r->state[i] = seed;
        seed += (seed + delta);
    }
}

/* 
 * http://blog.naver.com/sorkelf/40188049396
 * http://blog.daum.net/windria/51
 * http://stackoverflow.com/questions/1046714/what-is-a-good-random-number-generator-for-a-game
 */
unsigned long well512_rand(struct _well512 *r)
{
    unsigned long a, b, c, d;

    a = r->state[r->index];
    c = r->state[(r->index + 13) & 15];
    b = a ^ c ^ (a << 16) ^ (c << 15);
    c = r->state[(r->index + 9) & 15];
    c ^= (c >> 11);
    a = r->state[r->index] = b ^ c;
    d = a ^ ((a << 5) & 0xDA442D24UL);
    r->index = (r->index + 15) & 15;
    a = r->state[r->index];
    r->state[r->index] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);

    return r->state[r->index];
}

unsigned long well512_range(struct _well512 *r, unsigned long l, unsigned long m) {
    return l + (well512_rand(r) % (m - l));
}

unsigned long well512_max(struct _well512 *r, unsigned long m) {
    return well512_rand(r) % m;
}

float well512_randf(struct _well512 *r)
{
    union {
        unsigned long ul;
        float f;
    } p;
    unsigned long v = well512_rand(r);

    p.ul = (((v *= 16807) & 0x007fffff) - 1) | 0x3f800000;
    return p.f - 1.f;
}
