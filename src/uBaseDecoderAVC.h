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

#define AVCMaxSPSCount 32
#define AVCMaxPPSCount 256
#define AVCMaxSliceGroupIDs 8
#define AVCMaxShortTermRefPicSets 64
#define AVCNumMMCO 7

namespace vnova {
namespace utility {

struct AVCNalType {
	enum Enum {
		Unspecified = 0,
		CodedSlice_NonIDR = 1,
		CodedSlice_A = 2,
		CodedSlice_B = 3,
		CodedSlice_C = 4,
		CodedSlice_IDR = 5,
		SEI = 6,
		SPS = 7,
		PPS = 8,
		AUD = 9,
		EndOfSequence = 10,
		EndOfStream = 11,
		FillerData = 12,
		SPSExt = 13,
		Prefix = 14,
		SubsetSPS = 15,
		DPS = 16,
		// Reserved (17-18)
		CodedSlice_Aux = 19,
		CodedSlice_Ext = 20,
		CodedSlice_DepthExt = 21
		// Reserved (22-23)
		// Unspecified (24-31)
	};
};

class BaseDecoderAVC : public BaseDecoder {
public:
	BaseDecoderAVC();

	bool ParseNalUnit(const uint8_t *nal, uint32_t nalLength);
	uint8_t *GetDataBuffer() const;
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
	int64_t GetPictureOrderCountIncrement() const { return 2; }

private:
	struct SliceType {
		enum Enum { P = 0, B, I, SP, SI, Unknown };
	};

	struct SPS {
		SPS();

		uint8_t m_profileIDC;
		bool m_bConstraintSet0;
		bool m_bConstraintSet1;
		bool m_bConstraintSet2;
		bool m_bConstraintSet3;
		bool m_bConstraintSet4;
		bool m_bConstraintSet5;
		uint8_t m_levelIDC;
		uint32_t m_spsID;
		uint32_t m_chromaFormatIDC;
		bool m_bSeparateColourPlane;
		uint32_t m_chromaArrayType;
		uint32_t m_bitDepthLuma;
		uint32_t m_bitDepthChroma;
		bool m_bQPPrimeYZeroTransformBypass;
		uint32_t m_log2MaxFrameNum;
		uint32_t m_picOrderCntType;
		uint32_t m_log2MaxPicOrderCntLSB;
		bool m_bDeltaPicOrderAlwaysZero;
		int32_t m_offsetForNonRefPic;
		int32_t m_offsetForTopToBottomField;
		uint32_t m_numRefFramesInPicOrderCntCycle;
		int32_t m_offsetForRefFrame[256];
		uint32_t m_numRefFrames;
		bool m_bGapsInFrameNumValueAllowed;
		uint32_t m_picWidthInMBs;
		uint32_t m_picHeightInMapUnits;
		bool m_bFrameMBsOnly;
		bool m_bMBAdaptiveFrameField;
		bool m_bDirect8x8Inference;
		bool m_bFrameCropping;
		uint32_t m_frameCropLeftOffset;
		uint32_t m_frameCropRightOffset;
		uint32_t m_frameCropTopOffset;
		uint32_t m_frameCropBottomOffset;
		bool m_bVUIParametersPresent;

		struct VUI {
			bool m_bTimingInfoPresent;
			uint32_t m_numUnitsInTick; ///< guarded by `m_bTimingInfoPresent`
			uint32_t m_timeScale;      ///< guarded by` m_bTimingInfoPresent`

			uint8_t m_maxNumReorderFrames;
		} m_vui; ///< guarded by `m_bVUIParametersPresent`
	};

	struct PPS {
		PPS();

		uint32_t m_ppsID;
		uint32_t m_spsID;
		bool m_bEntropyCodingMode;
		bool m_bBottomFieldPicOrderInFramePresent;
		uint32_t m_numSliceGroups;
		uint32_t m_sliceGroupMapType;
		// slice_group_map_type == 0
		uint32_t m_runLength[AVCMaxSliceGroupIDs];
		// slice_group_map_type == 2
		uint32_t m_topLeft[AVCMaxSliceGroupIDs];
		uint32_t m_bottomRight[AVCMaxSliceGroupIDs];
		// slice_group_map_type == (3 || 4 || 5)
		bool m_bSliceGroupChangeDirection;
		uint32_t m_sliceGroupChangeRate;
		// end_type
		uint32_t m_numRefIDXL0Active;
		uint32_t m_numRefIDXL1Active;
		bool m_bWeightedPred;
		uint8_t m_weightedBipredIDC;
		int32_t m_picInitQP;
		int32_t m_picInitQS;
		int32_t m_chromaQPIndexOffset;
		bool m_bDeblockingFilterControlPresent;
		bool m_bConstraintedIntraPred;
		bool m_bRedundantPicCntPresent;
	};

	struct SliceHeader {
		SliceHeader();

		SliceType::Enum GetType() const;

		bool m_bIDR;
		uint32_t m_firstMBInSlice;
		SliceType::Enum m_sliceType;
		uint32_t m_ppsID;
		uint8_t m_colourPlaneID;
		uint32_t m_frameNum;
		bool m_bFieldPic;
		bool m_bBottomField;
		uint32_t m_idrPicID;
		uint32_t m_picOrderCntLSB;
		int32_t m_deltaPicOrderCntBottom;
		int32_t m_deltaPicOrderCnt[2];
		uint32_t m_redundantPicCnt;
		bool m_bDirectSpatialMVPred;
		bool m_bNumRefIDXActiveOverrideFlag;
		uint32_t m_numRefIDXL0Active;
		uint32_t m_numRefIDXL1Active;
		uint32_t m_cabacInitIDC;
		int32_t m_sliceQPDelta;
		bool m_bSPForSwitch;
		int32_t m_sliceQSDelta;
		uint32_t m_disableDeblockingFilterIDC;
		int32_t m_sliceAlphaC0OffsetDiv2;
		int32_t m_sliceBetaOffsetDiv2;
		bool m_memManagementControlOp[AVCNumMMCO];
		PPS *m_pps;
		SPS *m_sps;
	};

	struct Level {
		enum Enum {
			Number_1b = 9, // Special case
			Number_1 = 10,
			Number_1_1 = 11,
			Number_1_2 = 12,
			Number_1_3 = 13,
			Number_2 = 20,
			Number_2_1 = 21,
			Number_2_2 = 22,
			Number_3 = 30,
			Number_3_1 = 31,
			Number_3_2 = 32,
			Number_4 = 40,
			Number_4_1 = 41,
			Number_4_2 = 42,
			Number_5 = 50,
			Number_5_1 = 51,
			Number_5_2 = 52,
			Number_6 = 60,
			Number_6_1 = 61,
			Number_6_2 = 62
		};

		struct Limit {
			uint32_t m_maxDpbMbs;
		};

		const static std::map<Enum, Limit> limits;
	};

	struct RefPic {
		RefPic();

		SliceHeader m_header;
		uint32_t m_topFieldOrderCnt;
		uint32_t m_bottomFieldOrderCnt;
		uint32_t m_picOrderCntMsb;
		uint32_t m_frameNum;
		uint32_t m_frameNumOffset;
	};

	bool ParseSPS();
	bool ParsePPS();
	bool ParseSliceHeader();
	void HandleVUIParameters();
	void HandleHRDParameters();
	void HandlePictureOrderCount();
	bool HandleRefPicListReordering();
	bool HandlePredWeightTable();
	bool HandleDecRefPicMarking();
	bool HandleScalingList(uint32_t loopCount);
	bool HandleScalingListElement(uint32_t listSize);
	const Level::Limit *GetLevelLimit(SPS *sps) const;

	uint8_t m_currentNalRefIDC;
	uint32_t m_currentNalType;
	int64_t m_currentPictureOrderCount;

	SPS m_sps[AVCMaxSPSCount];
	PPS m_pps[AVCMaxPPSCount];
	SPS *m_activeSPS;
	PPS *m_activePPS;
	SliceHeader m_activeSlice; // @todo: Do we need to store history?
	RefPic m_refPic;
};

} // namespace utility
} // namespace vnova
