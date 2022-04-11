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

#include <math.h>
#include "LcevcMd5.hpp"

#if BD_CF_EXT
#define TX_SHIFT1(log2_size, bd) ((log2_size)-1 + bd - 8)
#else
#define TX_SHIFT1(log2_size) ((log2_size)-1 + BIT_DEPTH - 8)
#endif
#define TX_SHIFT2(log2_size) ((log2_size) + 6)

/* MD5 functions */
#define MD5FUNC(f, w, x, y, z, msg1, s, msg2) (w += f(x, y, z) + msg1 + msg2, w = w << s | w >> (32 - s), w += x)
#define FF(x, y, z) (z ^ (x & (y ^ z)))
#define GG(x, y, z) (y ^ (z & (x ^ y)))
#define HH(x, y, z) (x ^ y ^ z)
#define II(x, y, z) (y ^ (x | ~z))

////////////////////////////////////////////////////////////////////////////////

static void lcevc_md5_trans(u32 *buf, u32 *msg) {
	register u32 a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5FUNC(FF, a, b, c, d, msg[0], 7, 0xd76aa478);  /* 1 */
	MD5FUNC(FF, d, a, b, c, msg[1], 12, 0xe8c7b756); /* 2 */
	MD5FUNC(FF, c, d, a, b, msg[2], 17, 0x242070db); /* 3 */
	MD5FUNC(FF, b, c, d, a, msg[3], 22, 0xc1bdceee); /* 4 */

	MD5FUNC(FF, a, b, c, d, msg[4], 7, 0xf57c0faf);  /* 5 */
	MD5FUNC(FF, d, a, b, c, msg[5], 12, 0x4787c62a); /* 6 */
	MD5FUNC(FF, c, d, a, b, msg[6], 17, 0xa8304613); /* 7 */
	MD5FUNC(FF, b, c, d, a, msg[7], 22, 0xfd469501); /* 8 */

	MD5FUNC(FF, a, b, c, d, msg[8], 7, 0x698098d8);   /* 9 */
	MD5FUNC(FF, d, a, b, c, msg[9], 12, 0x8b44f7af);  /* 10 */
	MD5FUNC(FF, c, d, a, b, msg[10], 17, 0xffff5bb1); /* 11 */
	MD5FUNC(FF, b, c, d, a, msg[11], 22, 0x895cd7be); /* 12 */

	MD5FUNC(FF, a, b, c, d, msg[12], 7, 0x6b901122);  /* 13 */
	MD5FUNC(FF, d, a, b, c, msg[13], 12, 0xfd987193); /* 14 */
	MD5FUNC(FF, c, d, a, b, msg[14], 17, 0xa679438e); /* 15 */
	MD5FUNC(FF, b, c, d, a, msg[15], 22, 0x49b40821); /* 16 */

	/* Round 2 */
	MD5FUNC(GG, a, b, c, d, msg[1], 5, 0xf61e2562);   /* 17 */
	MD5FUNC(GG, d, a, b, c, msg[6], 9, 0xc040b340);   /* 18 */
	MD5FUNC(GG, c, d, a, b, msg[11], 14, 0x265e5a51); /* 19 */
	MD5FUNC(GG, b, c, d, a, msg[0], 20, 0xe9b6c7aa);  /* 20 */

	MD5FUNC(GG, a, b, c, d, msg[5], 5, 0xd62f105d);   /* 21 */
	MD5FUNC(GG, d, a, b, c, msg[10], 9, 0x2441453);   /* 22 */
	MD5FUNC(GG, c, d, a, b, msg[15], 14, 0xd8a1e681); /* 23 */
	MD5FUNC(GG, b, c, d, a, msg[4], 20, 0xe7d3fbc8);  /* 24 */

	MD5FUNC(GG, a, b, c, d, msg[9], 5, 0x21e1cde6);  /* 25 */
	MD5FUNC(GG, d, a, b, c, msg[14], 9, 0xc33707d6); /* 26 */
	MD5FUNC(GG, c, d, a, b, msg[3], 14, 0xf4d50d87); /* 27 */
	MD5FUNC(GG, b, c, d, a, msg[8], 20, 0x455a14ed); /* 28 */

	MD5FUNC(GG, a, b, c, d, msg[13], 5, 0xa9e3e905);  /* 29 */
	MD5FUNC(GG, d, a, b, c, msg[2], 9, 0xfcefa3f8);   /* 30 */
	MD5FUNC(GG, c, d, a, b, msg[7], 14, 0x676f02d9);  /* 31 */
	MD5FUNC(GG, b, c, d, a, msg[12], 20, 0x8d2a4c8a); /* 32 */

	/* Round 3 */
	MD5FUNC(HH, a, b, c, d, msg[5], 4, 0xfffa3942);   /* 33 */
	MD5FUNC(HH, d, a, b, c, msg[8], 11, 0x8771f681);  /* 34 */
	MD5FUNC(HH, c, d, a, b, msg[11], 16, 0x6d9d6122); /* 35 */
	MD5FUNC(HH, b, c, d, a, msg[14], 23, 0xfde5380c); /* 36 */

	MD5FUNC(HH, a, b, c, d, msg[1], 4, 0xa4beea44);   /* 37 */
	MD5FUNC(HH, d, a, b, c, msg[4], 11, 0x4bdecfa9);  /* 38 */
	MD5FUNC(HH, c, d, a, b, msg[7], 16, 0xf6bb4b60);  /* 39 */
	MD5FUNC(HH, b, c, d, a, msg[10], 23, 0xbebfbc70); /* 40 */

	MD5FUNC(HH, a, b, c, d, msg[13], 4, 0x289b7ec6); /* 41 */
	MD5FUNC(HH, d, a, b, c, msg[0], 11, 0xeaa127fa); /* 42 */
	MD5FUNC(HH, c, d, a, b, msg[3], 16, 0xd4ef3085); /* 43 */
	MD5FUNC(HH, b, c, d, a, msg[6], 23, 0x4881d05);  /* 44 */

	MD5FUNC(HH, a, b, c, d, msg[9], 4, 0xd9d4d039);   /* 45 */
	MD5FUNC(HH, d, a, b, c, msg[12], 11, 0xe6db99e5); /* 46 */
	MD5FUNC(HH, c, d, a, b, msg[15], 16, 0x1fa27cf8); /* 47 */
	MD5FUNC(HH, b, c, d, a, msg[2], 23, 0xc4ac5665);  /* 48 */

	/* Round 4 */
	MD5FUNC(II, a, b, c, d, msg[0], 6, 0xf4292244);   /* 49 */
	MD5FUNC(II, d, a, b, c, msg[7], 10, 0x432aff97);  /* 50 */
	MD5FUNC(II, c, d, a, b, msg[14], 15, 0xab9423a7); /* 51 */
	MD5FUNC(II, b, c, d, a, msg[5], 21, 0xfc93a039);  /* 52 */

	MD5FUNC(II, a, b, c, d, msg[12], 6, 0x655b59c3);  /* 53 */
	MD5FUNC(II, d, a, b, c, msg[3], 10, 0x8f0ccc92);  /* 54 */
	MD5FUNC(II, c, d, a, b, msg[10], 15, 0xffeff47d); /* 55 */
	MD5FUNC(II, b, c, d, a, msg[1], 21, 0x85845dd1);  /* 56 */

	MD5FUNC(II, a, b, c, d, msg[8], 6, 0x6fa87e4f);   /* 57 */
	MD5FUNC(II, d, a, b, c, msg[15], 10, 0xfe2ce6e0); /* 58 */
	MD5FUNC(II, c, d, a, b, msg[6], 15, 0xa3014314);  /* 59 */
	MD5FUNC(II, b, c, d, a, msg[13], 21, 0x4e0811a1); /* 60 */

	MD5FUNC(II, a, b, c, d, msg[4], 6, 0xf7537e82);   /* 61 */
	MD5FUNC(II, d, a, b, c, msg[11], 10, 0xbd3af235); /* 62 */
	MD5FUNC(II, c, d, a, b, msg[2], 15, 0x2ad7d2bb);  /* 63 */
	MD5FUNC(II, b, c, d, a, msg[9], 21, 0xeb86d391);  /* 64 */

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

////////////////////////////////////////////////////////////////////////////////

void lcevc_md5_init(LCEVC_MD5 *md5) {
	md5->h[0] = 0x67452301;
	md5->h[1] = 0xefcdab89;
	md5->h[2] = 0x98badcfe;
	md5->h[3] = 0x10325476;

	md5->bits[0] = 0;
	md5->bits[1] = 0;
}

////////////////////////////////////////////////////////////////////////////////

void lcevc_md5_update(LCEVC_MD5 *md5, void *buf_t, u32 len) {
	u8 *buf;
	u32 i, idx, part_len;

	buf = (u8 *)buf_t;

	idx = (u32)((md5->bits[0] >> 3) & 0x3f);

	md5->bits[0] += (len << 3);
	if (md5->bits[0] < (len << 3)) {
		(md5->bits[1])++;
	}

	md5->bits[1] += (len >> 29);
	part_len = 64 - idx;

	if (len >= part_len) {
		lcevc_mcpy(md5->msg + idx, buf, part_len);
		lcevc_md5_trans(md5->h, (u32 *)md5->msg);

		for (i = part_len; i + 63 < len; i += 64) {
			lcevc_md5_trans(md5->h, (u32 *)(buf + i));
		}
		idx = 0;
	} else {
		i = 0;
	}

	if (len - i > 0) {
		lcevc_mcpy(md5->msg + idx, buf + i, len - i);
	}
}

////////////////////////////////////////////////////////////////////////////////

void lcevc_md5_update_16(LCEVC_MD5 *md5, void *buf_t, u32 len) {
	u16 *buf;
	u32 i, idx, part_len, j;
	u8 t[512];

	buf = (u16 *)buf_t;
	idx = (u32)((md5->bits[0] >> 3) & 0x3f);

	len = len * 2;
	for (j = 0; j < len; j += 2) {
		t[j] = (u8)(*(buf));
		t[j + 1] = *(buf) >> 8;
		buf++;
	}

	md5->bits[0] += (len << 3);
	if (md5->bits[0] < (len << 3)) {
		(md5->bits[1])++;
	}

	md5->bits[1] += (len >> 29);
	part_len = 64 - idx;

	if (len >= part_len) {
		lcevc_mcpy(md5->msg + idx, t, part_len);
		lcevc_md5_trans(md5->h, (u32 *)md5->msg);

		for (i = part_len; i + 63 < len; i += 64) {
			lcevc_md5_trans(md5->h, (u32 *)(t + i));
		}
		idx = 0;
	} else {
		i = 0;
	}

	if (len - i > 0) {
		lcevc_mcpy(md5->msg + idx, t + i, len - i);
	}
}

////////////////////////////////////////////////////////////////////////////////

void lcevc_md5_finish(LCEVC_MD5 *md5, u8 digest[16]) {
	u8 *pos;
	int cnt;

	cnt = (md5->bits[0] >> 3) & 0x3F;
	pos = md5->msg + cnt;
	*pos++ = 0x80;
	cnt = 64 - 1 - cnt;

	if (cnt < 8) {
		lcevc_mset(pos, 0, cnt);
		lcevc_md5_trans(md5->h, (u32 *)md5->msg);
		lcevc_mset(md5->msg, 0, 56);
	} else {
		lcevc_mset(pos, 0, cnt - 8);
	}

	lcevc_mcpy((md5->msg + 14 * sizeof(u32)), &md5->bits[0], sizeof(u32));
	lcevc_mcpy((md5->msg + 15 * sizeof(u32)), &md5->bits[1], sizeof(u32));

	lcevc_md5_trans(md5->h, (u32 *)md5->msg);
	lcevc_mcpy(digest, md5->h, 16);
	lcevc_mset(md5, 0, sizeof(LCEVC_MD5));
}

////////////////////////////////////////////////////////////////////////////////

// int np; number of planes
// int w[LCEVC_IMGB_MAX_PLANE]; // width in pixels
// int h[LCEVC_IMGB_MAX_PLANE]; // heoght in pixels
// int x[LCEVC_IMGB_MAX_PLANE]; // x position of top left pixel
// int y[LCEVC_IMGB_MAX_PLANE]; // y position of top left pixel
// int s[LCEVC_IMGB_MAX_PLANE]; // stride in bytes
// int e[LCEVC_IMGB_MAX_PLANE]; // elevation in bytes
// void *a[LCEVC_IMGB_MAX_PLANE]; // data address

/*
p planes
y position
x position
h height
w width
s stride
a address
*/

#include <stdio.h>

int lcevc_md5_imgb(LCEVC_IMGB *imgb, u8 digest[N_C][16]) {
	LCEVC_MD5 md5[N_C];
	int i, j;

	for (i = 0; i < imgb->np; i++) {
		lcevc_md5_init(&md5[i]);

		// fprintf(stdout, "p %4d - h %4d - w %4d - s %4d \n", i, imgb->h[i], imgb->w[i], imgb->s[i]); fflush(stdout);

		for (j = imgb->y[i]; j < imgb->h[i]; j++) {
			lcevc_md5_update(&md5[i], ((u8 *)imgb->a[i]) + j * imgb->s[i] + imgb->x[i], imgb->w[i] * 2);
		}

		lcevc_md5_finish(&md5[i], digest[i]);
	}

	return LCEVC_OK;
}

////////////////////////////////////////////////////////////////////////////////
