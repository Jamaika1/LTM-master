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
// HuffmanEncoder.hpp
//

#pragma once

#include <vector>

#include "BitstreamPacker.hpp"

namespace lctm {

class HuffmanEncoderBuilder;

class HuffmanEncoder {
public:
	static const int MAX_SYMBOL = 256;

	static HuffmanEncoderBuilder build();

	//  Write codes+lengths to bitstream
	void write_codes(BitstreamPacker &b);

	// Write a coded symbol to the bitstream
	void write_symbol(BitstreamPacker &b, unsigned symbol);

private:
	struct HuffmanCode {
		HuffmanCode(unsigned s, unsigned b) : symbol(s), bits(b), value(0) {}
		unsigned symbol;
		unsigned bits;
		unsigned value;
	};

	friend class HuffmanEncoderBuilder;

	HuffmanEncoder() {}
	HuffmanEncoder(const std::vector<HuffmanCode> &codes) : codes_(codes) {}

	std::vector<HuffmanCode> codes_;
};

class HuffmanEncoderBuilder {
public:
	// Accumulate symbol counts
	void add_symbol(unsigned symbol, unsigned count);

	// Resolve code length & values and make a new encoder - returns
	// total number of bits if symbols are encoded with given counts
	//
	HuffmanEncoder finish();

private:
	unsigned symbol_counts_[HuffmanEncoder::MAX_SYMBOL] = {0};
};

} // namespace lctm
