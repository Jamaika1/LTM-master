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
// Contributors: Florian Maurer (florian.maurer@v-nova.com)

#include <cstring>

#include "uBaseDecoderYUV.h"
#include "Misc.hpp"

namespace vnova {
namespace utility {

namespace {

const uint32_t kNalHeaderLength = 3;
const uint8_t kNalHeader[kNalHeaderLength] = {0, 0, 1};

BaseDecNalUnitType::Enum uFromLCEVC(uint8_t type) {
	switch (type) {
	case LCEVCNalType::LCEVC_IDR:
	case LCEVCNalType::LCEVC_NonIDR:
		return BaseDecNalUnitType::Slice;
	case LCEVCNalType::Unspecified:
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

BaseDecoderYUV::BaseDecoderYUV() : m_currentNalTypeNumber(0), m_currentPictureOrderCountPlus1(0) {}

bool BaseDecoderYUV::ParseNalUnit(const uint8_t *nal, uint32_t nalLength) {
	bool bSuccess = true;

	// Account for AnnexB nal unit header ([0],0,0,1)
	uint32_t offset = uOffsetForNalUnitHeader(nal);

	// [forbidden-zero:1, forbidden-one:1, nal_type:5, reserved:9]
	m_currentNalTypeNumber = (nal[offset] & 0x3e) >> 1;
	if (m_currentNalTypeNumber == 28)
		m_currentNalType = LCEVCNalType::LCEVC_NonIDR;
	else if (m_currentNalTypeNumber == 29)
		m_currentNalType = LCEVCNalType::LCEVC_IDR;
	else if (m_currentNalTypeNumber < 28 || m_currentNalTypeNumber == 31)
		m_currentNalType = LCEVCNalType::Unspecified;
	else
		return false; // Reserved NAL Unit (30)

	if (m_currentNalType == LCEVCNalType::LCEVC_IDR || m_currentNalType == LCEVCNalType::LCEVC_NonIDR)
		m_currentPictureOrderCountPlus1++;

	offset += 1;

	return bSuccess;
}

uint8_t *BaseDecoderYUV::GetDataBuffer() const { return (uint8_t *)m_currentNalPayload.data(); }

BaseDecPictType::Enum BaseDecoderYUV::GetBasePictureType() const {
	if (m_currentNalType == LCEVCNalType::LCEVC_IDR)
		return BaseDecPictType::IDR;
	else
		return BaseDecPictType::Unknown;
}

BaseDecNalUnitType::Enum BaseDecoderYUV::GetBaseNalUnitType() const { return uFromLCEVC(m_currentNalType); }

int32_t BaseDecoderYUV::GetQP() const { return 0; }

uint32_t BaseDecoderYUV::GetNalType() const { return m_currentNalType; }

uint32_t BaseDecoderYUV::GetPictureWidth() const { return 0; }

uint32_t BaseDecoderYUV::GetPictureHeight() const { return 0; }

bool BaseDecoderYUV::GetDPBCanRefresh() const { return true; }

int64_t BaseDecoderYUV::GetPictureOrderCount() const { return m_currentPictureOrderCountPlus1 - 1; }

uint8_t BaseDecoderYUV::GetMaxNumberOfReorderFrames() const { return 0; }

uint32_t BaseDecoderYUV::GetFrameRate() const { return 0; }

uint32_t BaseDecoderYUV::GetBitDepthLuma() const { return 0; }

uint32_t BaseDecoderYUV::GetBitDepthChroma() const { return 0; }

uint32_t BaseDecoderYUV::GetChromaFormatIDC() const { return std::numeric_limits<uint32_t>::max(); }

uint32_t BaseDecoderYUV::GetTemporalId() const { return 0; }

std::unique_ptr<BaseDecoder> CreateBaseDecoderYUV() { return std::unique_ptr<BaseDecoder>{new BaseDecoderYUV{}}; }

} // namespace utility
} // namespace vnova
