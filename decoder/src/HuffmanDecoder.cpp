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
// HuffmanDecoder.cpp
//

#include "HuffmanDecoder.hpp"

#include "Config.hpp"
#include "Diagnostics.hpp"

#include <algorithm>

namespace lctm {

// Number of bits used to store codes
//
unsigned HuffmanDecoder::bit_width(unsigned n) {
	// clang-format off
	static const uint8_t table[32] = {
		// Figure 17 & 18: out = log2(max_length - min_length + 1);
		1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, // 0   - 15
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 16  - 31
	};
	// clang-format on

	CHECK(n < 32);

	return table[n];
}

// Read a huffman tree from bitstream into state
//
void HuffmanDecoder::read_codes(BitstreamUnpacker &b) {
	BitstreamUnpacker::ScopedContextLabel label(b, "entropy_code");

	const unsigned min_code_length = b.u(5, "min_code_length");
	const unsigned max_code_length = b.u(5, "max_code_length");

	if (min_code_length == 31 && max_code_length == 31) {
		// Empty table
		return;
	}

	if (min_code_length == 0 && max_code_length == 0) {
		// Single code
		single_symbol_ = b.u(8, "single_symbol");
		return;
	}

	const unsigned length_bits = bit_width(max_code_length - min_code_length);

	if (b.u(1, "presence_bitmap")) {
		// Symbols are described by a 'presence' bitmap over all possible symbols
		for (unsigned symbol = 0; symbol < 256; ++symbol) {
			BitstreamUnpacker::ScopedContextLabel label(b, "symbol");

			if (b.u(1, "presence"))
				codes_.emplace_back(symbol, b.u(length_bits, "length") + min_code_length);
		}
	} else {
		// Sparse symbols: count * (symbol, length}
		const unsigned count = b.u(5, "count");
		for (unsigned i = 0; i < count; ++i) {
			const unsigned symbol = b.u(8, "symbol");
			codes_.emplace_back(symbol, b.u(length_bits, "length") + min_code_length);
		}
	}

	if (codes_.empty())
		return;

	//// Generate the encoded values - canonical huffman coding
	//
	// Sort codes by ( decending coded length, ascending symbol)
	//
	std::sort(codes_.begin(), codes_.end(), [](const HuffmanCode &a, const HuffmanCode &b) {
		if (a.bits == b.bits)
			return a.symbol > b.symbol;
		else
			return a.bits < b.bits;
	});

	// Number the codes
	//
	unsigned current_length = max_code_length;
	unsigned current_value = 0;

	for (auto c = codes_.rbegin(); c != codes_.rend(); ++c) {
		if (c->bits < current_length) {
			current_value >>= (current_length - c->bits);
			current_length = c->bits;
		}
		c->value = current_value++;
	}
}

unsigned HuffmanDecoder::decode_symbol(BitstreamUnpacker &b) {
	BitstreamUnpacker::ScopedContextLabel label(b, "entropy_symb");

	if (codes_.empty())
		return single_symbol_;

	// Codes are sorted by increaing bit-length
	//
	// Check each code, adding bits to current value as necessary a match is found
	//
	unsigned bits = 0;
	unsigned value = 0;
	for (auto &c : codes_) {
		while (bits < c.bits) {
			value = (value << 1) | b.u(1);
			++bits;
		}
		if (value == c.value) {
#if BITSTREAM_DEBUG
			char acString[128];
			sprintf(acString, "u(%2u, \"%s\")", bits, "entropy_symb.codebits");
			fprintf(goBits, "%-64s => %4u (0x%02x)  [%8d]\n", acString, value, value, b.bit_offset());
			fflush(goBits);
			goStat.Update(acString, bits);
#endif
			c.count++;
			return c.symbol;
		}
	}

	// Failed to find matching symbol - something is wrong
	CHECK(0);
	return 0;
}

} // namespace lctm
