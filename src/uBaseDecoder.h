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

#include <map>
#include <memory>
#include <vector>

namespace vnova {
namespace utility {

struct BaseDecPictType {
	enum Enum { IDR = 0, I, P, B, BR, Unknown };

	static const char *ToString(BaseDecPictType::Enum type) {
		switch (type) {
		case BaseDecPictType::IDR:
			return "IDR";
		case BaseDecPictType::I:
			return "I";
		case BaseDecPictType::P:
			return "P";
		case BaseDecPictType::B:
			return "B";
		case BaseDecPictType::BR:
			return "BRef";
		default:
			return "Unknown";
		}
	}
};

struct BaseDecNalUnitType {
	enum Enum { Slice = 0, SEI, SPS, PPS, AUD, Filler, VPS, EOS, EOB, Unknown };

	static const char *ToString(BaseDecNalUnitType::Enum type) {
		switch (type) {
		case BaseDecNalUnitType::Slice:
			return "Slice";
		case BaseDecNalUnitType::SEI:
			return "SEI";
		case BaseDecNalUnitType::SPS:
			return "SPS";
		case BaseDecNalUnitType::PPS:
			return "PPS";
		case BaseDecNalUnitType::AUD:
			return "AUD";
		case BaseDecNalUnitType::Filler:
			return "Filler";
		case BaseDecNalUnitType::VPS:
			return "VPS";
		case BaseDecNalUnitType::EOS:
			return "EOS";
		case BaseDecNalUnitType::EOB:
			return "EOS";
		default:
			return "Unknown";
		}
	}
};

// How are NAL units delimited - marker vs. length
enum NALDelimitier {
	NALDelimiterNone = 0,
	NALDelimiterMarker,
	NALDelimiterU32Length
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class BaseDecoder {
public:
	enum Codec { None, AVC, HEVC, VVC, EVC };

	BaseDecoder();
	virtual ~BaseDecoder();

	static void Unencapsulate(const uint8_t *nalIn, uint32_t nalLength, DataBuffer &out);

	/// Accounts for AnnexB nal unit header ([0], 0, 0, 1), otherwise the address & length should represent to the actual nal unit
	/// data.
	virtual bool ParseNalUnit(const uint8_t *nal, uint32_t nalLength) = 0;
	virtual BaseDecPictType::Enum GetBasePictureType() const = 0;
	virtual BaseDecNalUnitType::Enum GetBaseNalUnitType() const = 0;
	virtual int32_t GetQP() const = 0;
	virtual uint32_t GetNalType() const = 0;
	virtual int64_t GetPictureOrderCount() const = 0;
	virtual uint32_t GetPictureWidth() const = 0;
	virtual uint32_t GetPictureHeight() const = 0;
	virtual bool GetDPBCanRefresh() const = 0;
	virtual uint8_t GetMaxNumberOfReorderFrames() const = 0;
	virtual uint32_t GetFrameRate() const = 0; ///< Returns 0 if not available
	virtual uint32_t GetBitDepthLuma() const = 0;
	virtual uint32_t GetBitDepthChroma() const = 0;
	virtual uint32_t GetChromaFormatIDC() const = 0;
	virtual uint32_t GetTemporalId() const = 0;
	virtual NALDelimitier Delimiter() const = 0;
	virtual int64_t GetPictureOrderCountIncrement() const = 0;

protected:
	uint32_t ReadBits(uint8_t numBits);
	uint32_t ReadUE();
	int32_t ReadSE();
	bool ReadFlag();
	bool ByteAligned() const;

	std::vector<uint8_t> m_currentNalPayload;
	uint8_t m_currentBits;
	uint8_t m_remainingBits;
	uint32_t m_byteOffset;
};

std::unique_ptr<BaseDecoder> CreateBaseDecoder(BaseDecoder::Codec base_codec);

} // namespace utility
} // namespace vnova
