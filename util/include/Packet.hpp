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
// Packet.hpp
//
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <cassert>

#include "Buffer.hpp"

namespace lctm {

class PacketBuilder;
class PacketView;
class Buffer;

class Packet {
public:
	Packet() = default;
	Packet(const Packet &packet) = default;
	Packet &operator=(const Packet &src) = default;

	// Create a new builder
	static PacketBuilder build();

	std::string name() const;
	uint64_t timestamp() const;
	uint64_t checksum() const;
	unsigned size() const;
	bool empty() const;

	// hex dump of packet contents
	std::string dump() const;

private:
	Packet(const std::string name, uint64_t timestamp, uint64_t checksum, const std::shared_ptr<Buffer> &buffer, unsigned offset,
	       unsigned size);

	friend PacketView;
	friend PacketBuilder;

	// Metadata
	std::string name_;
	uint64_t timestamp_ = 0;

	// Cached checksum of contents
	mutable uint64_t checksum_ = 0;
	mutable bool checksummed_ = false;

	// Underlying shared buffer
	std::shared_ptr<Buffer> buffer_;

	// Span within above buffer
	unsigned offset_ = 0;
	unsigned size_ = 0;
};

//// PacketView
//
// Scoped view into packet
//
class PacketView {
public:
	PacketView(const Packet &packet);
	~PacketView();

	const uint8_t *data() const {
		assert(mapped_data_);
		return mapped_data_;
	}
	unsigned size() const { return mapped_size_; }

private:
	friend PacketBuilder;

	const uint8_t *mapped_data_;
	unsigned mapped_size_;

	const Packet &packet_;
};

//// PacketBuilder
//
// Packet building object
//
class PacketBuilder {
public:
	PacketBuilder();

	// From a view into another packet
	PacketBuilder &contents(const PacketView &view, unsigned offset, unsigned size);

	// From existing data
	PacketBuilder &contents(const uint8_t *data, unsigned size);
	PacketBuilder &contents(const std::vector<uint8_t> data);

	// From new data
	PacketBuilder &reserve(unsigned size);

	// Set metadata
	PacketBuilder &name(const std::string &name);
	PacketBuilder &timestamp(uint64_t timestamp);
	void resize(unsigned size);

	// Make the packet
	Packet finish();

	// Write access to reserved bytes
	uint8_t *data() const;
	unsigned size() const;

private:
	// Accumulated packet
	Packet packet_;
	uint8_t *mapped_data_ = nullptr;
	unsigned mapped_size_ = 0;
};

} // namespace lctm
