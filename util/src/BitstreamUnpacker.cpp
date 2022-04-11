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
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//
// BitstreamUnpacker.cpp
//
// Deserialize variable bit width fields from a packet
//
// Use macro: BITSTREAM_DEBUG - define to !0 to get debug spew of bitfield operations
//

#include "BitstreamUnpacker.hpp"
#include "Config.hpp"
#include "Diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <memory>

// global precompiler defines
#if BITSTREAM_DEBUG
#pragma message("BistreamUnpacker BITSTREAM TRACE ON")
#else
#pragma message("BistreamUnpacker BITSTREAM TRACE off")
#endif

namespace {
// Bit shifts that are defined for shift >= bit-width
//
inline uint32_t shl32(uint32_t v, unsigned c) { return (c < 32) ? (v << c) : 0; }
inline uint32_t shr32(uint32_t v, unsigned c) { return (c < 32) ? (v >> c) : 0; }

// 'width' bitmask
inline uint32_t mask32(unsigned width) { return (1 << width) - 1; }

const int BITS_PER_BYTE = 8;
const int BITS_COUNT_MASK = 7;
const int BITS_COUNT_SHIFT = 3;
} // namespace

namespace lctm {

BitstreamUnpacker::BitstreamUnpacker(const PacketView &view) : view_(view) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "========  ========  ========  ========  ========  ========  ========  ========  ========  ========  \n");
	fprintf(goBits, "BitstreamUnpacker::emit    (%8d)\n", view.size());
	if (view.size() > 0)
		fprintf(goBits, "%s", hex_dump(view.data(), view.size(), 0).c_str());
	fprintf(goBits, "========  ========  ========  ========  ========  ========  ========  ========  ========  ========  \n");
	fflush(goBits);
#endif
}

BitstreamUnpacker::~BitstreamUnpacker() {}

// Read 0..32 bits into unsigned integer
//
uint32_t BitstreamUnpacker::u(unsigned num_bits) {
	assert(num_bits <= 32);
	assert((bit_offset_ + num_bits) <= bit_size());

	uint32_t r = 0;

	while (num_bits) {
		// Index and bit
#if defined __OPT_MODULO__
		const unsigned bit_used = bit_offset_ & BITS_COUNT_MASK;
#else
		const unsigned bit_used = bit_offset_ % BITS_PER_BYTE;
#endif
#if defined __OPT_DIVISION__
		unsigned idx = bit_offset_ >> BITS_COUNT_SHIFT;
#else
		unsigned idx = bit_offset_ / BITS_PER_BYTE;
#endif

		const unsigned data = view_.data()[idx];
		const unsigned bit_left = BITS_PER_BYTE - bit_used;

		// How many bits can be read from this byte?
		// const unsigned n = std::min(BITS_PER_BYTE - bit, num_bits);
		const unsigned n = LCEVC_MIN(bit_left, num_bits);

		// Extract bits and merge into result
		// r = shl32(r, n) | (shr32(data, (BITS_PER_BYTE - bit) - n) & mask32(n));
		r = (r << n) | ((data >> (bit_left - n)) & ((1 << n) - 1));

		bit_offset_ += n;
		num_bits -= n;
	}

	return r;
}

// Read a single of byte from stream - expects offset byte aligned
//
uint8_t BitstreamUnpacker::byte() {
#if defined __OPT_MODULO__
	assert((bit_offset_ & BITS_COUNT_MASK) == 0);
#else
	assert((bit_offset_ % BITS_PER_BYTE) == 0);
#endif
	assert(BITS_PER_BYTE <= (bit_size() - bit_offset_));
#if defined __OPT_DIVISION__
	const unsigned idx = bit_offset_ >> BITS_COUNT_SHIFT;
#else
	const unsigned idx = bit_offset_ / BITS_PER_BYTE;
#endif
	bit_offset_ += BITS_PER_BYTE;

//	CHECK(idx < view_.size());
	if(idx < view_.size()) {
		return view_.data()[idx];
	} else {
		WARN("Read beyond end of packet.");
		return 0;
	}
}

// Read a sequence of bytes from stream - expects offset byte aligned
//
Packet BitstreamUnpacker::bytes(unsigned num_bytes) {
#if defined __OPT_MODULO__
	assert((bit_offset_ & BITS_COUNT_MASK) == 0);
#else
	assert((bit_offset_ % BITS_PER_BYTE) == 0);
#endif
	assert(num_bytes * BITS_PER_BYTE <= (bit_size() - bit_offset_));
#if defined __OPT_DIVISION__
	const unsigned idx = bit_offset_ >> BITS_COUNT_SHIFT;
#else
	const unsigned idx = bit_offset_ / BITS_PER_BYTE;
#endif

	auto b = Packet::build().reserve(num_bytes);
	for (unsigned i = 0; i < num_bytes; ++i)
		b.data()[i] = view_.data()[idx + i];

	bit_offset_ += num_bytes * BITS_PER_BYTE;

	return b.finish();
}

//// Debug shim
//

// Read 0..32 bits into unsigned integer - includes a field label for tracing
//
uint32_t BitstreamUnpacker::u(unsigned num_bits, const char *label) {
	uint32_t r = u(num_bits);
#if BITSTREAM_DEBUG
	unsigned o = bit_offset_;
	char acString[128];
	sprintf(acString, "u(%2u, \"%s%s\")", num_bits, join(context_, ".", true).c_str(), label);
	fprintf(goBits, "%-64s => %4u (0x%02x)  [%8d]\n", acString, r, r, o);
	fflush(goBits);
	goStat.Update(acString, num_bits);
#endif
	return r;
}

void BitstreamUnpacker::push_context_label(const std::string &s) {
#if BITSTREAM_DEBUG
	context_.push_back(s);
#endif
}

void BitstreamUnpacker::pop_context_label() {
#if BITSTREAM_DEBUG
	assert(!context_.empty());
	context_.pop_back();
#endif
}

} // namespace lctm
