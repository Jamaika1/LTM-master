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

#include <algorithm>
#include <assert.h>
#include <climits>
#include <cmath>
#include <memory.h>
#include <stdexcept>

#include "Misc.hpp"
#include "uBaseDecoderAVC.h"

// Parsing short_term_ref_pic_sets according to standard seems to fail very occasionally when given HM output
// Notably, HM decoder uses a different mechanism to that described in standard to maintain short term RPS.
// We don't actually need this for LCEVC embedding so bail for the momement
//
#define NO_STRPS

#define NO_VUIPARAMS

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif

namespace vnova {
namespace utility {

namespace {

const uint32_t kNalHeaderLength = 3;
const uint8_t kNalHeader[kNalHeaderLength] = {0, 0, 1};
const uint32_t kNalEscapeLength = 3;
const uint8_t kNalEscape[kNalEscapeLength] = {0, 0, 3};
const uint8_t kBitCountByteMask[9] = {0, 128, 192, 224, 240, 248, 252, 254, 255};
const uint8_t kMaxNumRefFramesAllowed = 16;

uint32_t GetBitCountFromMax(uint32_t maxValue) {
	// Bit count is determined by ceil(log2(maxRepresentation);
	// Because log2 is c99 which is not so widely available we can use standard log with a divide.
	// log2(x) = logY(x) / logY(2)
	//
	// Alternatively we want to find the highest bit set in maxValue.
	static const float logBase = log(2.0f);
	float log2Value = log(static_cast<float>(maxValue)) / logBase;
	return static_cast<uint32_t>(ceil(log2Value));
}

BaseDecNalUnitType::Enum uFromAVC(uint8_t type) {
	switch (type) {
	case AVCNalType::Unspecified:
	case AVCNalType::CodedSlice_NonIDR:
	case AVCNalType::CodedSlice_A:
	case AVCNalType::CodedSlice_B:
	case AVCNalType::CodedSlice_C:
	case AVCNalType::CodedSlice_IDR:
	case AVCNalType::CodedSlice_Aux:
	case AVCNalType::CodedSlice_Ext:
	case AVCNalType::CodedSlice_DepthExt:
		return BaseDecNalUnitType::Slice;
	case AVCNalType::SEI:
		return BaseDecNalUnitType::SEI;
	case AVCNalType::SPSExt:
	case AVCNalType::SubsetSPS:
	case AVCNalType::SPS:
		return BaseDecNalUnitType::SPS;
	case AVCNalType::PPS:
		return BaseDecNalUnitType::PPS;
	case AVCNalType::AUD:
		return BaseDecNalUnitType::AUD;
	case AVCNalType::EndOfSequence:
		return BaseDecNalUnitType::EOS;
	case AVCNalType::EndOfStream:
		return BaseDecNalUnitType::EOB;
	case AVCNalType::FillerData:
		return BaseDecNalUnitType::Filler;
	case AVCNalType::Prefix:
	case AVCNalType::DPS:
		break;
	}

	return BaseDecNalUnitType::Unknown;
}

uint32_t uOffsetForNalUnitHeader(const uint8_t *nal) {
	if (memcmp(nal, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength;
	if (!nal[0] && memcmp(nal + 1, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength + 1;
	return 0;
}

} // namespace

// A.3.1
const std::map<BaseDecoderAVC::Level::Enum, BaseDecoderAVC::Level::Limit> BaseDecoderAVC::Level::limits{
    {Number_1b, {396}},     {Number_1, {396}},      {Number_1_1, {900}},   {Number_1_2, {2376}},   {Number_1_3, {2376}},
    {Number_2, {2376}},     {Number_2_1, {4752}},   {Number_2_2, {8100}},  {Number_3, {8100}},     {Number_3_1, {18000}},
    {Number_3_2, {20480}},  {Number_4, {32768}},    {Number_4_1, {32768}}, {Number_4_2, {34816}},  {Number_5, {110400}},
    {Number_5_1, {184320}}, {Number_5_2, {184320}}, {Number_6, {696320}},  {Number_6_1, {696320}}, {Number_6_2, {696320}}};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AVC
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BaseDecoderAVC::SPS::SPS() { memset(this, 0, sizeof(SPS)); }

BaseDecoderAVC::PPS::PPS() { memset(this, 0, sizeof(PPS)); }

BaseDecoderAVC::SliceHeader::SliceHeader()
    : m_bIDR(false), m_firstMBInSlice(0), m_sliceType(SliceType::Unknown), m_ppsID(0), m_colourPlaneID(0), m_frameNum(0),
      m_bFieldPic(false), m_bBottomField(false), m_idrPicID(0), m_picOrderCntLSB(0),
      m_deltaPicOrderCntBottom(0), m_deltaPicOrderCnt{0}, m_redundantPicCnt(0), m_bDirectSpatialMVPred(false),
      m_bNumRefIDXActiveOverrideFlag(false), m_numRefIDXL0Active(0), m_numRefIDXL1Active(0), m_cabacInitIDC(0), m_sliceQPDelta(0),
      m_bSPForSwitch(false), m_sliceQSDelta(0), m_disableDeblockingFilterIDC(0), m_sliceAlphaC0OffsetDiv2(0),
      m_sliceBetaOffsetDiv2(0), m_memManagementControlOp{0}, m_pps(nullptr), m_sps(nullptr) {}

BaseDecoderAVC::SliceType::Enum BaseDecoderAVC::SliceHeader::GetType() const { return m_sliceType; }

BaseDecoderAVC::RefPic::RefPic()
    : m_topFieldOrderCnt(0), m_bottomFieldOrderCnt(0), m_picOrderCntMsb(0), m_frameNum(0), m_frameNumOffset(0) {}

BaseDecoderAVC::BaseDecoderAVC() : m_currentNalType(0), m_activeSPS(nullptr), m_activePPS(nullptr) {}

bool BaseDecoderAVC::ParseNalUnit(const uint8_t *nal, uint32_t nalLength) {
	bool bSuccess = true;

	// Account for AnnexB nal unit header ([0],0,0,1)
	uint32_t offset = uOffsetForNalUnitHeader(nal);

	// [forbidden-zero:1, nal_ref_idc:2, nal_type:5]
	m_currentNalType = (nal[offset] & 0x1F);
	m_currentNalRefIDC = ((nal[offset] >> 5) & 0x3);

	offset += 1;

	BaseDecNalUnitType::Enum nalType = uFromAVC(m_currentNalType);

	if ((nalType == BaseDecNalUnitType::SPS) || (nalType == BaseDecNalUnitType::PPS) || (nalType == BaseDecNalUnitType::SEI) ||
	    (nalType == BaseDecNalUnitType::Slice)) {
		nal += offset;
		nalLength -= offset;

		Unencapsulate(nal, nalLength, m_currentNalPayload);
		m_currentBits = *m_currentNalPayload.data();
		m_remainingBits = 8;
		m_byteOffset = 0;

		switch (nalType) {
		case BaseDecNalUnitType::PPS:
			bSuccess = ParsePPS();
			break;
		case BaseDecNalUnitType::SPS:
			bSuccess = ParseSPS();
			break;
		case BaseDecNalUnitType::Slice:
			if ((bSuccess = ParseSliceHeader())) {
				HandlePictureOrderCount();
			}
			break;
		default:
			break;
		}
	}

	return bSuccess;
}

uint8_t *BaseDecoderAVC::GetDataBuffer() const { return (uint8_t *)m_currentNalPayload.data(); }

BaseDecPictType::Enum BaseDecoderAVC::GetBasePictureType() const {
	if (m_activeSlice.m_bIDR) {
		return BaseDecPictType::IDR;
	}

	switch (m_activeSlice.m_sliceType) {
	case SliceType::P:
		return BaseDecPictType::P;
	case SliceType::B:
		return BaseDecPictType::B;
	case SliceType::I:
		return BaseDecPictType::I;
	case SliceType::SP:
		return BaseDecPictType::P;
	case SliceType::SI:
		return BaseDecPictType::I;
	default:
		break;
	};

	return BaseDecPictType::Unknown;
}

BaseDecNalUnitType::Enum BaseDecoderAVC::GetBaseNalUnitType() const { 
	return uFromAVC(m_currentNalType);
}

int32_t BaseDecoderAVC::GetQP() const { return (m_activeSlice.m_pps->m_picInitQP + m_activeSlice.m_sliceQPDelta); }

uint32_t BaseDecoderAVC::GetNalType() const { return m_currentNalType; }

uint32_t BaseDecoderAVC::GetPictureWidth() const {
	if (!m_activeSPS) {
		return 0;
	}

	uint32_t cropUnitX = 1; // (7-19)
	if (m_activeSPS->m_chromaArrayType != 0 && m_activeSPS->m_chromaArrayType <= 2) {
		// subWidthC
		cropUnitX = 2; // (7-21)
	}

	return m_activeSPS->m_picWidthInMBs * 16 -
	       cropUnitX * (m_activeSPS->m_frameCropLeftOffset + m_activeSPS->m_frameCropRightOffset); // (D-30)
}

uint32_t BaseDecoderAVC::GetPictureHeight() const {
	if (!m_activeSPS) {
		return 0;
	}

	uint32_t twoMinusFrameMBsFlag = 2 - m_activeSPS->m_bFrameMBsOnly;
	uint32_t cropUnitY = twoMinusFrameMBsFlag; // (7-20)
	if (m_activeSPS->m_chromaArrayType != 0) {
		// subHeightC * (2 - frame_mbs_only_flag)
		cropUnitY = (1 + (m_activeSPS->m_chromaArrayType <= 1)) * (twoMinusFrameMBsFlag); // (7-22)
	}

	return twoMinusFrameMBsFlag * m_activeSPS->m_picHeightInMapUnits * 16 -
	       cropUnitY * (m_activeSPS->m_frameCropTopOffset + m_activeSPS->m_frameCropBottomOffset); // (D-31)
}

// Sections 8.2.5.1 & C.2.3 of Standard
bool BaseDecoderAVC::GetDPBCanRefresh() const { return m_activeSlice.m_memManagementControlOp[5] || m_activeSlice.m_bIDR; }

int64_t BaseDecoderAVC::GetPictureOrderCount() const { return m_currentPictureOrderCount; }

// Section E.2.1 of Standard
uint8_t BaseDecoderAVC::GetMaxNumberOfReorderFrames() const {
	if (!m_activeSPS) {
		return kMaxNumRefFramesAllowed;
	}

	if (m_activeSPS->m_bVUIParametersPresent) {
		return m_activeSPS->m_vui.m_maxNumReorderFrames;
	}

	if (m_activeSPS->m_bConstraintSet3 &&
	    (m_activeSPS->m_profileIDC == 44 || m_activeSPS->m_profileIDC == 86 || m_activeSPS->m_profileIDC == 100 ||
	     m_activeSPS->m_profileIDC == 110 || m_activeSPS->m_profileIDC == 122 || m_activeSPS->m_profileIDC == 244)) {
		return 0;
	}

	// A.3.1
	auto levelLimit = GetLevelLimit(m_activeSPS);
	if (!levelLimit) {
		return kMaxNumRefFramesAllowed;
	}

	uint32_t frameHeightInMbs = (2 - m_activeSPS->m_bFrameMBsOnly) * m_activeSPS->m_picHeightInMapUnits;
	uint8_t calculatedMaxNumRefFrames = levelLimit->m_maxDpbMbs / (m_activeSPS->m_picWidthInMBs * frameHeightInMbs);
	return std::min(calculatedMaxNumRefFrames, kMaxNumRefFramesAllowed);
}

uint32_t BaseDecoderAVC::GetFrameRate() const {
	if (!m_activeSPS || !m_activeSPS->m_bVUIParametersPresent || !m_activeSPS->m_vui.m_bTimingInfoPresent) {
		return 0;
	}
	return m_activeSPS->m_vui.m_timeScale / m_activeSPS->m_vui.m_numUnitsInTick / 2;
}

uint32_t BaseDecoderAVC::GetBitDepthLuma() const {
	if (m_activeSPS)
		return m_activeSPS->m_bitDepthLuma;
	else
		return 0;
}

uint32_t BaseDecoderAVC::GetBitDepthChroma() const {
	if (m_activeSPS)
		return m_activeSPS->m_bitDepthChroma;
	else
		return 0;
}

uint32_t BaseDecoderAVC::GetChromaFormatIDC() const {
	if (m_activeSPS)
		return m_activeSPS->m_chromaFormatIDC;
	else
		return 0;
}

uint32_t BaseDecoderAVC::GetTemporalId() const {
	return 0;
}

// Section 8.2.1 of Standard
void BaseDecoderAVC::HandlePictureOrderCount() {
	assert(!m_activeSlice.m_bFieldPic);
	assert((m_activeSPS->m_picOrderCntType == 0) || (m_activeSPS->m_picOrderCntType == 2));

	uint32_t topFieldOrderCnt = 0;
	uint32_t bottomFieldOrderCnt = 0;

	// 8.2.1.1
	if (m_activeSPS->m_picOrderCntType == 0) {
		uint32_t prevPicOrderCntMsb = 0;
		uint32_t prevPicOrderCntLsb = 0;
		uint32_t picOrderCntMsb = 0;
		uint32_t maxPicOrderCntLsb = 1 << m_activeSPS->m_log2MaxPicOrderCntLSB;

		if (m_currentNalType == AVCNalType::CodedSlice_IDR) {
			prevPicOrderCntMsb = 0;
			prevPicOrderCntLsb = 0;
		} else if (m_refPic.m_header.m_memManagementControlOp[5]) {
			prevPicOrderCntMsb = 0;
			prevPicOrderCntLsb = m_refPic.m_header.m_bBottomField ? 0 : m_refPic.m_topFieldOrderCnt;
		} else {
			prevPicOrderCntMsb = m_refPic.m_picOrderCntMsb;
			prevPicOrderCntLsb = m_refPic.m_header.m_picOrderCntLSB;
		}

		// (8-3)
		if (m_activeSlice.m_picOrderCntLSB < prevPicOrderCntLsb &&
		    ((prevPicOrderCntLsb - m_activeSlice.m_picOrderCntLSB) >= (maxPicOrderCntLsb / 2))) {
			picOrderCntMsb = prevPicOrderCntMsb + maxPicOrderCntLsb;
		} else if (m_activeSlice.m_picOrderCntLSB > prevPicOrderCntLsb &&
		           ((m_activeSlice.m_picOrderCntLSB - prevPicOrderCntLsb) > (maxPicOrderCntLsb / 2))) {
			picOrderCntMsb = prevPicOrderCntMsb - maxPicOrderCntLsb;
		} else
			picOrderCntMsb = prevPicOrderCntMsb;

		// (8-4), (8-5)
		bottomFieldOrderCnt = picOrderCntMsb + m_activeSlice.m_picOrderCntLSB;
		topFieldOrderCnt = bottomFieldOrderCnt;

		if (!m_activeSlice.m_bFieldPic)
			bottomFieldOrderCnt += m_activeSlice.m_deltaPicOrderCntBottom;

		if (m_currentNalRefIDC != 0) {
			m_refPic.m_picOrderCntMsb = picOrderCntMsb;
			m_refPic.m_header = m_activeSlice;
			m_refPic.m_topFieldOrderCnt = topFieldOrderCnt;
			m_refPic.m_bottomFieldOrderCnt = bottomFieldOrderCnt;
		}
	} else {
		uint32_t maxFrameNum = 1 << m_sps->m_log2MaxFrameNum;
		uint32_t frameNumOffset = 0;

		// (8-6), (8-11) Calculate frame num offset.
		if (m_currentNalType == AVCNalType::CodedSlice_IDR)
			frameNumOffset = 0;
		else if (m_refPic.m_frameNum > m_activeSlice.m_frameNum)
			frameNumOffset = m_refPic.m_frameNumOffset + maxFrameNum;
		else
			frameNumOffset = m_refPic.m_frameNumOffset;

		if (m_activeSPS->m_picOrderCntType == 1) {
			uint32_t absFrameNum = 0;
			uint32_t expectedPicOrderCnt = 0;

			// (8-7)
			if (m_activeSPS->m_numRefFramesInPicOrderCntCycle != 0)
				absFrameNum = frameNumOffset + m_activeSlice.m_frameNum;
			else
				absFrameNum = 0;

			if (m_currentNalRefIDC == 0 && absFrameNum > 0)
				absFrameNum -= 1;

			// (8-9)
			if (absFrameNum > 0) {
				// (8-8)
				uint32_t picOrderCntCycleCnt = (absFrameNum - 1) / m_activeSPS->m_numRefFramesInPicOrderCntCycle;
				uint32_t frameNumInPicOrderCntCycle = (absFrameNum - 1) % m_activeSPS->m_numRefFramesInPicOrderCntCycle;

				// (7-12)
				uint32_t expectedDeltaPerPicOrderCntCycle = 0;
				for (uint32_t i = 0; i < m_activeSPS->m_numRefFramesInPicOrderCntCycle; ++i)
					expectedDeltaPerPicOrderCntCycle += m_activeSPS->m_offsetForRefFrame[i];

				expectedPicOrderCnt = picOrderCntCycleCnt * expectedDeltaPerPicOrderCntCycle;
				for (uint32_t i = 0; i <= frameNumInPicOrderCntCycle; ++i)
					expectedPicOrderCnt += m_activeSPS->m_offsetForRefFrame[i];
			} else
				expectedPicOrderCnt = 0;

			if (m_currentNalRefIDC == 0)
				expectedPicOrderCnt += m_activeSPS->m_offsetForNonRefPic;

			// (8-10)
			topFieldOrderCnt = expectedPicOrderCnt + m_activeSlice.m_deltaPicOrderCnt[0];
			bottomFieldOrderCnt = topFieldOrderCnt + m_activeSPS->m_offsetForTopToBottomField;

			if (!m_activeSlice.m_bFieldPic)
				bottomFieldOrderCnt += m_activeSlice.m_deltaPicOrderCnt[1];
		} else if (m_activeSPS->m_picOrderCntType == 2) {
			uint32_t tempPicOrderCnt = 0;
			// (8-12) Calculate temp pic order count.
			if (m_currentNalType == AVCNalType::CodedSlice_IDR)
				tempPicOrderCnt = 0;
			else if (m_currentNalRefIDC == 0)
				tempPicOrderCnt = 2 * (frameNumOffset + m_activeSlice.m_frameNum) - 1;
			else
				tempPicOrderCnt = 2 * (frameNumOffset + m_activeSlice.m_frameNum);

			// (8-13)
			topFieldOrderCnt = tempPicOrderCnt;
			bottomFieldOrderCnt = tempPicOrderCnt;
		}

		m_refPic.m_frameNum = m_activeSlice.m_frameNum;
		m_refPic.m_frameNumOffset = m_activeSlice.m_memManagementControlOp[5] ? 0 : frameNumOffset;
	}

	// (8-1)
	if (!m_activeSlice.m_bFieldPic)
		m_currentPictureOrderCount = std::min(topFieldOrderCnt, bottomFieldOrderCnt);
	else
		m_currentPictureOrderCount = m_activeSlice.m_bBottomField ? bottomFieldOrderCnt : topFieldOrderCnt;
}

bool BaseDecoderAVC::ParseSPS() {
	uint8_t profileIDC = ReadBits(8);
	bool bConstraintSet0 = ReadFlag();
	bool bConstraintSet1 = ReadFlag();
	bool bConstraintSet2 = ReadFlag();
	bool bConstraintSet3 = ReadFlag();
	bool bConstraintSet4 = ReadFlag();
	bool bConstraintSet5 = ReadFlag();
	uint8_t reservedZero2Bits = ReadBits(2);
	uint8_t levelIDC = ReadBits(8);
	uint32_t spsID = ReadUE();

	if (spsID >= AVCMaxSPSCount) {
		INFO("ERROR -- spsID out of range");
		throw std::runtime_error("spsID out of range");
		return false;
	}

	if (reservedZero2Bits != 0) {
		INFO("malformed sps, expected 2 reserved zero bits to be 0");
		throw std::runtime_error("malformed sps, expected 2 reserved zero bits to be 0\n");
		return false;
	}

	m_activeSPS = &m_sps[spsID];
	m_activeSPS->m_profileIDC = profileIDC;
	m_activeSPS->m_bConstraintSet0 = bConstraintSet0;
	m_activeSPS->m_bConstraintSet1 = bConstraintSet1;
	m_activeSPS->m_bConstraintSet2 = bConstraintSet2;
	m_activeSPS->m_bConstraintSet3 = bConstraintSet3;
	m_activeSPS->m_bConstraintSet4 = bConstraintSet4;
	m_activeSPS->m_bConstraintSet5 = bConstraintSet5;
	m_activeSPS->m_levelIDC = levelIDC;
	m_activeSPS->m_spsID = spsID;

	if (m_activeSPS->m_profileIDC == 100 || m_activeSPS->m_profileIDC == 110 || m_activeSPS->m_profileIDC == 122 ||
	    m_activeSPS->m_profileIDC == 244 || m_activeSPS->m_profileIDC == 44 || m_activeSPS->m_profileIDC == 83 ||
	    m_activeSPS->m_profileIDC == 86 || m_activeSPS->m_profileIDC == 118 || m_activeSPS->m_profileIDC == 128 ||
	    m_activeSPS->m_profileIDC == 138 || m_activeSPS->m_profileIDC == 139 || m_activeSPS->m_profileIDC == 134 ||
	    m_activeSPS->m_profileIDC == 135) {
		m_activeSPS->m_chromaFormatIDC = ReadUE();

		if (m_activeSPS->m_chromaFormatIDC == 3) {
			m_activeSPS->m_bSeparateColourPlane = ReadFlag();
		}

		if (m_activeSPS->m_bSeparateColourPlane == false) {
			m_activeSPS->m_chromaArrayType = m_activeSPS->m_chromaFormatIDC;
		} else {
			m_activeSPS->m_chromaArrayType = 0;
		}

		m_activeSPS->m_bitDepthLuma = ReadUE() + 8;
		m_activeSPS->m_bitDepthChroma = ReadUE() + 8;
		m_activeSPS->m_bQPPrimeYZeroTransformBypass = ReadFlag();

		if (ReadFlag()) // seq_scaling_matrix_present_flag
		{
			uint32_t loopCount = (m_activeSPS->m_chromaFormatIDC != 3) ? 8 : 12;
			HandleScalingList(loopCount);
		}
	}

	m_activeSPS->m_log2MaxFrameNum = ReadUE() + 4;
	m_activeSPS->m_picOrderCntType = ReadUE();

	if (m_activeSPS->m_picOrderCntType == 0) {
		m_activeSPS->m_log2MaxPicOrderCntLSB = ReadUE() + 4;
	} else if (m_activeSPS->m_picOrderCntType == 1) {
		m_activeSPS->m_bDeltaPicOrderAlwaysZero = ReadFlag();
		m_activeSPS->m_offsetForNonRefPic = ReadSE();
		m_activeSPS->m_offsetForTopToBottomField = ReadSE();
		m_activeSPS->m_numRefFramesInPicOrderCntCycle = ReadUE();

		for (uint32_t i = 0; i < m_activeSPS->m_numRefFramesInPicOrderCntCycle; ++i) {
			m_activeSPS->m_offsetForRefFrame[i] = ReadSE();
		}
	}

	m_activeSPS->m_numRefFrames = ReadUE();
	m_activeSPS->m_bGapsInFrameNumValueAllowed = ReadFlag();
	m_activeSPS->m_picWidthInMBs = ReadUE() + 1;
	m_activeSPS->m_picHeightInMapUnits = ReadUE() + 1;
	m_activeSPS->m_bFrameMBsOnly = ReadFlag();

	if (m_activeSPS->m_bFrameMBsOnly == false) {
		m_activeSPS->m_bMBAdaptiveFrameField = ReadFlag();
	}

	m_activeSPS->m_bDirect8x8Inference = ReadFlag();
	m_activeSPS->m_bFrameCropping = ReadFlag();

	if (m_activeSPS->m_bFrameCropping) {
		m_activeSPS->m_frameCropLeftOffset = ReadUE();
		m_activeSPS->m_frameCropRightOffset = ReadUE();
		m_activeSPS->m_frameCropTopOffset = ReadUE();
		m_activeSPS->m_frameCropBottomOffset = ReadUE();
	}

	m_activeSPS->m_bVUIParametersPresent = ReadFlag();
#if !defined(NO_VUIPARAMS)
	if (m_activeSPS->m_bVUIParametersPresent)
		HandleVUIParameters();
#endif
	return true;
}

bool BaseDecoderAVC::ParsePPS() {
	uint32_t ppsID = ReadUE();

	if (ppsID >= AVCMaxPPSCount) {
		INFO("ppsID out of range");
		throw std::runtime_error("ppsID out of range");
		return false;
	}

	m_activePPS = &m_pps[ppsID];
	m_activePPS->m_ppsID = ppsID;
	m_activePPS->m_spsID = ReadUE();
	m_activePPS->m_bEntropyCodingMode = ReadFlag();
	m_activePPS->m_bBottomFieldPicOrderInFramePresent = ReadFlag();
	m_activePPS->m_numSliceGroups = ReadUE() + 1;

	if (m_activePPS->m_numSliceGroups > 1) {
		m_activePPS->m_sliceGroupMapType = ReadUE();

		switch (m_activePPS->m_sliceGroupMapType) {
		case 0: {
			for (uint32_t group = 0; group < m_activePPS->m_numSliceGroups; ++group) {
				m_activePPS->m_runLength[group] = ReadUE() + 1;
			}
			break;
		}
		case 2: {
			for (uint32_t group = 0; group < m_activePPS->m_numSliceGroups; ++group) {
				m_activePPS->m_topLeft[group] = ReadUE();
				m_activePPS->m_bottomRight[group] = ReadUE();
			}
			break;
		}
		case 3:
		case 4:
		case 5: {
			m_activePPS->m_bSliceGroupChangeDirection = ReadFlag();
			m_activePPS->m_sliceGroupChangeRate = ReadUE() + 1;
			break;
		}
		case 6: {
			INFO("Unsupported slice group map type");
			throw std::runtime_error("Unsupported slice group map type");
			break;
		}
		default:
			break;
		}
	}

	m_activePPS->m_numRefIDXL0Active = ReadUE() + 1;
	m_activePPS->m_numRefIDXL1Active = ReadUE() + 1;
	m_activePPS->m_bWeightedPred = ReadFlag();
	m_activePPS->m_weightedBipredIDC = ReadBits(2);
	m_activePPS->m_picInitQP = ReadSE() + 26;
	m_activePPS->m_picInitQS = ReadSE() + 26;
	m_activePPS->m_chromaQPIndexOffset = ReadSE();
	m_activePPS->m_bDeblockingFilterControlPresent = ReadFlag();
	m_activePPS->m_bConstraintedIntraPred = ReadFlag();
	m_activePPS->m_bRedundantPicCntPresent = ReadFlag();

	return true;
}

bool BaseDecoderAVC::ParseSliceHeader() {
	m_activeSlice.m_firstMBInSlice = ReadUE();

	uint32_t sliceType = ReadUE();
	if (sliceType > 4)
		sliceType -= 5;

	m_activeSlice.m_sliceType = static_cast<SliceType::Enum>(sliceType);
	m_activeSlice.m_ppsID = ReadUE();
	m_activeSlice.m_pps = &m_pps[m_activeSlice.m_ppsID];
	m_activeSlice.m_sps = &m_sps[m_activeSlice.m_pps->m_spsID];

	if (m_activeSlice.m_sps->m_bSeparateColourPlane) {
		m_activeSlice.m_colourPlaneID = ReadBits(2);
	}

	m_activeSlice.m_frameNum = ReadBits(m_activeSlice.m_sps->m_log2MaxFrameNum);

	if (m_activeSlice.m_sps->m_bFrameMBsOnly == false) {
		m_activeSlice.m_bFieldPic = ReadFlag();

		if (m_activeSlice.m_bFieldPic) {
			m_activeSlice.m_bBottomField = ReadFlag();
		}
	}

	if (m_currentNalType == 5) // if(IdrPicFlag)
	{
		m_activeSlice.m_idrPicID = ReadUE();
		m_activeSlice.m_bIDR = true;
	} else {
		m_activeSlice.m_bIDR = false;
	}

	if (m_activeSlice.m_sps->m_picOrderCntType == 0) {
		m_activeSlice.m_picOrderCntLSB = ReadBits(m_activeSlice.m_sps->m_log2MaxPicOrderCntLSB);

		if (m_activeSlice.m_pps->m_bBottomFieldPicOrderInFramePresent && (m_activeSlice.m_bFieldPic == false)) {
			m_activeSlice.m_deltaPicOrderCntBottom = ReadSE();
		}
	}

	if (m_activeSlice.m_sps->m_picOrderCntType == 1 && (m_activeSlice.m_sps->m_bDeltaPicOrderAlwaysZero == false)) {
		m_activeSlice.m_deltaPicOrderCnt[0] = ReadSE();

		if (m_activeSlice.m_pps->m_bBottomFieldPicOrderInFramePresent && (m_activeSlice.m_bFieldPic == false)) {
			m_activeSlice.m_deltaPicOrderCnt[1] = ReadSE();
		}
	}

	if (m_activeSlice.m_pps->m_bRedundantPicCntPresent) {
		m_activeSlice.m_redundantPicCnt = ReadUE();
	}

	if (m_activeSlice.m_sliceType == SliceType::B) {
		m_activeSlice.m_bDirectSpatialMVPred = ReadFlag();
	}

	m_activeSlice.m_numRefIDXL0Active = m_activeSlice.m_pps->m_numRefIDXL0Active;
	m_activeSlice.m_numRefIDXL1Active = m_activeSlice.m_pps->m_numRefIDXL1Active;

	if ((m_activeSlice.m_sliceType == SliceType::P) || (m_activeSlice.m_sliceType == SliceType::SP) ||
	    (m_activeSlice.m_sliceType == SliceType::B)) {
		m_activeSlice.m_bNumRefIDXActiveOverrideFlag = ReadFlag();

		if (m_activeSlice.m_bNumRefIDXActiveOverrideFlag) {
			m_activeSlice.m_numRefIDXL0Active = ReadUE() + 1;

			if (m_activeSlice.m_sliceType == SliceType::B) {
				m_activeSlice.m_numRefIDXL1Active = ReadUE() + 1;
			}
		}
	}

	HandleRefPicListReordering();

	if ((m_activeSlice.m_pps->m_bWeightedPred &&
	     (m_activeSlice.m_sliceType == SliceType::P || m_activeSlice.m_sliceType == SliceType::SP)) ||
	    (m_activeSlice.m_pps->m_weightedBipredIDC == 1 && m_activeSlice.m_sliceType == SliceType::B)) {
		HandlePredWeightTable();
	}

	if (m_currentNalRefIDC != 0) {
		HandleDecRefPicMarking();
	}

	if (m_activeSlice.m_pps->m_bEntropyCodingMode && (m_activeSlice.m_sliceType != SliceType::I) &&
	    (m_activeSlice.m_sliceType != SliceType::SI)) {
		m_activeSlice.m_cabacInitIDC = ReadUE();
	} else {
		m_activeSlice.m_cabacInitIDC = 0;
	}

	m_activeSlice.m_sliceQPDelta = ReadSE();
	// m_activeSlice.m_pps->m_picInitQP += m_activeSlice.m_sliceQPDelta;

	if ((m_activeSlice.m_sliceType == SliceType::SP) || (m_activeSlice.m_sliceType == SliceType::SI)) {
		if (m_activeSlice.m_sliceType == SliceType::SP) {
			m_activeSlice.m_bSPForSwitch = ReadFlag();
		}

		m_activeSlice.m_sliceQSDelta = ReadSE();
	}

	m_activeSlice.m_disableDeblockingFilterIDC = 0;
	m_activeSlice.m_sliceAlphaC0OffsetDiv2 = 0;
	m_activeSlice.m_sliceBetaOffsetDiv2 = 0;

	if (m_activeSlice.m_pps->m_bDeblockingFilterControlPresent) {
		m_activeSlice.m_disableDeblockingFilterIDC = ReadUE();

		if (m_activeSlice.m_disableDeblockingFilterIDC != 1) {
			m_activeSlice.m_sliceAlphaC0OffsetDiv2 = ReadSE();
			m_activeSlice.m_sliceBetaOffsetDiv2 = ReadSE();
		}
	}

	return true;
}

void BaseDecoderAVC::HandleVUIParameters() {
	if (ReadFlag()) // aspect_ratio_info_present_flag
	{
		if (ReadBits(8) == 255) // aspect_ratio_idc == Extended_SAR
		{
			ReadBits(16); // sar_width
			ReadBits(16); // sar_height
		}
	}

	if (ReadFlag()) // overscan_info_present_flag
	{
		ReadFlag(); // overscan_appropriate_flag
	}

	if (ReadFlag()) // video_signal_type_present_flag
	{
		ReadBits(3);    // video_format
		ReadFlag();     // video_full_range_flag
		if (ReadFlag()) // colour_description_present_flag
		{
			ReadBits(8); // colour_primaries
			ReadBits(8); // transfer_characteristics
			ReadBits(8); // matrix_coefficients
		}
	}
	if (ReadFlag()) // chroma_loc_info_present_flag
	{
		ReadUE(); // chroma_sample_loc_type_top_field
		ReadUE(); // chroma_sample_loc_type_bottom_field
	}
	if ((m_activeSPS->m_vui.m_bTimingInfoPresent = ReadFlag())) {
		m_activeSPS->m_vui.m_numUnitsInTick = ReadBits(32);
		m_activeSPS->m_vui.m_timeScale = ReadBits(32);
		ReadFlag(); // fixed_frame_rate_flag
	}
	bool nalHRDParametersPresentFlag = ReadFlag();
	if (nalHRDParametersPresentFlag) {
		HandleHRDParameters();
	}
	bool vclHRDParametersPresentFlag = ReadFlag();
	if (vclHRDParametersPresentFlag) {
		HandleHRDParameters();
	}
	if (nalHRDParametersPresentFlag || vclHRDParametersPresentFlag) {
		ReadFlag(); // low_delay_hrd_flag
	}
	ReadFlag();     // pic_struct_present_flag
	if (ReadFlag()) // bitstream_restriction_flag
	{
		ReadFlag(); // motion_vectors_over_pic_boundaries_flag
		ReadUE();   // max_bytes_per_pic_denom
		ReadUE();   // max_bits_per_mb_denom
		ReadUE();   // log2_max_mv_length_horizontal
		ReadUE();   // log2_max_mv_length_vertical
		m_activeSPS->m_vui.m_maxNumReorderFrames = ReadUE();
		ReadUE(); // max_dec_frame_buffering
	}
}

void BaseDecoderAVC::HandleHRDParameters() {
	uint32_t cpbCnt = ReadUE() + 1;
	ReadBits(4); // bit_rate_scale
	ReadBits(4); // cpb_size_scale

	for (uint32_t i = 0; i < cpbCnt; i++) {
		ReadUE();   // bit_rate_value_minus1[i]
		ReadUE();   // cpb_size_value_minus1[i]
		ReadFlag(); // cbr_flag[i]
	}
	ReadBits(5); // initial_cpb_removal_delay_length_minus1
	ReadBits(5); // cpb_removal_delay_length_minus1
	ReadBits(5); // dpb_output_delay_length_minus1
	ReadBits(5); // time_offset_length
}

bool BaseDecoderAVC::HandleRefPicListReordering() {
	if (m_activeSlice.m_sliceType != SliceType::I && m_activeSlice.m_sliceType != SliceType::SI) {
		bool bRefPicListReorderingFlagL0 = ReadFlag();

		if (bRefPicListReorderingFlagL0) {
			uint32_t reorderingOfPicNumsIDC;

			do {
				reorderingOfPicNumsIDC = ReadUE();

				if ((reorderingOfPicNumsIDC == 0) || (reorderingOfPicNumsIDC == 1)) {
					ReadUE(); // abs_diff_pic_num_minus1
				} else if (reorderingOfPicNumsIDC == 2) {
					ReadUE(); // long_term_pic_num
				}

			} while (reorderingOfPicNumsIDC != 3);
		}
	}

	if (m_activeSlice.m_sliceType == SliceType::B) {
		bool bRefPicListReorderingFlagL1 = ReadFlag();

		if (bRefPicListReorderingFlagL1) {
			uint32_t reorderingOfPicNumsIDC;

			do {
				reorderingOfPicNumsIDC = ReadUE();

				if ((reorderingOfPicNumsIDC == 0) || (reorderingOfPicNumsIDC == 1)) {
					ReadUE(); // abs_diff_pic_num_minus1
				} else if (reorderingOfPicNumsIDC == 2) {
					ReadUE(); // long_term_pic_num
				}

			} while (reorderingOfPicNumsIDC != 3);
		}
	}

	return true;
}

bool BaseDecoderAVC::HandlePredWeightTable() {
	ReadUE(); // luma_log2_weight_dneom

	if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
		ReadUE(); // chroma_log2_weight_denom
	}

	for (uint32_t i = 0; i < m_activeSlice.m_numRefIDXL0Active; ++i) {
		if (ReadFlag()) // luma_weight_l0_flag
		{
			ReadSE(); // luma_weight_l0[i]
			ReadSE(); // luma_offset_l0[i]
		}

		if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
			if (ReadFlag()) // chroma_weight_l0_flag
			{
				for (uint32_t j = 0; j < 2; ++j) {
					ReadSE(); // chroma_weight_l0[i][j]
					ReadSE(); // chroma_offset_l0[i][j]
				}
			}
		}
	}

	if (m_activeSlice.m_sliceType == SliceType::B) {
		for (uint32_t i = 0; i < m_activeSlice.m_numRefIDXL1Active; ++i) {
			if (ReadFlag()) // luma_weight_l1_flag
			{
				ReadSE(); // luma_weight_l1[i]
				ReadSE(); // luma_offset_l1[i]
			}

			if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
				if (ReadFlag()) // chroma_weight_l1_flag
				{
					for (uint32_t j = 0; j < 2; ++j) {
						ReadSE(); // chroma_weight_l1[i][j]
						ReadSE(); // chroma_offset_l1[i][j]
					}
				}
			}
		}
	}

	return true;
}

bool BaseDecoderAVC::HandleDecRefPicMarking() {
	if (m_currentNalType == 5) {
		ReadFlag(); // no_output_of_prior_pics_flag
		ReadFlag(); // long_term_reference_flag
	} else {
		if (ReadFlag()) // adaptive_ref_pic_marking_mode_flag
		{
			uint32_t memManagementControlOp;

			do {
				memManagementControlOp = ReadUE();

				if (memManagementControlOp < AVCNumMMCO)
					m_activeSlice.m_memManagementControlOp[memManagementControlOp] = true;

				if ((memManagementControlOp == 1) || (memManagementControlOp == 3)) {
					ReadUE(); // difference_of_pic_nums_minus1
				}
				if (memManagementControlOp == 2) {
					ReadUE(); // long_term_pic_num
				}

				if ((memManagementControlOp == 3) || (memManagementControlOp == 6)) {
					ReadUE(); // long_term_frame_idx
				}

				if (memManagementControlOp == 4) {
					ReadUE(); // max_long_term_Frame_idx_plus1
				}

			} while (memManagementControlOp != 0);
		}
	}

	return true;
}

bool BaseDecoderAVC::HandleScalingList(uint32_t loopCount) {
	for (uint32_t i = 0; i < loopCount; ++i) {
		if (ReadFlag()) // seq_scaling_list_present_flag
		{
			if (i < 6) {
				HandleScalingListElement(16);
			} else {
				HandleScalingListElement(64);
			}
		}
	}

	return true;
}

bool BaseDecoderAVC::HandleScalingListElement(uint32_t listSize) {
	int32_t lastScale = 8;
	int32_t nextScale = 8;
	int32_t deltaScale;

	for (uint32_t j = 0; j < listSize; ++j) {
		if (nextScale != 0) {
			deltaScale = ReadSE();
			nextScale = (lastScale + deltaScale + 256) % 256;
		}
		lastScale = (nextScale == 0) ? lastScale : nextScale;
	}

	return true;
}

// A.3.1
const BaseDecoderAVC::Level::Limit *BaseDecoderAVC::GetLevelLimit(SPS *sps) const {
	if (!sps) {
		return nullptr;
	}

	Level::Enum level = static_cast<Level::Enum>(sps->m_levelIDC);
	if (level == Level::Number_1_1 && sps->m_bConstraintSet3) {
		level = Level::Number_1b;
	}
	auto limits = Level::limits;
	return limits.find(level) != limits.end() ? &Level::limits.at(level) : nullptr;
}

std::unique_ptr<BaseDecoder> CreateBaseDecoderAVC() { return std::unique_ptr<BaseDecoder>{new BaseDecoderAVC{}}; }

} // namespace utility
} // namespace vnova

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#endif
