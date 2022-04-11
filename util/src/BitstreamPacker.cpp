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
//
// BitstreamPacker.cpp
//
// Serialize variable bit width fields into a packet
//
// Use macro: BITSTREAM_DEBUG - define to !0 to get debug spew of bitfield operations
//

#include "BitstreamPacker.hpp"

#include "Diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <memory>

// global precompiler defines
#if BITSTREAM_DEBUG
#pragma message("BistreamPacker BITSTREAM TRACE ON")
#else
#pragma message("BistreamPacker BITSTREAM TRACE off")
#endif

namespace {
// Bit shifts that are defined for shift >= bit-width
//
inline uint32_t shl32(uint32_t v, unsigned c) { return (c < 32) ? (v << c) : 0; }
inline uint32_t shr32(uint32_t v, unsigned c) { return (c < 32) ? (v >> c) : 0; }

// 'width' bitmask
inline uint32_t mask32(unsigned width) { return (1 << width) - 1; }

const int BITS_PER_BYTE = 8;

const int INIITAL_SIZE = 128;

} // namespace

namespace lctm {

BitstreamPacker::BitstreamPacker() { data_.resize(INIITAL_SIZE); }

BitstreamPacker::~BitstreamPacker() {}

// Add accumulated bits to an existing packetbuilder
//
unsigned BitstreamPacker::emit(PacketBuilder &builder) {
	unsigned size = (bit_offset_ + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;

#if BITSTREAM_DEBUG
	fprintf(goBits, "========  ========  ========  ========  ========  ========  ========  ========  ========  ========  \n");
	fprintf(goBits, "BitstreamPacker::emit   (%8d)\n", size);
	fprintf(goBits, "%s", hex_dump(data_.data(), size, 0).c_str());
	fprintf(goBits, "========  ========  ========  ========  ========  ========  ========  ========  ========  ========  \n");
	fflush(goBits);
#endif

	builder.contents(data_.data(), size);
	return size;
}

// Build packet with accumulated bits
//
Packet BitstreamPacker::finish() {
	auto pb = Packet::build();
	emit(pb);
	return pb.finish();
}

/// write 0..32 bits into unsigned integer - with optional debug label
void BitstreamPacker::u(unsigned num_bits, uint32_t value) {
	assert(num_bits <= 32);

	value &= mask32(num_bits);

	while (num_bits) {
		// Index and bit
		const unsigned bit = bit_offset_ % BITS_PER_BYTE;
		unsigned idx = bit_offset_ / BITS_PER_BYTE;

		// Expand temp buffer as needed
		if (idx >= data_.size())
			data_.resize(data_.size() * 2, (uint8_t)0);

		// How many bits can be written into this byte?
		const unsigned n = std::min(BITS_PER_BYTE - bit, num_bits);

		// Insert bits into data
		const unsigned v = shr32(value, num_bits - n);
		data_[idx] |= shl32(v, BITS_PER_BYTE - (bit + n));

		bit_offset_ += n;
		num_bits -= n;
	}
}

void BitstreamPacker::u(unsigned nbits, uint32_t value, const char *label) {
#if BITSTREAM_DEBUG
	unsigned v = value & ((1 << nbits) - 1);
	char acString[128];
	sprintf(acString, "u(%2u, \"%s%s\")", nbits, join(context_name_, ".", true).c_str(), label);
	fprintf(goBits, "%-64s => %4u (0x%02x)  [%8d]\n", acString, v, v, bit_offset_);
	fflush(goBits);
	goStat.Update(acString, nbits);
#endif
	//	if(dumping()) {
	//
	//	}
	u(nbits, value);
}

// Write a sequence of bytes
void BitstreamPacker::bytes(const uint8_t *data, unsigned size) {
	assert((bit_offset_ % BITS_PER_BYTE) == 0);
	const unsigned idx = bit_offset_ / BITS_PER_BYTE;

	while (idx + size >= data_.size())
		data_.resize(data_.size() * 2, 0);

	for (unsigned i = 0; i < size; ++i)
		data_[idx + i] = data[i];

	bit_offset_ += size * 8;
}

void BitstreamPacker::bytes(const PacketView &view) { bytes(view.data(), view.size()); }

void BitstreamPacker::push_context_label(const std::string &name, bool dump) {
	context_name_.push_back(name);
	context_dump_.push_back(dump);
}

void BitstreamPacker::pop_context_label() {
	assert(!context_name_.empty());
	assert(!context_dump_.empty());
	context_name_.pop_back();
	context_dump_.pop_back();
}

} // namespace lctm
