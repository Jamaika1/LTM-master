// The copyright in this software is being made available under the BSD
// License, included below. This software may be subject to other third party
// and contributor rights, including patent rights, and no such rights are
// granted under this license.
//
// Copyright (c) 2022, ISO/IEC
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//  * Neither the name of the ISO/IEC nor the names of its contributors may
//    be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//
// BitstreamStatistic.hpp
//
//

#if !defined __BITSTREAM_STATISTIC_HPP__
#define __BITSTREAM_STATISTIC_HPP__

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STATISTIC_CATEGORIES 64

#define STAT_DATA_SIZE 12
#define STAT_ENTROPY_ENABLED 13
#define STAT_RLE_ONLY 14
#define STAT_ENTROPY_CODE 15
#define STAT_ENTROPY_SYMBOL 16

class BitstreamStatistic {
	char mcCategory[STATISTIC_CATEGORIES][64];
	int miCounter[STATISTIC_CATEGORIES];
	int miBits[STATISTIC_CATEGORIES];

public:
	int Reset();
	int Update(char *pcCategory, int iSize);
	int Dump();
};

typedef struct PsnrStatistic {
	// bitrate
	int miBaseBytes;
	int miEnhancementBytes;
	// psnr
	float mfCurMse[3];
	float mfCurPsnr[3];
	float mfAccMse[3];
	float mfAccPsnr[3];
} PsnrStatistic;

int PicturePsnr15bpp(int16_t *pIn, int16_t *pOut, unsigned plane, int iWidth, int iHeight, PsnrStatistic &oPsnr);

typedef struct ReportStructure {
	int miTimeStamp;
	int miPictureType;
	int miBaseSize;
	int miEnhancementSize;
	int miRunTime;
} ReportStructure;

typedef struct ReportStructureComp {
	bool operator()(const ReportStructure &l, const ReportStructure &r) { return (l.miTimeStamp > r.miTimeStamp); }
} ReportStructureComp;

#endif
