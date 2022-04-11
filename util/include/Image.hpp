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
// Image.hpp
//
#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "Surface.hpp"

namespace lctm {

// Various image pixel formats
//
// NB: depth operations rely on the values of these enums
enum ImageFormat {
	IMAGE_FORMAT_NONE = 0,

	IMAGE_FORMAT_YUV420P8 = 1,
	IMAGE_FORMAT_YUV422P8 = 2,
	IMAGE_FORMAT_YUV444P8 = 3,
	IMAGE_FORMAT_Y8 = 4,

	IMAGE_FORMAT_YUV420P10 = 5,
	IMAGE_FORMAT_YUV422P10 = 6,
	IMAGE_FORMAT_YUV444P10 = 7,
	IMAGE_FORMAT_Y10 = 8,

	IMAGE_FORMAT_YUV420P12 = 9,
	IMAGE_FORMAT_YUV422P12 = 10,
	IMAGE_FORMAT_YUV444P12 = 11,
	IMAGE_FORMAT_Y12 = 12,

	IMAGE_FORMAT_YUV420P14 = 13,
	IMAGE_FORMAT_YUV422P14 = 14,
	IMAGE_FORMAT_YUV444P14 = 15,
	IMAGE_FORMAT_Y14 = 16,

	// Used for internal planes
	IMAGE_FORMAT_YUV420P16 = 17,
	IMAGE_FORMAT_YUV422P16 = 18,
	IMAGE_FORMAT_YUV444P16 = 19,
	IMAGE_FORMAT_Y16 = 20,

	_IMAGE_FORMAT_MAX,
};

enum Colourspace {
	Colourspace_Y = 0,
	Colourspace_YUV420 = 1,
	Colourspace_YUV422 = 2,
	Colourspace_YUV444 = 3,
};

enum Bitdepth {
	Bitdepth_8 = 8,
	Bitdepth_10 = 10,
	Bitdepth_12 = 12,
	Bitdepth_14 = 14,
};

// Format and layout of an image
//
struct ImageDescInfo;

class ImageDescription {
public:
	ImageDescription();
	ImageDescription(ImageFormat format, unsigned width, unsigned height);

	ImageFormat format() const;
	Colourspace colourspace() const;
	unsigned width(unsigned plane = 0) const;
	unsigned height(unsigned plane = 0) const;

	// return true if width and height are valid for the chosen format
	bool is_valid() const;

	// Total number of planes in image
	unsigned num_planes() const;

	// Total number of bytes to store an image
	unsigned byte_size() const;

	// Bytes in the give plane
	unsigned plane_size(unsigned int plane) const;

	// Return byte offset of pixel row within plane
	unsigned row_offset(unsigned int plane, unsigned row) const;

	// Return byte stride of plane
	unsigned row_stride(unsigned int plane) const;

	// Return byte size of pixel row (may be less than stride due to alignment)
	unsigned row_size(unsigned int plane) const;

	// Bits per component
	unsigned bit_depth() const;

	// Bytes per component
	unsigned byte_depth() const;

	// Return true if there are no gaps betweeen rows of pixels
	bool rows_are_contiguous(unsigned int plane) const;

	// Construct a vooya compatibile YUV filename from base
	std::string make_name(const std::string &base) const;

	// Make a new image description with same layout, but different bit depth
	ImageDescription with_depth(unsigned depth) const;

	// Make a new image description with same format, but different size
	ImageDescription with_size(unsigned width, unsigned height) const;

private:
	ImageFormat format_;
	unsigned width_;
	unsigned height_;

	const ImageDescInfo &imagedesc() const;
};

bool operator==(const class ImageDescription &lhs, const class ImageDescription &rhs);

class Image {
public:
	Image();
	Image(const std::string &name, const ImageDescription &description, uint64_t timestamp, const std::vector<Surface> &planes);
	Image(const std::string &name, const ImageDescription &description, uint64_t timestamp, const Surface (&planes)[3]);

	const Surface &plane(uint32_t p) const;

	//
	const ImageDescription &description() const { return description_; }

	// debugname
	std::string name() const { return name_; }

	// timestamp
	uint64_t timestamp() const { return timestamp_; }

	// number
	uint64_t number() const { return number_; }

	// checksum
	uint64_t checksum() const { return checksum_; }

	bool empty() const { return planes_.size() == 0; }

private:
	// Metadata
	const std::string name_;

	// Format and dimensions
	const ImageDescription description_;

	// Metadata
	const uint64_t timestamp_ = 0;
	uint64_t number_ = 0;
	uint64_t checksum_ = 0;

	// Image planes stored in surfaces
	//
	std::vector<Surface> planes_;
};

// Parsing for parameters
//
std::istream &operator>>(std::istream &in, ImageFormat &v);
std::istream &operator>>(std::istream &in, Colourspace &v);

} // namespace lctm
