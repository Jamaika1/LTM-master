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
//// EntropyDecoder.hpp
//
#pragma once

#include <utility>

#include "Component.hpp"
#include "Surface.hpp"
//#include "Packet.hpp"
#include "BitstreamUnpacker.hpp"
#include "SignaledConfiguration.hpp"
#include "Surface.hpp"

namespace lctm {

class SymbolSource;

class EntropyDecoderResidualsBase : public Component {

public:
	EntropyDecoderResidualsBase() : Component("EntropyDecoderResiduals") {}

protected:
	// State of the RLE residuals parser - used to select huffman tree
	enum { STATE_LSB, STATE_MSB, STATE_ZERO, STATE_COUNT };

	struct rle_pel_t {
		int16_t pel;
		unsigned zero_runlength;
	};

	rle_pel_t decode_pel(SymbolSource &source) const;
};

class EntropyDecoderResiduals : protected EntropyDecoderResidualsBase {

public:
	EntropyDecoderResiduals() {}

	// Decode per-surface data into plane of symbols when coding units are NOT used (i.e no temporal and tile_mode=0)
	Surface process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only, BitstreamUnpacker &b);

private:
};

class EntropyDecoderResidualsTiled : protected EntropyDecoderResidualsBase {

public:
	EntropyDecoderResidualsTiled() {}

	// Decode per-surface data into plane of symbols when coding units are used (i.e temporal or tile_mode>0)
	Surface process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only, BitstreamUnpacker &b,
	                unsigned transform_block_size);

private:
};

class EntropyDecoderTemporal : public Component {

	// State of the RLE temporal parser - used to select huffman tree
	enum { STATE_ZERO_RUN, STATE_ONE_RUN, STATE_COUNT };

public:
	EntropyDecoderTemporal() : Component("EntropyDecoderTemporal") {}

	// Decode per-surface data into plane of temporal flags
	//
	Surface process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only, BitstreamUnpacker &b,
	                unsigned transform_block_size, bool use_reduced_signalling);

private:
	unsigned decode_run(SymbolSource &source, bool symbol) const;
};

class EntropyDecoderFlags : public Component {

	enum { STATE_ZERO_RUN, STATE_ONE_RUN, STATE_COUNT };

public:
	EntropyDecoderFlags() : Component("EntropyDecoderFlags") {}

	// Decode per-surface data into plane of flags
	//
	Surface process(unsigned width, unsigned height, BitstreamUnpacker &b);

private:
	unsigned decode_run(SymbolSource &source, bool symbol) const;
};

class EntropyDecoderSizes : public Component {

	enum { STATE_LSB, STATE_MSB, STATE_COUNT };

public:
	EntropyDecoderSizes() : Component("EntropyDecoderSizes") {}

	// Decode per-surface data into plane of sizes
	//
	Surface process(unsigned width, unsigned height, BitstreamUnpacker &b, const std::vector<bool> entropy_enabled,
	                const unsigned tile_idx, CompressionType compression_type);

private:
	uint16_t decode_size(SymbolSource &source) const;
	int16_t decode_size_delta(SymbolSource &source) const;
};

} // namespace lctm
