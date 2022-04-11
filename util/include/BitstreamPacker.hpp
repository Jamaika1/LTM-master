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
//// BitstreamPacker.hpp
//
#pragma once

#include <cstdint>
#include <string>

#include "Config.hpp"
#include "Packet.hpp"

// global precompiler defines
#include "Misc.hpp"

// bitstream statistics
#include "BitstreamStatistic.hpp"
#include <stdio.h>

extern BitstreamStatistic goStat;
extern FILE *goBits;

namespace lctm {

class BitstreamPacker {
public:
	BitstreamPacker();

	// bitstream statistics
	~BitstreamPacker();

	// write 0..32 bits into unsigned integer - with optional debug label
	void u(unsigned nbits, uint32_t value);
	void u(unsigned nbits, uint32_t value, const char *label);

	// Write a sequence of bytes
	void bytes(const PacketView &bytes);
	void bytes(const uint8_t *data, unsigned size);

	unsigned bit_offset() const { return bit_offset_; }

	// unsigned byte_size() const { return (bit_offset_ + 7) / 8; }
	unsigned byte_size() const { return (bit_offset_ + 7) >> 3; }

	void push_context_label(const std::string &s, bool dump = false);
	void pop_context_label();

	class ScopedContextLabel {
	public:
		ScopedContextLabel(BitstreamPacker &b, const std::string &l) : b_(b) { b_.push_context_label(l); }

		~ScopedContextLabel() { b_.pop_context_label(); }

	private:
		BitstreamPacker &b_;
	};

	// Write accumulated data to the given packet builder
	unsigned emit(PacketBuilder &builder);

	// Build a packet with accumlated bits
	Packet finish();

private:
	// Current bit offset in destination
	unsigned bit_offset_ = 0;

	// Temp. buffer for packet data
	std::vector<uint8_t> data_;

	// Stack of context labels for debugging trace
	std::vector<std::string> context_name_;
	std::vector<bool> context_dump_;
};

} // namespace lctm
