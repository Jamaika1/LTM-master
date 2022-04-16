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

#include "vvdec/CommonLib/BitStream.h"
#include "vvdec/CommonLib/ParameterSetManager.h"
#include "vvdec/DecoderLib/VLCReader.h"

// XXX Collision w/ VTM
#undef CHECK

#include "Misc.hpp"
#include "uBaseDecoderVVC.h"


using namespace lctm;
using namespace vvdec;

namespace vnova {
namespace utility {
namespace {


BaseDecNalUnitType::Enum uFromVVC(uint8_t type) {
	switch (type) {
	case VVCNalType::CODED_SLICE_TRAIL:
	case VVCNalType::CODED_SLICE_STSA:
	case VVCNalType::CODED_SLICE_RADL:
	case VVCNalType::CODED_SLICE_RASL:
	case VVCNalType::RESERVED_VCL_4:
	case VVCNalType::RESERVED_VCL_5:
	case VVCNalType::RESERVED_VCL_6:
	case VVCNalType::CODED_SLICE_IDR_W_RADL:
	case VVCNalType::CODED_SLICE_IDR_N_LP:
	case VVCNalType::CODED_SLICE_CRA:
	case VVCNalType::CODED_SLICE_GDR:
		return BaseDecNalUnitType::Slice;

	case VVCNalType::DCI:
		return BaseDecNalUnitType::Unknown;

	case VVCNalType::VPS:
		return BaseDecNalUnitType::VPS;
	case VVCNalType::SPS:
		return BaseDecNalUnitType::SPS;
	case VVCNalType::PPS:
		return BaseDecNalUnitType::PPS;

	case VVCNalType::PREFIX_APS:
	case VVCNalType::SUFFIX_APS:
	case VVCNalType::PH:
		return BaseDecNalUnitType::Unknown;

	case VVCNalType::ACCESS_UNIT_DELIMITER:
		return BaseDecNalUnitType::AUD;

	case VVCNalType::EOS:
		return BaseDecNalUnitType::EOS;

	case VVCNalType::EOB:
		return BaseDecNalUnitType::EOB;

	case VVCNalType::PREFIX_SEI:
	case VVCNalType::SUFFIX_SEI:
		return BaseDecNalUnitType::SEI;

	case VVCNalType::FD:
		return BaseDecNalUnitType::Filler;

	default:
		return BaseDecNalUnitType::Unknown;
	}
}

static const uint32_t kNalHeaderLength = 3;
static const uint8_t kNalHeader[kNalHeaderLength] = {0, 0, 1};
static const uint32_t kNalEscapeLength = 3;
static const uint8_t kNalEscape[kNalEscapeLength] = {0, 0, 3};

uint32_t uOffsetForNalUnitHeader(const uint8_t *nal) {
	if (memcmp(nal, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength;
	if (!nal[0] && memcmp(nal + 1, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength + 1;
	return 0;
}

} // namespace

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// VVC
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BaseDecoderVVC::BaseDecoderVVC() {
}

bool BaseDecoderVVC::ParseNalUnit(const uint8_t *nal, uint32_t nalLength) {
	bool bSuccess = true;

#if defined __BASE_VVC__
	// Account for AnnexB nal unit header ([0],0,0,1)
	uint32_t offset = uOffsetForNalUnitHeader(nal);

	m_nuhLayerId = nal[offset+0] & 0x3f;
	m_nalUnitType = ((nal[offset+1] >> 3) & 0x1F);
	m_temporalId = (nal[offset + 1] & 0x07) - 1;

	offset += 2;

	nal += offset;
	nalLength -= offset;

	InputBitstream bitstream;
	Unencapsulate(nal, nalLength, bitstream.getFifo());

	switch (m_nalUnitType) {
	case NAL_UNIT_VPS:
		bSuccess = parseVPS(bitstream);
		break;
	case NAL_UNIT_PPS:
		bSuccess = parsePPS(bitstream);
		break;
	case NAL_UNIT_SPS:
		bSuccess = parseSPS(bitstream);
		break;
	case NAL_UNIT_PH:
		bSuccess = parsePH(bitstream);
		break;
    case NAL_UNIT_PREFIX_APS:
    case NAL_UNIT_SUFFIX_APS:
		bSuccess = parseAPS(bitstream);
		break;

    case NAL_UNIT_CODED_SLICE_TRAIL:
    case NAL_UNIT_CODED_SLICE_STSA:
    case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
    case NAL_UNIT_CODED_SLICE_IDR_N_LP:
    case NAL_UNIT_CODED_SLICE_CRA:
    case NAL_UNIT_CODED_SLICE_GDR:
    case NAL_UNIT_CODED_SLICE_RADL:
    case NAL_UNIT_CODED_SLICE_RASL:
		bSuccess = parseSliceHeader(bitstream);
		break;

	case NAL_UNIT_EOS:
		break;
	default:
		break;
	}
#endif // defined __BASE_VVC__

	return bSuccess;
}

BaseDecPictType::Enum BaseDecoderVVC::GetBasePictureType() const {
	return m_basePictureType;
}

BaseDecNalUnitType::Enum BaseDecoderVVC::GetBaseNalUnitType() const {
	return uFromVVC(m_nalUnitType);
}

int32_t BaseDecoderVVC::GetQP() const { return 30;  }

uint32_t BaseDecoderVVC::GetNalType() const { return m_nalUnitType; }

uint32_t BaseDecoderVVC::GetPictureWidth() const {
	return m_width;
}

uint32_t BaseDecoderVVC::GetPictureHeight() const {
	return m_height;
}

bool BaseDecoderVVC::GetDPBCanRefresh() const {
	CHECK(0);
	return false;
}

uint32_t BaseDecoderVVC::GetBitDepthLuma() const {
	return m_bitdepthLuma;
}

uint32_t BaseDecoderVVC::GetBitDepthChroma() const {
	return m_bitdepthChroma;
}

uint32_t BaseDecoderVVC::GetChromaFormatIDC() const {
	return m_chromaFormatIDC;
}

int64_t BaseDecoderVVC::GetPictureOrderCount() const {
	return m_pictureOrderCount;
}

uint8_t BaseDecoderVVC::GetMaxNumberOfReorderFrames() const {
	CHECK(0);
	return 0;
}

uint32_t BaseDecoderVVC::GetFrameRate() const {
	return 30; // XXX DOn't think this is used anywhere
}

uint32_t BaseDecoderVVC::GetTemporalId() const {
	return m_temporalId;
}

bool BaseDecoderVVC::parseVPS(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	VPS *vps = new VPS();
	m_reader.setBitstream(&bitstream);
	m_reader.parseVPS(vps);
	m_parameterSetManager.storeVPS(vps, bitstream.getFifo());
#endif // defined __BASE_VVC__
	return true;
}

bool BaseDecoderVVC::parsePPS(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	PPS *pps = new PPS();
	m_reader.setBitstream(&bitstream);
	m_reader.parsePPS(pps, &m_parameterSetManager);
	pps->setLayerId( m_nuhLayerId );
	//pps->setTemporalId( m_temporalId );
	m_parameterSetManager.storePPS( pps, bitstream.getFifo() );
#endif // defined __BASE_VVC__
	return true;
}

bool BaseDecoderVVC::parseSPS(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	SPS *sps = new SPS();
	m_reader.setBitstream(&bitstream);
	m_reader.parseSPS(sps, &m_parameterSetManager);
	sps->setLayerId( m_nuhLayerId );
	m_parameterSetManager.storeSPS(sps, bitstream.getFifo());
#endif // defined __BASE_VVC__
	return true;
}

bool BaseDecoderVVC::parsePH(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	PPS *pps = new PPS();
	m_reader.setBitstream(&bitstream);
	m_reader.parsePictureHeader(m_picHeader.get(), &m_parameterSetManager, true );
	m_picHeader->setValid();
#endif // defined __BASE_VVC__
	return true;
}

bool BaseDecoderVVC::parseAPS(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	APS* aps = new APS();
	m_reader.setBitstream(&bitstream);
	m_reader.parseAPS(aps);
	aps->setTemporalId(m_temporalId);
	aps->setLayerId(m_nuhLayerId);
	aps->setHasPrefixNalUnitType(m_nalUnitType == NAL_UNIT_PREFIX_APS);
	m_parameterSetManager.storeAPS(aps, bitstream.getFifo());
#endif // defined __BASE_VVC__
	return true;
}

bool BaseDecoderVVC::parseSliceHeader(InputBitstream &bitstream) {
#if defined __BASE_VVC__
	Slice *slice = new Slice();
	slice->setPicHeader(m_picHeader.get());
	slice->initSlice();

	slice->setNalUnitType((NalUnitType)m_nalUnitType);
	slice->setNalUnitLayerId(m_nuhLayerId);
	slice->setTLayer(m_temporalId);

	m_reader.setBitstream(&bitstream);
	// m_reader.parseSliceHeader(slice, &m_picHeader, &m_parameterSetManager, m_prevTid0POC);
	bool m_bFirstSliceInPicture = true;
	m_reader.parseSliceHeader(slice, m_picHeader.get(), &m_parameterSetManager, m_prevTid0POC, m_pcParsePic, m_bFirstSliceInPicture);

	const PPS *pps = static_cast<const ParameterSetManager&>( m_parameterSetManager ).getPPS(m_apcSlicePilot->getPicHeader()->getPPSId());
	CHECK(pps);
	const SPS *sps = static_cast< const ParameterSetManager&>( m_parameterSetManager ).getSPS(pps->getSPSId());
	CHECK(sps);

	// Taking in consideration the conformance window
	m_width = pps->getPicWidthInLumaSamples() -
		sps->getWinUnitX(sps->getChromaFormatIdc()) *
		(pps->getConformanceWindow().getWindowLeftOffset() + pps->getConformanceWindow().getWindowRightOffset());
	m_height = pps->getPicHeightInLumaSamples() -
		sps->getWinUnitY(sps->getChromaFormatIdc()) *
		(pps->getConformanceWindow().getWindowTopOffset() + pps->getConformanceWindow().getWindowBottomOffset());

	m_bitdepthLuma = sps->getBitDepth(CHANNEL_TYPE_LUMA);
	m_bitdepthChroma = sps->getBitDepth(CHANNEL_TYPE_CHROMA);
	m_chromaFormatIDC = sps->getChromaFormatIdc();

	if (slice->isIDR())
		m_basePictureType = BaseDecPictType::IDR;
	else {
		switch (slice->getSliceType()) {
		case B_SLICE:
			m_basePictureType = BaseDecPictType::B;
			break;
		case P_SLICE:
			m_basePictureType = BaseDecPictType::P;
			break;
		case I_SLICE:
			m_basePictureType = BaseDecPictType::I;
			break;
		default:
			CHECK(0);
		}
	}

	m_pictureOrderCount = slice->getPOC();

	m_QP = slice->getSliceQp();

	if (slice->getTLayer() == 0 &&
		slice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RASL &&
		slice->getNalUnitType() != NAL_UNIT_CODED_SLICE_RADL) {
		m_prevTid0POC = slice->getPOC();
	}
#endif // defined __BASE_VVC__
	return true;
}

std::unique_ptr<BaseDecoder> CreateBaseDecoderVVC() { return std::unique_ptr<BaseDecoder>{new BaseDecoderVVC{}}; }

} // namespace utility
} // namespace vnova
