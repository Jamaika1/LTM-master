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

#pragma once

#include <memory>

#include "uTypes.h"
#include "uBaseDecoder.h"

#define __BASE_VVC__ 1

#define VVCMaxSPSCount 16
#define VVCMaxPPSCount 64
#define VVCMaxSubLayersCount 8

namespace vnova {
namespace utility {


struct VVCNalType {
	enum Enum {
		CODED_SLICE_TRAIL = 0,
		CODED_SLICE_STSA = 1,
		CODED_SLICE_RADL = 2,
		CODED_SLICE_RASL= 3,
		RESERVED_VCL_4 = 4,
		RESERVED_VCL_5 = 5,
		RESERVED_VCL_6 = 6,
		CODED_SLICE_IDR_W_RADL = 7,
		CODED_SLICE_IDR_N_LP = 8,
		CODED_SLICE_CRA = 9,
		CODED_SLICE_GDR = 10,
		RESERVED_IRAP_VCL_11 = 11,
		RESERVED_IRAP_VCL_12 = 12,
		DCI = 13,
		VPS = 14,
		SPS = 15,
		PPS = 16,
		PREFIX_APS = 17,
		SUFFIX_APS = 18,
		PH = 19,
		ACCESS_UNIT_DELIMITER = 20,
		EOS = 21,
		EOB = 22,
		PREFIX_SEI = 23,
		SUFFIX_SEI = 24,
		FD = 25,
		RESERVED_NVCL_26 = 26,
		RESERVED_NVCL_27 = 27,
		UNSPECIFIED_28 = 28,
		UNSPECIFIED_29 = 29,
		UNSPECIFIED_30 = 30,
		UNSPECIFIED_31 = 31
	};
};

class BaseDecoderVVC : public BaseDecoder {
public:
	BaseDecoderVVC();

	bool ParseNalUnit(const uint8_t *nal, uint32_t nalLength);
	BaseDecPictType::Enum GetBasePictureType() const;
	BaseDecNalUnitType::Enum GetBaseNalUnitType() const;
	int32_t GetQP() const;
	uint32_t GetNalType() const;
	int64_t GetPictureOrderCount() const;
	uint32_t GetPictureWidth() const;
	uint32_t GetPictureHeight() const;
	bool GetDPBCanRefresh() const;
	uint8_t GetMaxNumberOfReorderFrames() const;
	uint32_t GetFrameRate() const;
	uint32_t GetBitDepthLuma() const;
	uint32_t GetBitDepthChroma() const;
	uint32_t GetChromaFormatIDC() const;
	uint32_t GetTemporalId() const;
	NALDelimitier Delimiter() const { return NALDelimiterMarker; }
	int64_t GetPictureOrderCountIncrement() const { return 1; }

private:
	bool parseVPS(vvdec::InputBitstream &bitstream);
	bool parsePPS(vvdec::InputBitstream &bitstream);
	bool parseSPS(vvdec::InputBitstream &bitstream);
	bool parsePH(vvdec::InputBitstream &bitstream);
	bool parseAPS(vvdec::InputBitstream &bitstream);
	bool parseSliceHeader(vvdec::InputBitstream &bitstream);

	uint32_t m_nuhLayerId =0;
	uint32_t m_nalUnitType =0;
	uint32_t m_temporalId =0;

	// Reader from VTM
#if defined __BASE_VVC__
	vvdec::HLSyntaxReader m_reader;
	vvdec::ParameterSetManager m_parameterSetManager;
	std::shared_ptr<vvdec::PicHeader> m_picHeader;
#endif // defined __BASE_VVC__

	vvdec::Slice *m_apcSlicePilot = nullptr;
	uint32_t m_uiSliceSegmentIdx = 0;

	vvdec::Picture* m_pcParsePic = nullptr;
	int m_prevTid0POC = 0;

	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_bitdepthLuma = 0;
	uint32_t m_bitdepthChroma = 0;
	uint32_t m_chromaFormatIDC = 0;

	BaseDecPictType::Enum m_basePictureType = BaseDecPictType::Unknown;
	uint64_t m_pictureOrderCount = 0;

	int32_t m_QP = 0;
};

} // namespace utility
} // namespace vnova
