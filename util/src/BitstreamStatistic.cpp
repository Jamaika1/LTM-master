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
// BitstreamStatistic.cpp
//
//

#include "BitstreamStatistic.hpp"

#include "Diagnostics.hpp"

int BitstreamStatistic::Reset() {
	for (int i = 0; i < STATISTIC_CATEGORIES; i++) {
		strcpy(mcCategory[i], "");
		miCounter[i] = 0;
		miBits[i] = 0;
	}
	strcpy(mcCategory[STAT_DATA_SIZE], "data_size");
	strcpy(mcCategory[STAT_ENTROPY_ENABLED], "entropy_enab");
	strcpy(mcCategory[STAT_RLE_ONLY], "rle_only");
	strcpy(mcCategory[STAT_ENTROPY_CODE], "entropy_code");
	strcpy(mcCategory[STAT_ENTROPY_SYMBOL], "entropy_symb");
	return 0;
}

int BitstreamStatistic::Update(char *pcCategory, int iSize) {
	if (strstr(pcCategory, "data_size") != NULL) {
		miCounter[STAT_DATA_SIZE]++;
		miBits[STAT_DATA_SIZE] += iSize;
	} else if (strstr(pcCategory, "entropy_enab") != NULL) {
		miCounter[STAT_ENTROPY_ENABLED]++;
		miBits[STAT_ENTROPY_ENABLED] += iSize;
	} else if (strstr(pcCategory, "rle_only") != NULL) {
		miCounter[STAT_RLE_ONLY]++;
		miBits[STAT_RLE_ONLY] += iSize;
	} else if (strstr(pcCategory, "entropy_code") != NULL) {
		miCounter[STAT_ENTROPY_CODE]++;
		miBits[STAT_ENTROPY_CODE] += iSize;
	} else if (strstr(pcCategory, "entropy_symb") != NULL) {
		miCounter[STAT_ENTROPY_SYMBOL]++;
		miBits[STAT_ENTROPY_SYMBOL] += iSize;
	}
	return 0;
}

int BitstreamStatistic::Dump() {
	for (int i = 0; i < STATISTIC_CATEGORIES; i++) {
		printf("Stat - %-32s - %8d - %8d \n", mcCategory[i], miCounter[i], miBits[i]);
	}
	fflush(stdout);
	return 0;
}

int PicturePsnr15bpp(int16_t *pIn, int16_t *pOut, unsigned plane, int iWidth, int iHeight, PsnrStatistic &oPsnr) {
	long long llMse = 0;
	for (int i = 0; i < iHeight; i++) {
		for (int j = 0; j < iWidth; j++) {
			int y = (int)pIn[i * iWidth + j] - (int)pOut[i * iWidth + j];
			llMse += (y * y);
		}
	}
	oPsnr.mfCurMse[plane] = (float)(llMse) / (float)(iWidth * iHeight);

	oPsnr.mfCurPsnr[plane] = (float)(10.0f * log10((32767.0f * 32767.0f) / oPsnr.mfCurMse[plane]));
	oPsnr.mfAccMse[plane] += oPsnr.mfCurMse[plane];
	oPsnr.mfAccPsnr[plane] += oPsnr.mfCurMse[plane];

	// REPORT("psnrY %8.4f -- psnrU %8.4f -- psnrV %8.4f", (oPsnr.mfCurPsnrY), (oPsnr.mfCurPsnrU), (oPsnr.mfCurPsnrV));
	// REPORT("mseY  %8.4f -- mseU  %8.4f -- mseV  %8.4f", (oPsnr.mfAccMseY), (oPsnr.mfAccMseU), (oPsnr.mfAccMseV));

	return 0;
}
