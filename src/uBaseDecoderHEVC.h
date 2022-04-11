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

#include "uTypes.h"
#include "uBaseDecoder.h"

#define HEVCMaxSPSCount 16
#define HEVCMaxPPSCount 64
#define HEVCMaxSubLayersCount 8

namespace vnova {
namespace utility {


struct HEVCNalType {
	enum Enum {
		CodedSlice_Trail_N = 0,
		CodedSlice_Trail_R = 1,
		CodedSlice_TSA_N = 2,
		CodedSlice_TLA_R = 3,
		CodedSlice_STSA_N = 4,
		CodedSlice_STSA_R = 5,
		CodedSlice_RADL_N = 6,
		CodedSlice_RADL_R = 7,
		CodedSlice_RASL_N = 8,
		CodedSlice_RASL_R = 9,
		RSV_VCL_N10 = 10,
		RSV_VCL_N12 = 12,
		RSV_VCL_N14 = 14,
		// Reserved (11,13,15)
		CodedSlice_BLA_W_LP = 16,
		CodedSlice_BLA_W_RADL = 17,
		CodedSlice_BLA_N_LP = 18,
		CodedSlice_IDR_W_RADL = 19,
		CodedSlice_IDR_N_LP = 20,
		CodedSlice_CRA = 21,
		RSV_IRAP_VCL22 = 22,
		RSV_IRAP_VCL23 = 23,
		// Reserved (22-31)
		VPS = 32,
		SPS = 33,
		PPS = 34,
		AUD = 35,
		EndOfSequence = 36,
		EndOfBitstream = 37,
		FillerData = 38,
		PrefixSEI = 39,
		SuffixSEI = 40
		// Reserved (41-47)
		// Unspecified (48-63)
	};
};


class BaseDecoderHEVC : public BaseDecoder {
public:
	BaseDecoderHEVC();

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
	struct SliceType {
		enum Enum { B = 0, P, I, Unknown };
	};

	struct SPS {
		SPS();

		uint32_t m_vpsID;
		uint32_t m_maxSubLayers;
		bool m_bTemporalIDNesting;
		uint32_t m_spsID;
		uint32_t m_chromaFormatIDC;
		bool m_bSeparateColourPlane;
		uint32_t m_chromaArrayType;
		uint32_t m_picWidthInLumaSamples;
		uint32_t m_picHeightInLumaSamples;
		bool m_bConformanceWindow;
		uint32_t m_confWinLeftOffset;
		uint32_t m_confWinRightOffset;
		uint32_t m_confWinTopOffset;
		uint32_t m_confWinBottomOffset;
		uint32_t m_bitDepthLuma;
		uint32_t m_bitDepthChroma;
		uint32_t m_log2MaxPicOrderCntLSB;
		bool m_bSubLayerOrderingInfoPresent;
		uint8_t m_maxNumReorderPics[HEVCMaxSubLayersCount];
		uint32_t m_log2MinLumaCodingBlockSize;
		uint32_t m_log2DiffMaxMinLumaCodingBlockSize;
		uint32_t m_log2MinLumaTransformBlockSize;
		uint32_t m_log2DiffMaxMinLumaTransformBlockSize;
		uint32_t m_maxTransformHierarchyDepthInter;
		uint32_t m_maxTransformHierarchyDepthIntra;
		bool m_bScalingListEnabled;
		bool m_bScalingListDataPresent;
		// scaling list
		bool m_bAMPEnabled;
		bool m_bSampleAdaptiveOffsetEnabled;
		bool m_bPCMEnabled;
		uint32_t m_pcmSampleBitDepthLuma;
		uint32_t m_pcmSampleBitDepthChroma;
		uint32_t m_log2MinPCMLumaCodingBlockSize;
		uint32_t m_log2DiffMaxMinPCMLumaCodingBlockSize;
		bool m_bPCMLoopFilterDisabled;
		uint32_t m_numShortTermRefPicSets;
		bool m_scaling_list_pred_mode_flag[4][6];
		uint32_t m_scaling_list_pred_matrix_id_delta[4][6];
		int32_t m_scaling_list_dc_coef_minus8[2][6];
		int32_t m_scaling_list_delta_coef;
		uint32_t m_ScalingList[4][6][256];
		// short term ref pic set
		bool m_bLongTermRefPicsPresent;
		uint32_t m_numLongTermRefPics;
		uint32_t m_longTermRefPicPOCLSB[32];
		bool m_bLongTermUsedByCurrentPic[32];
		bool m_bTemporalMVPEnabled;
		bool m_bStrongIntraSmoothingEnabled;
		bool m_bVUIParametersPresent;
		uint32_t m_NumDeltaPocs[65];
		uint32_t m_numPositivePics[65];
		uint32_t m_numNegativePics[65];
		int32_t m_DeltaPocS0[65][65];
		int32_t m_DeltaPocS1[65][65];
		bool m_UsedByCurrPicS0[65][65];
		bool m_UsedByCurrPicS1[65][65];
		bool m_used_by_curr_pic_flag[32];

		struct VUI {
			bool m_bTimingInfoPresent;
			uint32_t m_numUnitsInTick; ///< guarded by `m_bTimingInfoPresent`
			uint32_t m_timeScale;      ///< guarded by` m_bTimingInfoPresent`
		} m_vui;                       ///< guarded by `m_bVUIParametersPresent`
	};

	struct PPS {
		PPS();

		uint32_t m_ppsID;
		uint32_t m_spsID;
		bool m_bDependentSliceSegmentsEnabled;
		bool m_bOutputFlagPresent;
		uint32_t m_numExtraSliceHeaderBits;
		bool m_bSignDataHidingEnabled;
		bool m_bCabacInitPresent;
		uint32_t m_numRefIDXL0Active;
		uint32_t m_numRefIDXL1Active;
		int32_t m_initQP;
		bool m_bConstrainedIntraPred;
		bool m_bTransformSkipEnabled;
		bool m_bCUQPDeltaEnabled;
		uint32_t m_diffCUQPDeltaDepth;
		int32_t m_cbqpOffset;
		int32_t m_crqpOffset;
		bool m_bSliceChromaQPOffsetsPresent;
		bool m_bWeightedPred;
		bool m_bWeightedBipred;
		bool m_bTransquantBypassEnabled;
		bool m_bTilesEnabled;
		bool m_bEntropyCodingSyncEnabled;
		bool m_bPPSLoopFilterAcrossSlicesEnabled;
		bool m_bDeblockingFilterControlPresent;
		bool m_bDeblockingFilterOverrideEnabled;
		bool m_bPPSDeblockingFilterDisabled;
		int32_t m_ppsBetaOffsetDiv2;
		int32_t m_ppsTCOffsetDiv2;
		bool m_bPPSScalingListDataPresent;
		bool m_bListsModificationPresent;
		uint32_t m_log2ParallelMergeLevelMinus2;
		bool m_bSliceSegmentHeaderExtensionPresent;
	};

	struct SliceHeader {
		SliceHeader();

		bool m_bFirstSliceSegmentInPic;
		bool m_bNoOutputOfPriorPics;
		uint32_t m_ppsID;
		bool m_bDependentSliceSegment;
		uint32_t m_sliceSegmentAddress;
		uint32_t m_sliceType;
		bool m_bPicOutput;
		uint32_t m_colourPlaneID;
		uint32_t m_slicePicOrderCntLSB;
		uint32_t m_shortTermRefPicIdx;
		uint32_t m_numLongTermSPS;
		uint32_t m_numLongTermPics;
		bool m_bSliceSAOLuma;
		bool m_bSliceSAOChroma;
		bool m_bNumRefIdxActiveOverride;
		uint32_t m_numRefIDXL0ActiveMinus1;
		uint32_t m_numRefIDXL1ActiveMinus1;
		bool m_bMVDL1Zero;
		bool m_bCabacInit;
		bool m_bCollocatedFromL0;
		uint32_t m_collocatedRefIdx;
		uint32_t m_fiveMinusMaxNumMergeCand;
		int32_t m_sliceQPDelta;
		PPS *m_pps;
		SPS *m_sps;
	};

	struct PrevTid0Pic {
		uint32_t m_picOrderCntLsb;
		uint32_t m_picOrderCntMsb;
		uint32_t m_picOrderCntVal;
	};

	bool isSubLayerNonReferencePicture(HEVCNalType::Enum nut);
	bool isIdr(HEVCNalType::Enum nut);
	bool isBla(HEVCNalType::Enum nut);
	bool isRasl(HEVCNalType::Enum nut);
	bool isRadl(HEVCNalType::Enum nut);
	bool isIrap(HEVCNalType::Enum nut);
	bool ParseVPS();
	bool ParseSPS();
	bool ParsePPS();
	bool ParseSliceHeader();
	void HandleVUIParameters();
	bool ParseProfileTierLevels(bool bProfilePresent, uint32_t maxNumSubLayersMinus1);
	bool ParseProfileTierLevel(bool bLevelPresent);
	bool ParseShortTermRefSet(uint32_t idx);
	bool ParseRefPicListsModification();
	bool ParsePredWeightTable();
	void HandlePictureOrderCount();
	bool HandleScalingList();
	void HandleSliceHeader();
	void DecodePictureOrderCount();

	uint32_t m_currentNalType;
	uint32_t m_currentTemporalId;
	int64_t m_currentPictureOrderCount;

	bool m_bFirstPicture;
	bool m_bNoRaslOutput;
	SPS m_sps[HEVCMaxSPSCount];
	PPS m_pps[HEVCMaxPPSCount];
	SPS *m_activeSPS;
	PPS *m_activePPS;
	SliceHeader m_activeSlice;
	PrevTid0Pic m_prevTid0Pic;
};

} // namespace utility
} // namespace vnova
