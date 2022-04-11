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
// Buffer.cpp
//
// Implementation of underlying memory mechanism for packets * planes.
//
// This version uses a std::vector<uint8_t>
//
// The users of this will make read/write intentions clear using map/unmap, so there
// is the means to store the data in some other mappable store.
//
#include "Buffer.hpp"

#include "Diagnostics.hpp"

#include <cassert>
#include <cstring>
#include <stdlib.h>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace lctm {

//// Buffer
//
//
Buffer::Buffer() {}

Buffer::~Buffer() {}

//// BufferVector
//
// Default Buffer implementation that just uses a std::vector<>
//
class BufferVector : public Buffer {
public:
	~BufferVector() override {}

	BufferVector(const uint8_t *data, unsigned size) : bytes_(data, data + size) {}

	BufferVector(unsigned size) : bytes_(size, 0) {}

	void map_read(unsigned offset, unsigned size, const uint8_t *&mapped_data, unsigned &mapped_size) const override {
		assert(offset <= bytes_.size());
		assert(offset + size <= bytes_.size());

		mapped_data = bytes_.data() + offset;
		mapped_size = size;
	}

	void map_write(unsigned offset, unsigned size, uint8_t *&mapped_data, unsigned &mapped_size) override {
		assert(offset <= bytes_.size());
		assert(offset + size <= bytes_.size());

		mapped_data = bytes_.data() + offset;
		mapped_size = size;
	}

	void unmap() const override {}

private:
	std::vector<uint8_t> bytes_;
};

std::unique_ptr<Buffer> CreateBufferVector(const uint8_t *data, unsigned size) {
	return std::unique_ptr<Buffer>(new BufferVector(data, size));
}

std::unique_ptr<Buffer> CreateBufferVector(unsigned size) { return std::unique_ptr<Buffer>(new BufferVector(size)); }

//// BufferAligned
//
// Create page aligned buffers
//

class BufferAligned : public Buffer {
	const unsigned ALIGNMENT = 64;

public:
	BufferAligned(const uint8_t *data, unsigned size) : size_(size) {
		size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
#ifdef _WIN32
		bytes_ = _aligned_malloc(size, ALIGNMENT);
#else
		bytes_ = aligned_alloc(ALIGNMENT, size);
#endif
		memcpy(bytes_, data, size);
	}

	BufferAligned(unsigned size) : size_(size) {
		size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
#ifdef _WIN32
		bytes_ = _aligned_malloc(size, ALIGNMENT);
#else
		bytes_ = aligned_alloc(ALIGNMENT, size);
#endif
	}

	~BufferAligned() override {
		if (bytes_) {
#ifdef _WIN32
			_aligned_free(bytes_);
#else
			free(bytes_);
#endif
			bytes_ = nullptr;
		}
	}

	void map_read(unsigned offset, unsigned size, const uint8_t *&mapped_data, unsigned &mapped_size) const override {
		assert(offset <= size_);
		assert(offset + size <= size_);

		mapped_data = (uint8_t *)bytes_;
		mapped_size = size;
	}

	void map_write(unsigned offset, unsigned size, uint8_t *&mapped_data, unsigned &mapped_size) override {
		assert(offset <= size_);
		assert(offset + size <= size_);

		mapped_data = (uint8_t *)bytes_;
		mapped_size = size;
	}

	void unmap() const override {}

private:
	size_t size_ = 0;
	void *bytes_ = 0;
};

std::unique_ptr<Buffer> CreateBufferAligned(const uint8_t *data, unsigned size) {
	return std::unique_ptr<Buffer>(new BufferAligned(data, size));
}

std::unique_ptr<Buffer> CreateBufferAligned(unsigned size) { return std::unique_ptr<Buffer>(new BufferAligned(size)); }

} // namespace lctm
