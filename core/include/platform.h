/** @file platform.h */

/**    Copyright 2009-2011 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


/* all platform-specific ifdefs should go here */

#ifndef _PLATFORM_HACKS_H_
#define _PLATFORM_HACKS_H_

#ifdef __GNUC__
#define MONGO_INLINE static __inline__
#else
#define MONGO_INLINE static
#endif

#ifdef __cplusplus
#define MONGO_EXTERN_C_START extern "C" {
#define MONGO_EXTERN_C_END }
#else
#define MONGO_EXTERN_C_START
#define MONGO_EXTERN_C_END
#endif


#include <stdint.h>
#include <endian.h>

/* big endian is only used for OID generation. little is used everywhere else */
//#if __BYTE_ORDER == __BIG_ENDIAN
#if __BYTE_ORDER != __LITTLE_ENDIAN
#define bson_little_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_little_endian32(out, in) ( bson_swap_endian32(out, in) )
#define bson_big_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_big_endian32(out, in) ( memcpy(out, in, 4) )
#else
#define bson_little_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_little_endian32(out, in) ( memcpy(out, in, 4) )
#define bson_big_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_big_endian32(out, in) ( bson_swap_endian32(out, in) )
#endif

MONGO_EXTERN_C_START

MONGO_INLINE void bson_swap_endian64( void *outp, const void *inp ) {
    const char *in = ( const char * )inp;
    char *out = ( char * )outp;

    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];

}
MONGO_INLINE void bson_swap_endian32( void *outp, const void *inp ) {
    const char *in = ( const char * )inp;
    char *out = ( char * )outp;

    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];
}

MONGO_EXTERN_C_END

#endif
