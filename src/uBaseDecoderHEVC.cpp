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
#include "uBaseDecoderHEVC.h"

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

BaseDecNalUnitType::Enum uFromHEVC(uint8_t type) {
	switch (type) {
	case HEVCNalType::CodedSlice_Trail_N:
	case HEVCNalType::CodedSlice_Trail_R:
	case HEVCNalType::CodedSlice_TSA_N:
	case HEVCNalType::CodedSlice_TLA_R:
	case HEVCNalType::CodedSlice_STSA_N:
	case HEVCNalType::CodedSlice_STSA_R:
	case HEVCNalType::CodedSlice_RADL_N:
	case HEVCNalType::CodedSlice_RADL_R:
	case HEVCNalType::CodedSlice_RASL_N:
	case HEVCNalType::CodedSlice_RASL_R:
	case HEVCNalType::CodedSlice_BLA_W_LP:
	case HEVCNalType::CodedSlice_BLA_W_RADL:
	case HEVCNalType::CodedSlice_BLA_N_LP:
	case HEVCNalType::CodedSlice_IDR_W_RADL:
	case HEVCNalType::CodedSlice_IDR_N_LP:
	case HEVCNalType::CodedSlice_CRA:
		return BaseDecNalUnitType::Slice;
	case HEVCNalType::VPS:
		return BaseDecNalUnitType::VPS;
	case HEVCNalType::SPS:
		return BaseDecNalUnitType::SPS;
	case HEVCNalType::PPS:
		return BaseDecNalUnitType::PPS;
	case HEVCNalType::AUD:
		return BaseDecNalUnitType::AUD;
	case HEVCNalType::EndOfSequence:
		return BaseDecNalUnitType::EOS;
	case HEVCNalType::EndOfBitstream:
		return BaseDecNalUnitType::EOB;
	case HEVCNalType::FillerData:
		return BaseDecNalUnitType::Filler;
	case HEVCNalType::PrefixSEI:
	case HEVCNalType::SuffixSEI:
		return BaseDecNalUnitType::SEI;
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

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// HEVC
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BaseDecoderHEVC::SPS::SPS() { memset(this, 0, sizeof(SPS)); }

BaseDecoderHEVC::PPS::PPS() { memset(this, 0, sizeof(PPS)); }

BaseDecoderHEVC::SliceHeader::SliceHeader() { memset(this, 0, sizeof(SliceHeader)); }

BaseDecoderHEVC::BaseDecoderHEVC()
    : m_currentNalType(0), m_bFirstPicture(true), m_bNoRaslOutput(false), m_activeSPS(nullptr), m_activePPS(nullptr) {}

bool BaseDecoderHEVC::ParseNalUnit(const uint8_t *nal, uint32_t nalLength) {
	bool bSuccess = true;

	// Account for AnnexB nal unit header ([0],0,0,1)
	uint32_t offset = uOffsetForNalUnitHeader(nal);

	// [forbidden-zero:1, nal_type:6, layer_id_top:1]
	m_currentNalType = ((nal[offset] >> 1) & 0x3F);
	// [layer_id:5, temporal_id_plus1:3]
	m_currentTemporalId = nal[offset + 1] >> 5;

	offset += 2;

	BaseDecNalUnitType::Enum nalType = uFromHEVC(m_currentNalType);

	// if (nalType != BaseDecNalUnitType::VPS)
	{
		// printf("BaseDecoderHEVC::ParseNalUnit [%d] --> [%s] nallenght [%d]\n", m_currentNalType, uToString(nalType),
		// nalLengthIn);
		nal += offset;
		nalLength -= offset;

		Unencapsulate(nal, nalLength, m_currentNalPayload);
		m_currentBits = *m_currentNalPayload.data();
		m_remainingBits = 8;
		m_byteOffset = 0;

		switch (nalType) {
		case BaseDecNalUnitType::VPS:
			bSuccess = ParseVPS();
			break;
		case BaseDecNalUnitType::PPS:
			bSuccess = ParsePPS();
			break;
		case BaseDecNalUnitType::SPS:
			bSuccess = ParseSPS();
			break;
		case BaseDecNalUnitType::Slice:
			if ((bSuccess = ParseSliceHeader())) {
				HandleSliceHeader();
				HandlePictureOrderCount();
				m_bFirstPicture = false;
			}
			break;
		case BaseDecNalUnitType::EOS:
			m_bFirstPicture = true;
			break;
		default:
			break;
		}
	}
	// else
	//    printf("BaseDecoderHEVC::ParseNalUnit [%d] --> [%s] nallenght [%d] ---->> SKIPPED\n", m_currentNalType,
	//    uToString(nalType), nalLengthIn);

	return bSuccess;
}

BaseDecPictType::Enum BaseDecoderHEVC::GetBasePictureType() const {
	switch (m_activeSlice.m_sliceType) {
	case SliceType::P:
		return BaseDecPictType::P;
	case SliceType::B:
		return BaseDecPictType::B;
	case SliceType::I:
		return BaseDecPictType::I;
	default:
		break;
	}

	return BaseDecPictType::Unknown;
}

BaseDecNalUnitType::Enum BaseDecoderHEVC::GetBaseNalUnitType() const {
	return uFromHEVC(m_currentNalType);
}

int32_t BaseDecoderHEVC::GetQP() const { return (m_activeSlice.m_pps->m_initQP + m_activeSlice.m_sliceQPDelta); }

uint32_t BaseDecoderHEVC::GetNalType() const { return m_currentNalType; }

uint32_t BaseDecoderHEVC::GetPictureWidth() const {
	if (!m_activeSPS) {
		return 0;
	}

	// 6.2
	uint32_t subWidthC = 1;
	if (m_activeSPS->m_chromaFormatIDC == 1 || m_activeSPS->m_chromaFormatIDC == 2) {
		subWidthC = 2;
	}

	return m_activeSPS->m_picWidthInLumaSamples -
	       subWidthC * (m_activeSPS->m_confWinRightOffset + m_activeSPS->m_confWinLeftOffset); // (D-28)
}

uint32_t BaseDecoderHEVC::GetPictureHeight() const {
	if (!m_activeSPS) {
		return 0;
	}

	// 6.2
	uint32_t subHeightC = 1;
	if (m_activeSPS->m_chromaFormatIDC == 1) {
		subHeightC = 2;
	}

	return m_activeSPS->m_picHeightInLumaSamples -
	       subHeightC * (m_activeSPS->m_confWinBottomOffset + m_activeSPS->m_confWinTopOffset); // (D-29)
}

// Sections 8.3.2 & C.3.2 of Standard
bool BaseDecoderHEVC::GetDPBCanRefresh() const { return m_currentPictureOrderCount == 0; }

uint32_t BaseDecoderHEVC::GetBitDepthLuma() const {
	if (m_activeSPS)
		return m_activeSPS->m_bitDepthLuma;
	else
		return 0;
}

uint32_t BaseDecoderHEVC::GetBitDepthChroma() const {
	if (m_activeSPS)
		return m_activeSPS->m_bitDepthChroma;
	else
		return 0;
}

uint32_t BaseDecoderHEVC::GetChromaFormatIDC() const {
	if (m_activeSPS)
		return m_activeSPS->m_chromaFormatIDC;
	else
		return 0;
}

int64_t BaseDecoderHEVC::GetPictureOrderCount() const { return m_currentPictureOrderCount; }

// Section C.5.2.2 of Standard
uint8_t BaseDecoderHEVC::GetMaxNumberOfReorderFrames() const {
	return m_activeSPS ? m_activeSPS->m_maxNumReorderPics[m_activeSPS->m_maxSubLayers - 1] : kMaxNumRefFramesAllowed;
}

uint32_t BaseDecoderHEVC::GetFrameRate() const {
	if (!m_activeSPS || !m_activeSPS->m_bVUIParametersPresent || !m_activeSPS->m_vui.m_bTimingInfoPresent) {
		return 0;
	}
	return m_activeSPS->m_vui.m_timeScale / m_activeSPS->m_vui.m_numUnitsInTick;
}

uint32_t BaseDecoderHEVC::GetTemporalId() const {
	return m_currentTemporalId;
}

// Section 8.3.1 of Standard
void BaseDecoderHEVC::HandlePictureOrderCount() {
	uint32_t picOrderCntMsb = 0;
	uint32_t maxPicOrderCntLsb = 1 << m_activeSPS->m_log2MaxPicOrderCntLSB;
	HEVCNalType::Enum nalType = static_cast<HEVCNalType::Enum>(m_currentNalType);

	if (isIdr(nalType)) {
		// Note 1
		m_prevTid0Pic.m_picOrderCntLsb = 0;
		m_prevTid0Pic.m_picOrderCntMsb = 0;
		m_activeSlice.m_slicePicOrderCntLSB = 0;
	}

	uint32_t prevPicOrderCntLsb = m_prevTid0Pic.m_picOrderCntLsb;
	uint32_t prevPicOrderCntMsb = m_prevTid0Pic.m_picOrderCntMsb;

	// (8-1)
	if (isIrap(nalType) && m_bNoRaslOutput) {
		picOrderCntMsb = 0;
	} else if (m_activeSlice.m_slicePicOrderCntLSB < prevPicOrderCntLsb &&
	           (prevPicOrderCntLsb - m_activeSlice.m_slicePicOrderCntLSB) >= (maxPicOrderCntLsb / 2)) {
		picOrderCntMsb = prevPicOrderCntMsb + maxPicOrderCntLsb;
	} else if (m_activeSlice.m_slicePicOrderCntLSB > prevPicOrderCntLsb &&
	           (m_activeSlice.m_slicePicOrderCntLSB - prevPicOrderCntLsb) > (maxPicOrderCntLsb / 2)) {
		picOrderCntMsb = prevPicOrderCntMsb - maxPicOrderCntLsb;
	} else
		picOrderCntMsb = prevPicOrderCntMsb;

	if (!isRasl(nalType) && !isRadl(nalType) && !isSubLayerNonReferencePicture(nalType) && m_currentTemporalId == 0) {
		m_prevTid0Pic.m_picOrderCntLsb = m_activeSlice.m_slicePicOrderCntLSB;
		m_prevTid0Pic.m_picOrderCntMsb = picOrderCntMsb;
	}

	// (8-2)
	m_currentPictureOrderCount = picOrderCntMsb + m_activeSlice.m_slicePicOrderCntLSB;
}

// Section 7.4.2.2 of Standard
bool BaseDecoderHEVC::isSubLayerNonReferencePicture(HEVCNalType::Enum nut) {
	switch (nut) {
	case HEVCNalType::CodedSlice_Trail_N:
	case HEVCNalType::CodedSlice_TSA_N:
	case HEVCNalType::CodedSlice_STSA_N:
	case HEVCNalType::CodedSlice_RADL_N:
	case HEVCNalType::CodedSlice_RASL_N:
	case HEVCNalType::RSV_VCL_N10:
	case HEVCNalType::RSV_VCL_N12:
	case HEVCNalType::RSV_VCL_N14:
		return true;
	default:
		return false;
	}
}

// Section 3.62 of Standard
bool BaseDecoderHEVC::isIdr(HEVCNalType::Enum nut) {
	switch (nut) {
	case HEVCNalType::CodedSlice_IDR_W_RADL:
	case HEVCNalType::CodedSlice_IDR_N_LP:
		return true;
	default:
		return false;
	}
}

// Section 3.16 of Standard
bool BaseDecoderHEVC::isBla(HEVCNalType::Enum nut) {
	switch (nut) {
	case HEVCNalType::CodedSlice_BLA_W_LP:
	case HEVCNalType::CodedSlice_BLA_N_LP:
	case HEVCNalType::CodedSlice_BLA_W_RADL:
		return true;
	default:
		return false;
	}
}

// Section 3.117 of Standard
bool BaseDecoderHEVC::isRasl(HEVCNalType::Enum nut) {
	switch (nut) {
	case HEVCNalType::CodedSlice_RASL_N:
	case HEVCNalType::CodedSlice_RASL_R:
		return true;
	default:
		return false;
	}
}

// Section 3.115 of Standard
bool BaseDecoderHEVC::isRadl(HEVCNalType::Enum nut) {
	switch (nut) {
	case HEVCNalType::CodedSlice_RADL_N:
	case HEVCNalType::CodedSlice_RADL_R:
		return true;
	default:
		return false;
	}
}

// Section 3.68 of Standard
bool BaseDecoderHEVC::isIrap(HEVCNalType::Enum nut) {
	return nut >= HEVCNalType::CodedSlice_BLA_W_LP && nut <= HEVCNalType::RSV_IRAP_VCL23;
}

bool BaseDecoderHEVC::ParseVPS() {
	return true; // No need to parse, so bail out
}

bool BaseDecoderHEVC::ParseSPS() {
	uint32_t vpsID = ReadBits(4);
	// REPORT("%-40s - %8d", "sps_video_parameter_set_id", vpsID);
	uint32_t maxSubLayersMinus1 = ReadBits(3);
	// REPORT("%-40s - %8d", "sps_max_sub_layers_minus1", maxSubLayersMinus1);
	bool bTemporalIDNesting = ReadFlag();
	// REPORT("%-40s - %8d", "sps_temporal_id_nesting_flag", bTemporalIDNesting);

	if (!ParseProfileTierLevels(true, maxSubLayersMinus1)) {
		return false;
	}

	uint32_t spsID = ReadUE();
	// REPORT("%-40s - %8d", "sps_seq_parameter_set_id", spsID);

	if (spsID >= HEVCMaxSPSCount) {
		INFO("spsID out of range %4d", spsID);
		throw std::runtime_error("spsID out of range");
		return false;
	}

	m_activeSPS = &m_sps[spsID];
	m_activeSPS->m_vpsID = vpsID;
	m_activeSPS->m_maxSubLayers = maxSubLayersMinus1 + 1;
	m_activeSPS->m_bTemporalIDNesting = bTemporalIDNesting;
	m_activeSPS->m_spsID = spsID;
	m_activeSPS->m_chromaFormatIDC = ReadUE();
	// REPORT("%-40s - %8d", "chroma_format_idc", m_activeSPS->m_chromaFormatIDC);

	if (m_activeSPS->m_chromaFormatIDC == 3) {
		m_activeSPS->m_bSeparateColourPlane = ReadFlag();
	}

	if (m_activeSPS->m_bSeparateColourPlane == false) {
		m_activeSPS->m_chromaArrayType = m_activeSPS->m_chromaFormatIDC;
	} else {
		m_activeSPS->m_chromaArrayType = 0;
	}

	m_activeSPS->m_picWidthInLumaSamples = ReadUE();
	// REPORT("%-40s - %8d", "pic_width_in_luma_samples", m_activeSPS->m_picWidthInLumaSamples);
	m_activeSPS->m_picHeightInLumaSamples = ReadUE();
	// REPORT("%-40s - %8d", "pic_height_in_luma_samples", m_activeSPS->m_picHeightInLumaSamples);
	m_activeSPS->m_bConformanceWindow = ReadFlag();
	// REPORT("%-40s - %8d", "conformance_window_flag", m_activeSPS->m_bConformanceWindow);

	if (m_activeSPS->m_bConformanceWindow) {
		m_activeSPS->m_confWinLeftOffset = ReadUE();
		m_activeSPS->m_confWinRightOffset = ReadUE();
		m_activeSPS->m_confWinTopOffset = ReadUE();
		m_activeSPS->m_confWinBottomOffset = ReadUE();
	}

	m_activeSPS->m_bitDepthLuma = ReadUE() + 8;
	m_activeSPS->m_bitDepthChroma = ReadUE() + 8;
	m_activeSPS->m_log2MaxPicOrderCntLSB = ReadUE() + 4;
	// REPORT("%-40s - %8d", "log2_max_pic_order_cnt_lsb_minus4", m_activeSPS->m_log2MaxPicOrderCntLSB - 4);

	m_activeSPS->m_bSubLayerOrderingInfoPresent = ReadFlag();
	// REPORT("%-40s - %8d", "sps_sub_layer_ordering_info_present_flag", m_activeSPS->m_bSubLayerOrderingInfoPresent);

	for (uint32_t i = (m_activeSPS->m_bSubLayerOrderingInfoPresent ? 0 : maxSubLayersMinus1); i <= maxSubLayersMinus1; ++i) {
		int sps_max_dec_pic_buffering_minus1 = ReadUE(); // max_dec_pic_buffering_minus1
		// REPORT("%-40s - %8d - %8d", "sps_max_dec_pic_buffering_minus1", i, sps_max_dec_pic_buffering_minus1);
		m_activeSPS->m_maxNumReorderPics[i] = ReadUE();
		// REPORT("%-40s - %8d - %8d", "sps_max_num_reorder_pics", i, m_activeSPS->m_maxNumReorderPics[i]);
		int max_latency_increase_plus1 = ReadUE(); // max_latency_increase_plus1
		// REPORT("%-40s - %8d - %8d", "max_latency_increase_plus1", i, max_latency_increase_plus1);
	}

	m_activeSPS->m_log2MinLumaCodingBlockSize = ReadUE() + 3;
	m_activeSPS->m_log2DiffMaxMinLumaCodingBlockSize = ReadUE();
	m_activeSPS->m_log2MinLumaTransformBlockSize = ReadUE() + 2;
	m_activeSPS->m_log2DiffMaxMinLumaTransformBlockSize = ReadUE();
	m_activeSPS->m_maxTransformHierarchyDepthInter = ReadUE();
	// REPORT("%-40s - %8d", "max_transform_hierarchy_depth_inter", m_activeSPS->m_maxTransformHierarchyDepthInter);
	m_activeSPS->m_maxTransformHierarchyDepthIntra = ReadUE();
	// REPORT("%-40s - %8d", "max_transform_hierarchy_depth_intra", m_activeSPS->m_maxTransformHierarchyDepthIntra);
	m_activeSPS->m_bScalingListEnabled = ReadFlag();
	// REPORT("%-40s - %8d", "scaling_list_enabled_flag", m_activeSPS->m_bScalingListEnabled);

	if (m_activeSPS->m_bScalingListEnabled) {
		m_activeSPS->m_bScalingListDataPresent = ReadFlag();

		if (m_activeSPS->m_bScalingListDataPresent) {
			// todo!
			BaseDecoderHEVC::HandleScalingList();
			// throw std::runtime_error("Unimplemented HEVC header parsing [scaling list]\n");
		}
	}

	m_activeSPS->m_bAMPEnabled = ReadFlag();
	m_activeSPS->m_bSampleAdaptiveOffsetEnabled = ReadFlag();
	m_activeSPS->m_bPCMEnabled = ReadFlag();

	if (m_activeSPS->m_bPCMEnabled) {
		m_activeSPS->m_pcmSampleBitDepthLuma = ReadBits(4) + 1;
		m_activeSPS->m_pcmSampleBitDepthChroma = ReadBits(4) + 1;
		m_activeSPS->m_log2MinPCMLumaCodingBlockSize = ReadUE() + 3;
		m_activeSPS->m_log2DiffMaxMinPCMLumaCodingBlockSize = ReadUE();
		m_activeSPS->m_bPCMLoopFilterDisabled = ReadFlag();
	}

	m_activeSPS->m_numShortTermRefPicSets = ReadUE();
	// REPORT("%-40s - %8d", "num_short_term_ref_pic_sets", m_activeSPS->m_numShortTermRefPicSets);

	// debug0879
	// Ignore remaining data for now.
	return true;

	for (uint32_t i = 0; i < m_activeSPS->m_numShortTermRefPicSets; ++i) {
		ParseShortTermRefSet(i);
		// printf("Unimplemented HEVC header pasing [short term ref pic sets]\n");
		// throw std::runtime_error("Unimplemented HEVC header pasing [short term ref pic sets]\n");
	}

	m_activeSPS->m_bLongTermRefPicsPresent = ReadFlag();

	if (m_activeSPS->m_bLongTermRefPicsPresent) {
		m_activeSPS->m_numLongTermRefPics = ReadUE();

		for (uint32_t i = 0; i < m_activeSPS->m_numLongTermRefPics; ++i) {
			m_activeSPS->m_longTermRefPicPOCLSB[i] = ReadBits(m_activeSPS->m_log2MaxPicOrderCntLSB);
			m_activeSPS->m_bLongTermUsedByCurrentPic[i] = ReadFlag();
		}
	}

	m_activeSPS->m_bTemporalMVPEnabled = ReadFlag();
	m_activeSPS->m_bStrongIntraSmoothingEnabled = ReadFlag();
#if !defined(NO_VUIPARAMS)
	if ((m_activeSPS->m_bVUIParametersPresent = ReadFlag())) {
		HandleVUIParameters(); // Doesn't parse entire VUI
	}
#endif

	// Ignore remaining data for now.
	return true;
}

bool BaseDecoderHEVC::ParsePPS() {
	uint32_t ppsID = ReadUE();

	if (ppsID >= HEVCMaxPPSCount) {
		INFO("ppsID out of range %4d", ppsID);
		throw std::runtime_error("ppsID out of range");
		return false;
	}

	m_activePPS = &m_pps[ppsID];
	m_activePPS->m_ppsID = ppsID;
	m_activePPS->m_spsID = ReadUE();
	m_activePPS->m_bDependentSliceSegmentsEnabled = ReadFlag();
	m_activePPS->m_bOutputFlagPresent = ReadFlag();
	m_activePPS->m_numExtraSliceHeaderBits = ReadBits(3);
	m_activePPS->m_bSignDataHidingEnabled = ReadFlag();
	m_activePPS->m_bCabacInitPresent = ReadFlag();
	m_activePPS->m_numRefIDXL0Active = ReadUE() + 1;
	m_activePPS->m_numRefIDXL1Active = ReadUE() + 1;
	m_activePPS->m_initQP = ReadSE() + 26;
	m_activePPS->m_bConstrainedIntraPred = ReadFlag();
	m_activePPS->m_bTransformSkipEnabled = ReadFlag();
	m_activePPS->m_bCUQPDeltaEnabled = ReadFlag();

	if (m_activePPS->m_bCUQPDeltaEnabled) {
		m_activePPS->m_diffCUQPDeltaDepth = ReadUE();
	}

	m_activePPS->m_cbqpOffset = ReadSE();
	m_activePPS->m_crqpOffset = ReadSE();
	m_activePPS->m_bSliceChromaQPOffsetsPresent = ReadFlag();
	m_activePPS->m_bWeightedPred = ReadFlag();
	m_activePPS->m_bWeightedBipred = ReadFlag();
	m_activePPS->m_bTransquantBypassEnabled = ReadFlag();
	m_activePPS->m_bTilesEnabled = ReadFlag();
	m_activePPS->m_bEntropyCodingSyncEnabled = ReadFlag();

	if (m_activePPS->m_bTilesEnabled) {
		INFO("Unimplemented HEVC header parsing [tiles enabled]");
		throw std::runtime_error("Unimplemented HEVC header parsing [tiles enabled]");
	}

	m_activePPS->m_bPPSLoopFilterAcrossSlicesEnabled = ReadFlag();
	m_activePPS->m_bDeblockingFilterControlPresent = ReadFlag();

	if (m_activePPS->m_bDeblockingFilterControlPresent) {
		m_activePPS->m_bDeblockingFilterOverrideEnabled = ReadFlag();
		m_activePPS->m_bPPSDeblockingFilterDisabled = ReadFlag();

		if (!m_activePPS->m_bPPSDeblockingFilterDisabled) {
			m_activePPS->m_ppsBetaOffsetDiv2 = ReadSE();
			m_activePPS->m_ppsTCOffsetDiv2 = ReadSE();
		}
	}

	m_activePPS->m_bPPSScalingListDataPresent = ReadFlag();

	if (m_activePPS->m_bPPSScalingListDataPresent) {
		for (uint8_t sizeId = 0; sizeId < 4; ++sizeId) {
			for (uint8_t matrixId = 0; matrixId < 6; matrixId += (sizeId == 3 ? 3 : 1)) {
				bool scalingListPredMode = ReadFlag(); // scaling_list_pred_mode_flag[sizeId][matrixId]

				if (!scalingListPredMode)
					ReadUE(); // scaling_list_pred_matrix_id_delta[sizeId][matrixId]
				else {
					int32_t nextCoef = 8;
					uint32_t coefNum = std::min(64, (1 << (4 + (sizeId << 1))));

					if (sizeId > 1)
						nextCoef = ReadSE() + 8; // scaling_list_dc_coef_minus8[sizeId - 2][matrixId]

					for (uint32_t i = 0; i < coefNum; ++i)
						ReadSE(); // scaling_list_delta_coef
				}
			}
		}
	}

	m_activePPS->m_bListsModificationPresent = ReadFlag();
	m_activePPS->m_log2ParallelMergeLevelMinus2 = ReadUE();
	m_activePPS->m_bSliceSegmentHeaderExtensionPresent = ReadFlag();

	return true;
}

bool BaseDecoderHEVC::ParseSliceHeader() {
	m_activeSlice.m_bFirstSliceSegmentInPic = ReadFlag();

	if (m_currentNalType >= 16 && m_currentNalType <= 23) {
		m_activeSlice.m_bNoOutputOfPriorPics = ReadFlag();
	}

	m_activeSlice.m_ppsID = ReadUE();
	m_activeSlice.m_pps = &m_pps[m_activeSlice.m_ppsID];
	m_activeSlice.m_sps = &m_sps[m_activeSlice.m_pps->m_spsID];

	if (!m_activeSlice.m_bFirstSliceSegmentInPic) {
		if (m_activeSlice.m_pps->m_bDependentSliceSegmentsEnabled) {
			m_activeSlice.m_bDependentSliceSegment = ReadFlag();
		}

		// @todo
		m_activeSlice.m_sliceSegmentAddress = 0;
		INFO("Unimplemented HEVC header parsing [first slice segment in pic]");
		throw std::runtime_error("Unimplemented HEVC header parsing [first slice segment in pic]\n");
	}

	if (!m_activeSlice.m_pps->m_bDependentSliceSegmentsEnabled) {
		for (uint32_t i = 0; i < m_activeSlice.m_pps->m_numExtraSliceHeaderBits; ++i) {
			ReadFlag(); // slice_reserved_flag[ i ]
		}

		m_activeSlice.m_sliceType = ReadUE();

		if (m_activeSlice.m_pps->m_bOutputFlagPresent) {
			m_activeSlice.m_bPicOutput = ReadFlag();
		}

		if (m_activeSlice.m_sps->m_bSeparateColourPlane) {
			m_activeSlice.m_colourPlaneID = ReadBits(2);
		}

		if (m_currentNalType != 19 && m_currentNalType != 20) {
			m_activeSlice.m_slicePicOrderCntLSB = ReadBits(m_activeSlice.m_sps->m_log2MaxPicOrderCntLSB);
			bool bShortTermRefPicSetSPS = ReadFlag(); // short_term_ref_pic_set_sps_flag
#if !defined(NO_STRPS)
			if (!bShortTermRefPicSetSPS) {
				ParseShortTermRefSet(m_activeSlice.m_sps->m_numShortTermRefPicSets);
			} else if (m_activeSlice.m_sps->m_numShortTermRefPicSets > 1) {
				m_activeSlice.m_shortTermRefPicIdx = ReadBits(GetBitCountFromMax(m_activeSlice.m_sps->m_numShortTermRefPicSets));
			}

			if (m_activeSlice.m_sps->m_bLongTermRefPicsPresent) {
				if (m_activeSlice.m_sps->m_numLongTermRefPics > 0) {
					m_activeSlice.m_numLongTermSPS = ReadUE();
				}

				m_activeSlice.m_numLongTermPics = ReadUE();

				for (uint32_t i = 0; i < (m_activeSlice.m_numLongTermSPS + m_activeSlice.m_numLongTermPics); ++i) {
					if (i < m_activeSlice.m_numLongTermSPS) {
						if (m_activeSlice.m_sps->m_numLongTermRefPics > 1) {
							ReadBits(GetBitCountFromMax(m_activeSlice.m_sps->m_numLongTermRefPics)); // lt_idx_sps[i]
						}
					} else {
						ReadBits(GetBitCountFromMax(m_activeSlice.m_sps->m_log2MaxPicOrderCntLSB)); // poc_lsb_lt[i]
						ReadFlag();                                                                 // used_by_curr_pic_lt_flag[i]
					}

					if (ReadFlag()) // delta_poc_msb_present[i]
					{
						ReadUE(); // delta_poc_msb_cycle_lt[i]
					}
				}
			}

			if (m_activeSlice.m_sps->m_bTemporalMVPEnabled) {
				ReadFlag(); // slice_temporal_mvp_enabled
			}
#endif
		}
#if !defined(NO_STRPS) // Out of sync - ignore
		if (m_activeSlice.m_sps->m_bSampleAdaptiveOffsetEnabled) {
			m_activeSlice.m_bSliceSAOLuma = ReadFlag();

			if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
				m_activeSlice.m_bSliceSAOChroma = ReadFlag();
			}
		}

		if ((m_activeSlice.m_sliceType == SliceType::P) || (m_activeSlice.m_sliceType == SliceType::B)) {
			m_activeSlice.m_bNumRefIdxActiveOverride = ReadFlag();

			if (m_activeSlice.m_bNumRefIdxActiveOverride) {
				m_activeSlice.m_numRefIDXL0ActiveMinus1 = ReadUE();

				if (m_activeSlice.m_sliceType == SliceType::B) {
					m_activeSlice.m_numRefIDXL1ActiveMinus1 = ReadUE();
				}
			}

			if (m_activeSlice.m_pps->m_bListsModificationPresent /*&& (NumPicTotalCurr > 1)*/) {
				ParseRefPicListsModification();
			}

			if (m_activeSlice.m_sliceType == SliceType::B) {
				m_activeSlice.m_bMVDL1Zero = ReadFlag();
			}

			if (m_activeSlice.m_pps->m_bCabacInitPresent) {
				m_activeSlice.m_bCabacInit = ReadFlag();
			}

			if (m_activeSlice.m_sps->m_bTemporalMVPEnabled) {
				if (m_activeSlice.m_sliceType == SliceType::B) {
					m_activeSlice.m_bCollocatedFromL0 = ReadFlag();
				}

				if ((m_activeSlice.m_bCollocatedFromL0 && m_activeSlice.m_numRefIDXL0ActiveMinus1 > 0) ||
				    (!m_activeSlice.m_bCollocatedFromL0 && m_activeSlice.m_numRefIDXL1ActiveMinus1 > 0)) {
					m_activeSlice.m_collocatedRefIdx = ReadUE();
				}
			}

			if ((m_activeSlice.m_pps->m_bWeightedPred && (m_activeSlice.m_sliceType == SliceType::P)) ||
			    (m_activeSlice.m_pps->m_bWeightedBipred && (m_activeSlice.m_sliceType == SliceType::B))) {
				ParsePredWeightTable();
			}

			m_activeSlice.m_fiveMinusMaxNumMergeCand = ReadUE();
		}

		m_activeSlice.m_sliceQPDelta = ReadSE();
		// m_activePPS->m_initQP += m_activeSlice.m_sliceQPDelta;	//	Modifies per-slice.
#endif
	}
	return true;
}

void BaseDecoderHEVC::HandleVUIParameters() {
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
	ReadFlag(); // neutral_chroma_indication_flag
	ReadFlag(); // field_seq_flag
	ReadFlag(); // frame_field_info_present_flag

	if (ReadFlag()) // default_display_window_flag
	{
		ReadUE(); // def_disp_win_left_offset
		ReadUE(); // def_disp_win_right_offset
		ReadUE(); // def_disp_win_top_offset
		ReadUE(); // def_disp_win_bottom_offset
	}
	if ((m_activeSPS->m_vui.m_bTimingInfoPresent = ReadFlag())) {
		m_activeSPS->m_vui.m_numUnitsInTick = ReadBits(32);
		m_activeSPS->m_vui.m_timeScale = ReadBits(32);
		// Ignore remaining data for now
	}
}

bool BaseDecoderHEVC::ParseProfileTierLevels(bool bProfilePresent, uint32_t maxNumSubLayersMinus1) {
	if (bProfilePresent)
		ParseProfileTierLevel(true); // General Profile

	std::vector<bool> subLayerProfilePresent(maxNumSubLayersMinus1);
	std::vector<bool> subLayerLevelPresent(maxNumSubLayersMinus1);

	for (uint32_t i = 0; i < maxNumSubLayersMinus1; ++i) {
		subLayerProfilePresent[i] = ReadFlag();
		// REPORT("%-40s - %8d - %8d", "sub_layer_profile_present_flag", i, (bool)(subLayerProfilePresent[i]));
		subLayerLevelPresent[i] = ReadFlag();
		// REPORT("%-40s - %8d - %8d", "sub_layer_level_present_flag", i, (bool)(subLayerLevelPresent[i]));
	}

	if (maxNumSubLayersMinus1 > 0) {
		for (uint32_t i = maxNumSubLayersMinus1; i < 8; ++i)
			ReadBits(2); // reserved_zero_2bits

		for (uint32_t i = 0; i < maxNumSubLayersMinus1; ++i) {
			if (subLayerProfilePresent[i])
				ParseProfileTierLevel(subLayerLevelPresent[i]); // sublayer[i] profile
		}
	}

	return true;
}

bool BaseDecoderHEVC::ParseProfileTierLevel(bool bLevelPresent) {
	uint32_t profileSpace = ReadBits(2);
	// REPORT("%-40s - %8d", "general_profile_space", profileSpace);
	bool bTier = ReadFlag();
	// REPORT("%-40s - %8d", "general_tier_flag", bTier);
	uint32_t profileIDC = ReadBits(5);
	// REPORT("%-40s - %8d", "general_profile_idc", profileIDC);
	bool bProfileCompatibility[32];

	for (uint32_t i = 0; i < 32; ++i) {
		bProfileCompatibility[i] = ReadFlag();
		// REPORT("%-40s - %8d - %8d", "general_profile_compatibility_flag", i, bProfileCompatibility[i]);
	}

	bool bProgressiveSource = ReadFlag();
	// REPORT("%-40s - %8d", "general_progressive_source_flag", bProgressiveSource);
	bool bInterlacedSource = ReadFlag();
	// REPORT("%-40s - %8d", "general_interlaced_source_flag", bInterlacedSource);
	bool bNonPackedConstraint = ReadFlag();
	// REPORT("%-40s - %8d", "general_non_packed_constraint_flag", bNonPackedConstraint);
	bool bFrameOnlyConstraint = ReadFlag();
	// REPORT("%-40s - %8d", "general_frame_only_constraint_flag", bFrameOnlyConstraint);

	uint32_t reserved44Top = ReadBits(32);
	uint32_t reserved44Bottom = ReadBits(12);

	if (bLevelPresent) {
		uint32_t levelIDC = ReadBits(8);
		// REPORT("%-40s - %8d", "general_level_idc", levelIDC);
	}

	return true;
}

bool BaseDecoderHEVC::ParseShortTermRefSet(uint32_t idx) {
	bool bInterRefPicSetPrediction = false;

	if (idx != 0) {
		bInterRefPicSetPrediction = ReadFlag();
		// REPORT("%-40s - %8d", "inter_ref_pic_set_prediction_flag", bInterRefPicSetPrediction);
	}

	uint32_t deltaIdxMinus1 = 0;
	if (bInterRefPicSetPrediction) {
		// printf("Unimplemented HEVC header parsing [short-term ref]");
		// throw std::runtime_error("Unimplemented HEVC header parsing [short-term ref]");

		// printf("ParseShortTermRefSet:[%d] (bInterRefPicSetPrediction)\n", idx);

		if (idx == m_activeSPS->m_numShortTermRefPicSets) {
			deltaIdxMinus1 = ReadUE(); // delta_idx_minus1
		}

		uint32_t delta_rps_sign = ReadBits(1); // delta_rps_sign
		// REPORT("%-40s - %8d", "delta_rps_sign", delta_rps_sign);
		uint32_t abs_delta_rps_minus1 = ReadUE(); // abs_delta_rps_minus1
		// REPORT("%-40s - %8d", "abs_delta_rps_minus1", abs_delta_rps_minus1);

		uint32_t RefRpsIdx = idx - (deltaIdxMinus1 + 1);
		int32_t deltaRps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);

		bool use_delta_flag[32] = {true};

		// REPORT("**** %4d **** %4d ****", deltaRps, m_activeSPS->m_NumDeltaPocs[RefRpsIdx]);

		for (uint32_t j = 0; j <= m_activeSPS->m_NumDeltaPocs[RefRpsIdx]; ++j) {
			m_activeSPS->m_used_by_curr_pic_flag[j] = ReadFlag();
			// REPORT("%-40s - %8d", "used_by_curr_pic_flag", m_activeSPS->m_used_by_curr_pic_flag[j]);
			if (!m_activeSPS->m_used_by_curr_pic_flag[j]) {
				use_delta_flag[j] = ReadFlag();
				// REPORT("%-40s - %8d", "use_delta_flag", use_delta_flag[j]);
			}
		}

		uint32_t u = 0;
		for (int32_t j = m_activeSPS->m_numPositivePics[RefRpsIdx] - 1; j >= 0; j--) {
			int32_t dpoc = m_activeSPS->m_DeltaPocS1[RefRpsIdx][j] + deltaRps;
			uint32_t kdx = m_activeSPS->m_numNegativePics[RefRpsIdx] + j;
			if (dpoc < 0 && use_delta_flag[kdx]) {
				m_activeSPS->m_DeltaPocS0[idx][u] = dpoc;
				m_activeSPS->m_UsedByCurrPicS0[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[kdx];
				u++;
			}
		}
		uint32_t fdx = m_activeSPS->m_NumDeltaPocs[RefRpsIdx];
		if (deltaRps < 0 && use_delta_flag[fdx]) {
			m_activeSPS->m_DeltaPocS0[idx][u] = deltaRps;
			m_activeSPS->m_UsedByCurrPicS0[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[fdx];
			u++;
		}
		for (uint32_t j = 0; j < m_activeSPS->m_numNegativePics[RefRpsIdx]; ++j) {
			int32_t dpoc = m_activeSPS->m_DeltaPocS0[RefRpsIdx][j] + deltaRps;
			if (dpoc < 0 && use_delta_flag[j]) {
				m_activeSPS->m_DeltaPocS0[idx][u] = dpoc;
				m_activeSPS->m_UsedByCurrPicS0[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[j];
				u++;
			}
		}
		m_activeSPS->m_numNegativePics[idx] = u;
		//==========================================================================================
		u = 0;
		for (int32_t j = m_activeSPS->m_numNegativePics[RefRpsIdx] - 1; j >= 0; j--) {
			int32_t dpoc = m_activeSPS->m_DeltaPocS0[RefRpsIdx][j] + deltaRps;
			if (dpoc > 0 && use_delta_flag[j]) {
				m_activeSPS->m_DeltaPocS1[idx][u] = dpoc;
				m_activeSPS->m_UsedByCurrPicS1[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[j];
				u++;
			}
		}
		fdx = m_activeSPS->m_NumDeltaPocs[RefRpsIdx];
		if (deltaRps > 0 && use_delta_flag[fdx]) {
			m_activeSPS->m_DeltaPocS1[idx][u] = deltaRps;
			m_activeSPS->m_UsedByCurrPicS1[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[fdx];
			u++;
		}
		for (uint32_t j = 0; j < m_activeSPS->m_numPositivePics[RefRpsIdx]; ++j) {
			uint32_t kdx = m_activeSPS->m_numNegativePics[RefRpsIdx] + j;
			int32_t dpoc = m_activeSPS->m_DeltaPocS1[RefRpsIdx][j] + deltaRps;
			if (dpoc > 0 && use_delta_flag[kdx]) {
				m_activeSPS->m_DeltaPocS1[idx][u] = dpoc;
				m_activeSPS->m_UsedByCurrPicS1[idx][u] = m_activeSPS->m_used_by_curr_pic_flag[kdx];
				u++;
			}
		}
		m_activeSPS->m_numPositivePics[idx] = u;
		m_activeSPS->m_NumDeltaPocs[idx] = m_activeSPS->m_numNegativePics[idx] + m_activeSPS->m_numPositivePics[idx];
		// REPORT("@@@@ %4d @@@@ %4d @@@@ 1", idx, m_activeSPS->m_NumDeltaPocs[idx]);
	} else {
		// printf("ParseShortTermRefSet:[%d] (!bInterRefPicSetPrediction)\n", idx);

		uint32_t numNegativePics = ReadUE(); // num_negative_pics
		// REPORT("%-40s - %8d", "num_negative_pics", numNegativePics);
		uint32_t numPositivePics = ReadUE(); // num_positive_pics
		// REPORT("%-40s - %8d", "num_positive_pics", numPositivePics);

		// Used to assist validation of implementation
		uint32_t deltaPosMinus1;
		bool bUsedByCurrPic;

		for (uint32_t i = 0; i < numNegativePics; ++i) {
			deltaPosMinus1 = ReadUE(); // delta_poc_s0_minus1[i]
			// REPORT("%-40s - %8d", "delta_poc_s0_minus1", deltaPosMinus1);
			bUsedByCurrPic = ReadFlag(); // used_by_curr_pic_s0_flag[i]
			// REPORT("%-40s - %8d", "used_by_curr_pic_s0_flag", bUsedByCurrPic);
			m_activeSPS->m_UsedByCurrPicS0[idx][i] = bUsedByCurrPic;
			if (i == 0)
				m_activeSPS->m_DeltaPocS0[idx][i] = -(int32_t)(deltaPosMinus1 + 1);
			else
				m_activeSPS->m_DeltaPocS0[idx][i] = m_activeSPS->m_DeltaPocS0[idx][i - 1] - (deltaPosMinus1 + 1);
		}

		for (uint32_t i = 0; i < numPositivePics; ++i) {
			deltaPosMinus1 = ReadUE(); // delta_poc_s1_minus1[i]
			// REPORT("%-40s - %8d", "delta_poc_s1_minus1", deltaPosMinus1);
			bUsedByCurrPic = ReadFlag(); // used_by_curr_pic_s1_flag[i]
			// REPORT("%-40s - %8d", "used_by_curr_pic_s1_flag", bUsedByCurrPic);
			m_activeSPS->m_UsedByCurrPicS1[idx][i] = bUsedByCurrPic;
			if (i == 0)
				m_activeSPS->m_DeltaPocS1[idx][i] = (deltaPosMinus1 + 1);
			else
				m_activeSPS->m_DeltaPocS1[idx][i] = m_activeSPS->m_DeltaPocS1[idx][i - 1] + (deltaPosMinus1 + 1);
		}
		m_activeSPS->m_numNegativePics[idx] = numNegativePics;
		m_activeSPS->m_numPositivePics[idx] = numPositivePics;
		m_activeSPS->m_NumDeltaPocs[idx] = numNegativePics + numPositivePics;
		// REPORT("@@@@ %4d @@@@ %4d @@@@ 0", idx, m_activeSPS->m_NumDeltaPocs[idx]);

		//		printf("ParseShortTermRefSet:[%d] - m_numPositivePics = %d m_numNegativePics = %d\n", idx,
		//		 m_activeSPS->m_numPositivePics[idx], m_activeSPS->m_numNegativePics[idx]);
	}

	return true;
}

bool BaseDecoderHEVC::ParseRefPicListsModification() { return true; }

bool BaseDecoderHEVC::ParsePredWeightTable() {
	bool bLumaWeightFlags[15] = {false};
	bool bChromaWeightFlags[15] = {false};

	uint32_t lumaLog2WeightDenom = ReadUE();

	if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
		ReadSE(); // delta_chroma_log2_weight_denom
	}

	for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL0ActiveMinus1; ++i) {
		bLumaWeightFlags[i] = ReadFlag(); // luma_weight_l0_flag[i]
	}

	if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
		for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL0ActiveMinus1; ++i) {
			bChromaWeightFlags[i] = ReadFlag(); // chroma_weight_l0_flag[i]
		}
	}

	for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL0ActiveMinus1; ++i) {
		if (bLumaWeightFlags[i]) {
			ReadSE(); // delta_luma_weight_l0[i]
			ReadSE(); // luma_offset_l0[i]
		}

		if (bChromaWeightFlags[i]) {
			for (uint32_t j = 0; j < 2; ++j) {
				ReadSE(); // delta_chroma_weight_l0[i][j]
				ReadSE(); // delta_chroma_offset_l0[i][j]
			}
		}
	}

	if (m_activeSlice.m_sliceType == SliceType::B) {
		memset(bLumaWeightFlags, 0, sizeof(bool) * 15);
		memset(bChromaWeightFlags, 0, sizeof(bool) * 15);

		for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL1ActiveMinus1; ++i) {
			bLumaWeightFlags[i] = ReadFlag();
		}

		if (m_activeSlice.m_sps->m_chromaArrayType != 0) {
			for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL1ActiveMinus1; ++i) {
				bChromaWeightFlags[i] = ReadFlag();
			}
		}

		for (uint32_t i = 0; i <= m_activeSlice.m_numRefIDXL1ActiveMinus1; ++i) {
			if (bLumaWeightFlags[i]) {
				ReadSE(); // delta_luma_weight_l1
				ReadSE(); // luma_offset_l1
			}

			if (bChromaWeightFlags[i]) {
				for (uint32_t j = 0; j < 2; ++j) {
					ReadSE(); // delta_chroma_weight_l1[i][j]
					ReadSE(); // delta_chroma_offset_l1[i][j]
				}
			}
		}
	}

	return true;
}

bool BaseDecoderHEVC::HandleScalingList() {
	for (uint32_t sizeId = 0; sizeId < 4; ++sizeId) {
		uint32_t idx = ((sizeId == 3) ? 2 : 6);
		for (uint32_t matrixId = 0; matrixId < idx; matrixId++) {
			m_activeSPS->m_scaling_list_pred_mode_flag[sizeId][matrixId] = ReadFlag();
			if (!m_activeSPS->m_scaling_list_pred_mode_flag[sizeId][matrixId])
				m_activeSPS->m_scaling_list_pred_matrix_id_delta[sizeId][matrixId] = ReadUE();
			else {
				uint32_t nextCoef = 8;
				uint8_t coefNum = (64 > (1 << (4 + (sizeId << 1)))) ? (1 << (4 + (sizeId << 1))) : 64; // min function
				if (sizeId > 1) {
					m_activeSPS->m_scaling_list_dc_coef_minus8[sizeId - 2][matrixId] = ReadSE();
					nextCoef = m_activeSPS->m_scaling_list_dc_coef_minus8[sizeId - 2][matrixId] + 8;
				}
				for (int i = 0; i < coefNum; ++i) {
					m_activeSPS->m_scaling_list_delta_coef = ReadSE();
					nextCoef = (nextCoef + m_activeSPS->m_scaling_list_delta_coef + 256) % 256;
					m_activeSPS->m_ScalingList[sizeId][matrixId][i] = nextCoef;
				}
			}
		}
	}

	return true;
}

void BaseDecoderHEVC::HandleSliceHeader() {
	HEVCNalType::Enum nalType = static_cast<HEVCNalType::Enum>(m_currentNalType);
	m_bNoRaslOutput = isIdr(nalType) || isBla(nalType) || m_bFirstPicture;
}

std::unique_ptr<BaseDecoder> CreateBaseDecoderHEVC() { return std::unique_ptr<BaseDecoder>{new BaseDecoderHEVC{}}; }

} // namespace utility
} // namespace vnova

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#endif
