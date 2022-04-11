/* The copyright in this software is being made available under the BSD
 *  License, included below. This software may be subject to other third party
 *  and contributor rights, including patent rights, and no such rights are
 *  granted under this license.
 *
 *  Copyright (c) 2019-2021, ISO/IEC
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of the ISO/IEC nor the names of its contributors may
 *     be used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGE.
 */
// Contributors: EVC test model
// Contributors: Stefano Battista(bautz66 @gmail.com)

#ifndef _LCEVC_UTIL_H_
#define _LCEVC_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

////////////////////////////////////////////////////////////////////////////////

// #include "evc_def.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define N_C 3 /* number of color component */

typedef int BOOL;
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

////////////////////////////////////////////////////////////////////////////////

#define lcevc_mset(dst, v, size) memset((dst), (v), (size))
#define lcevc_mcpy(dst, src, size) memcpy((dst), (src), (size))

#define LCEVC_OK (0)

/*! macro to determine maximum */
// #define LCEVC_MAX(a,b)                   (((a) > (b)) ? (a) : (b))

/*! macro to determine minimum */
// #define LCEVC_MIN(a,b)                   (((a) < (b)) ? (a) : (b))

/*! macro to absolute a value */
#define LCEVC_ABS(a)                     abs(a)

/*! macro to absolute a 64-bit value */
#define LCEVC_ABS64(a)                   (((a)^((a)>>63)) - ((a)>>63))

/*! macro to absolute a 32-bit value */
#define LCEVC_ABS32(a)                   (((a)^((a)>>31)) - ((a)>>31))

/*! macro to absolute a 16-bit value */
#define LCEVC_ABS16(a)                   (((a)^((a)>>15)) - ((a)>>15))

/*! macro to clipping within min and max */
#define LCEVC_CLIP3(min_x, max_x, value)   LCEVC_MAX((min_x), LCEVC_MIN((max_x), (value)))

/*! macro to clipping within min and max */
#define LCEVC_CLIP(n,min,max)            (((n)>(max))? (max) : (((n)<(min))? (min) : (n)))

#define LCEVC_SIGN(x)                    (((x) < 0) ? -1 : 1)

/*! macro to get a sign from a 16-bit value.\n
operation: if(val < 0) return 1, else return 0 */
#define LCEVC_SIGN_GET(val)              ((val<0)? 1: 0)

/*! macro to set sign into a value.\n
operation: if(sign == 0) return val, else if(sign == 1) return -val */
#define LCEVC_SIGN_SET(val, sign)        ((sign)? -val : val)

/*! macro to get a sign from a 16-bit value.\n
operation: if(val < 0) return 1, else return 0 */
#define LCEVC_SIGN_GET16(val)            (((val)>>15) & 1)

/*! macro to set sign into a 16-bit value.\n
operation: if(sign == 0) return val, else if(sign == 1) return -val */
#define LCEVC_SIGN_SET16(val, sign)      (((val) ^ ((s16)((sign)<<15)>>15)) + (sign))

#define LCEVC_ALIGN(val, align)          ((((val) + (align) - 1) / (align)) * (align))

#define CONV_LOG2(v)                    (lcevc_tbl_log2[v])

////////////////////////////////////////////////////////////////////////////////

/* MD5 structure */
typedef struct _LCEVC_MD5
{
    u32     h[4]; /* hash state ABCD */
    u8      msg[64]; /*input buffer (nalu message) */
    u32     bits[2]; /* number of bits, modulo 2^64 (lsb first)*/
} LCEVC_MD5;

#define LCEVC_IMGB_MAX_PLANE (4)

typedef struct _LCEVC_IMGB {
	int cs; /* color space */
	int np; /* number of plane */
	/* width (in unit of pixel) */
	int w[LCEVC_IMGB_MAX_PLANE];
	/* height (in unit of pixel) */
	int h[LCEVC_IMGB_MAX_PLANE];
	/* X position of left top (in unit of pixel) */
	int x[LCEVC_IMGB_MAX_PLANE];
	/* Y postion of left top (in unit of pixel) */
	int y[LCEVC_IMGB_MAX_PLANE];
	/* buffer stride (in unit of byte) */
	int s[LCEVC_IMGB_MAX_PLANE];
	/* buffer elevation (in unit of byte) */
	int e[LCEVC_IMGB_MAX_PLANE];
	/* address of each plane */
	void *a[LCEVC_IMGB_MAX_PLANE];
} LCEVC_IMGB;

////////////////////////////////////////////////////////////////////////////////

/* MD5 Functions */
void lcevc_md5_init(LCEVC_MD5 * md5);
void lcevc_md5_update(LCEVC_MD5 * md5, void * buf, u32 len);
void lcevc_md5_update_16(LCEVC_MD5 * md5, void * buf, u32 len);
void lcevc_md5_finish(LCEVC_MD5 * md5, u8 digest[16]);
int lcevc_md5_imgb(LCEVC_IMGB * imgb, u8 digest[N_C][16]);

#ifdef __cplusplus
}
#endif

#endif /* _LCEVC_UTIL_H_ */
