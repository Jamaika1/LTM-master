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
#include "uBaseDecoder.h"

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

static uint32_t uOffsetForNalUnitHeader(const uint8_t *nal) {
	if (memcmp(nal, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength;
	if (!nal[0] && memcmp(nal + 1, kNalHeader, kNalHeaderLength) == 0)
		return kNalHeaderLength + 1;
	return 0;
}

} // namespace

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Base class
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BaseDecoder::BaseDecoder() : m_currentBits(0), m_remainingBits(8), m_byteOffset(0) {}

BaseDecoder::~BaseDecoder() {}

void BaseDecoder::Unencapsulate(const uint8_t *nalIn, uint32_t nalLength, DataBuffer &out) {
	out.resize(nalLength);

	const uint8_t *nalEnd = nalIn + nalLength;
	uint8_t *outPtr = out.data();

	while (nalIn < nalEnd - 3) {
		if (memcmp(nalIn, kNalEscape, kNalEscapeLength) == 0) {
			nalIn += 3;
			*outPtr++ = 0;
			*outPtr++ = 0;
		} else {
			*outPtr++ = *nalIn;
			nalIn++;
		}
	}

	// Appending remaining data.
	uint32_t remaining = static_cast<uint32_t>(nalEnd - nalIn);
	if (remaining) {
		memcpy(outPtr, nalIn, remaining);
		outPtr += remaining;
	}

	// Shrink to correct size.
	uint32_t newLength = static_cast<uint32_t>(outPtr - out.data());
	out.resize(newLength);
}

uint32_t BaseDecoder::ReadBits(uint8_t numBits) {
	assert(numBits <= 32);

	uint32_t res = 0;

	while (numBits) {
		if (m_remainingBits < numBits) {
			res <<= m_remainingBits;                         // Make room
			res |= (m_currentBits >> (8 - m_remainingBits)); // Push in the bits we want
			numBits -= m_remainingBits;                      // Reduce number of bits remaining to read
			m_byteOffset++;                                  // Move to next byte for next load

			if (m_byteOffset >= m_currentNalPayload.size()) // Safety.
			{
				INFO("ERROR -- NalParsing error, out of bytes to read from");
				throw std::runtime_error("NalParsing error, out of bytes to read from");
			}

			m_currentBits = *(m_currentNalPayload.data() + m_byteOffset); // Load in new byte
			m_remainingBits = 8;                                          // Reset remaining bits
		} else {
			res <<= numBits;                         // Make room
			res |= (m_currentBits >> (8 - numBits)); // Push in the bits we want
			m_currentBits <<= numBits;               // Update current bits
			m_remainingBits -= numBits;              // Reduce number of bits remaining to read
			numBits = 0;                             // Done.
		}
	}

	return res;
}

uint32_t BaseDecoder::ReadUE() {
	uint32_t digitCount = 0;
	uint32_t bit = ReadBits(1);
	uint32_t res = 0;

	while (bit == 0) {
		digitCount += 1;
		bit = ReadBits(1);
	}

	res = bit;

	while (digitCount > 0) {
		digitCount -= 1;
		res <<= 1;
		res |= ReadBits(1);
	}

	return res - 1;
}

int32_t BaseDecoder::ReadSE() {
	uint32_t ue = ReadUE();

	// Even UE are negative.
	// Odd are positive.

	if (ue & 0x01) {
		return static_cast<int32_t>((ue + 1) >> 1);
	} else {
		// Negate, and on even boundary so no need for the addition.
		return -static_cast<int32_t>(ue >> 1);
	}
}

bool BaseDecoder::ReadFlag() {
	uint32_t bit = ReadBits(1);
	return (bit != 0);
}

bool BaseDecoder::ByteAligned() const {
	return (m_remainingBits % 8) == 0;
}

std::unique_ptr<BaseDecoder> CreateBaseDecoder(BaseDecoder::Codec base_codec) {
	extern std::unique_ptr<BaseDecoder> CreateBaseDecoderAVC();
	extern std::unique_ptr<BaseDecoder> CreateBaseDecoderHEVC();
	extern std::unique_ptr<BaseDecoder> CreateBaseDecoderVVC();
	extern std::unique_ptr<BaseDecoder> CreateBaseDecoderEVC();
	extern std::unique_ptr<BaseDecoder> CreateBaseDecoderYUV();

	switch (base_codec) {
	case BaseDecoder::AVC:
		return std::unique_ptr<BaseDecoder>(CHECK(CreateBaseDecoderAVC()));
	case BaseDecoder::HEVC:
		return std::unique_ptr<BaseDecoder>(CHECK(CreateBaseDecoderHEVC()));
	case BaseDecoder::VVC:
		return std::unique_ptr<BaseDecoder>(CHECK(CreateBaseDecoderVVC()));
	case BaseDecoder::EVC:
		return std::unique_ptr<BaseDecoder>(CHECK(CreateBaseDecoderEVC()));
	case BaseDecoder::None:
		return std::unique_ptr<BaseDecoder>(CHECK(CreateBaseDecoderYUV()));
	default:
		CHECK(0);
		return nullptr;
	}
}

} // namespace utility
} // namespace vnova

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#endif
