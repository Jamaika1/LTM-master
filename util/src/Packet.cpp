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
//// Packet.cpp
//
#include "Packet.hpp"

#include "Diagnostics.hpp"
#include "Misc.hpp"

#include <cassert>

namespace lctm {

//
//
Packet::Packet(const std::string name, uint64_t timestamp, uint64_t checksum, const std::shared_ptr<Buffer> &buffer,
               unsigned offset, unsigned size)
    : name_(name), timestamp_(timestamp), checksum_(checksum), buffer_(buffer), offset_(offset), size_(size) {}

std::string Packet::name() const { return name_; }
uint64_t Packet::timestamp() const { return timestamp_; }
unsigned Packet::size() const { return size_; }
bool Packet::empty() const { return !buffer_; }

PacketBuilder Packet::build() { return PacketBuilder(); }

// Lazy checksum of packet contents
//
uint64_t Packet::checksum() const {
	if (!checksummed_) {
		PacketView v(*this);
		checksum_ = crc64(0, v.data(), v.size());
		checksummed_ = true;
	}
	return checksum_;
}

// hex dump of packet contents
//
std::string Packet::dump() const {
	PacketView v(*this);
	return hex_dump(v.data(), v.size(), 0);
}

//// PacketBuilder
//
//
PacketBuilder::PacketBuilder() : packet_{"", 0, 0, 0, 0, 0} {}

// From a view into another packet
PacketBuilder &PacketBuilder::contents(const PacketView &view, unsigned offset, unsigned size) {

	// Start with all the same metadata
	packet_ = view.packet_;

	assert(offset <= packet_.size());
	assert(offset + size <= packet_.size());

	// Adjust this offset and size
	packet_.offset_ += offset;
	packet_.size_ = size;

	return *this;
}

PacketBuilder &PacketBuilder::contents(const uint8_t *data, unsigned size) {
	packet_.buffer_ = CreateBufferVector(data, size);
	packet_.offset_ = 0;
	packet_.size_ = size;
	return *this;
}

PacketBuilder &PacketBuilder::contents(const std::vector<uint8_t> v) { return contents(v.data(), (unsigned)v.size()); }

PacketBuilder &PacketBuilder::reserve(unsigned size) {
	packet_.buffer_ = std::shared_ptr<Buffer>(CreateBufferVector(size));
	packet_.offset_ = 0;
	packet_.size_ = size;

	packet_.buffer_->map_write(packet_.offset_, packet_.size_, mapped_data_, mapped_size_);
	return *this;
}

uint8_t *PacketBuilder::data() const {
	assert(mapped_data_ != nullptr);

	return mapped_data_;
}
unsigned PacketBuilder::size() const {
	assert(mapped_data_ != nullptr);

	return mapped_size_;
}

PacketBuilder &PacketBuilder::name(const std::string &name) {
	packet_.name_ = name;
	return *this;
}

PacketBuilder &PacketBuilder::timestamp(uint64_t timestamp) {
	packet_.timestamp_ = timestamp;
	return *this;
}

void PacketBuilder::resize(unsigned size) {
	assert(mapped_data_ != nullptr);

	packet_.buffer_ = std::shared_ptr<Buffer>(CreateBufferVector(mapped_data_, size));
	packet_.size_ = size;
	packet_.offset_ = 0;
	packet_.buffer_->map_write(packet_.offset_, packet_.size_, mapped_data_, mapped_size_);
}

Packet PacketBuilder::finish() {
	if (mapped_data_) {
		packet_.buffer_->unmap();
		mapped_data_ = nullptr;
		mapped_size_ = 0;
	}
	return packet_;
}

//// PacketView
//
//
PacketView::PacketView(const Packet &packet) : packet_(packet) {
	if (packet_.buffer_) {
		packet_.buffer_->map_read(packet_.offset_, packet_.size_, mapped_data_, mapped_size_);
	} else {
		mapped_data_ = nullptr;
		mapped_size_ = 0;
	}
}

PacketView::~PacketView() {
	if (packet_.buffer_) {
		packet_.buffer_->unmap();
	}
}

} // namespace lctm
