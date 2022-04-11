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
// YUReader.hpp
//
#pragma once

#include "Image.hpp"
#include "Misc.hpp"

#include <memory>
#include <string>

namespace lctm {

class YUVReader {
public:
	// Information
	ImageDescription description() const { return image_description_; }
	unsigned length() const { return length_; }
	float rate() const { return rate_; }

	// Read into Image
	Image read(unsigned position, uint64_t timestamp = 0) const;
	void update_data(const ImageDescription &image_description);

private:
	friend std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name);
	friend std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name, unsigned rate);
	friend std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name, const ImageDescription &image_description,
	                                                  unsigned rate);

	YUVReader(const std::string &name, float rate, FILE *file, uintmax_t fileSize);
	YUVReader(const std::string &name, const ImageDescription &image_description, unsigned length, float rate, FILE *file,
	          uintmax_t fileSize);

	void set_position(unsigned position) const;

	std::string name_;
	uintmax_t fileSize_;

	ImageDescription image_description_;
	unsigned length_;
	float rate_;

	UniquePtrFile file_;

	mutable unsigned position_ = 0;
};

std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name);
std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name, unsigned rate);
std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name, const ImageDescription &image_description, unsigned rate);

} // namespace lctm
