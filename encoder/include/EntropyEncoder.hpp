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
// EntropyEncoder.hpp
//

#pragma once

#include <utility>

#include "Component.hpp"
#include "EncodedLayer.hpp"
#include "EncoderConfiguration.hpp"
#include "Packet.hpp"
#include "Surface.hpp"

namespace lctm {

// A Pair of packets return from entropy coders: with and without prefix coding.
//
struct EncodedChunk {
	EncodedChunk() {}
	EncodedChunk(const Packet &r, const Packet &p) : raw(r), prefix(p) {}
	bool empty() const { return raw.empty() && prefix.empty(); }

	Packet raw;
	Packet prefix;
};

class EntropyEncoderResiduals : public Component {
public:
	EntropyEncoderResiduals() : Component("EntropyEncoderResiduals") {}

	// Encode in full frame raster order when coding units are not used (i.e no temporal and tile_mode=0)
	EncodedChunk process(const Surface &surface);

private:
};

class EntropyEncoderResidualsTiled : public Component {
public:
	EntropyEncoderResidualsTiled() : Component("EntropyEncoderResidualsTiled") {}

	// Encode in coding unit raster order when CUs are used (i.e temporal or tile_mode>0)
	EncodedChunk process(const Surface &surface, unsigned transform_block_size);

private:
};

class EntropyEncoderTemporal : public Component {
public:
	EntropyEncoderTemporal() : Component("EntropyEncoderTemporal") {}

	EncodedChunk process(const Surface &surface, unsigned transform_block_size, bool use_reduced_signalling);

private:
};

class EntropyEncoderFlags : public Component {
public:
	EntropyEncoderFlags() : Component("EntropyEncoderFlags") {}

	EncodedChunk process(const Surface &surface);

private:
};

class EntropyEncoderSizes : public Component {
public:
	EntropyEncoderSizes() : Component("EntropyEncoderSizes") {}

	EncodedChunk process(const Surface &surface, const std::vector<bool> entropy_enabled, const unsigned tile_idx,
	                     CompressionType compression_type);

private:
};

} // namespace lctm
