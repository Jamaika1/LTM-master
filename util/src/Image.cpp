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
// Image.cpp
//
#include "Image.hpp"
#include "Diagnostics.hpp"
#include "Misc.hpp"

#include <cassert>

namespace lctm {

// Various properties of the Image pixel formats
//
static const struct ImageDescInfo {
	unsigned planes;                     // Number of distinct planes in image
	unsigned char width_mask;            // Width bits under this mask must be zero for valid picture
	unsigned char height_mask;           // Height bits under this mask must be zero for valid picture
	unsigned char width_plane_shift[4];  // width of image must divide by 2^width_plane_shift for all planes
	unsigned char height_plane_shift[4]; // height of image must divide by 2^height_plane_shift for all planes
	unsigned bytes;                      // NUmber of bytes per component
	int component_bits;                  // Number of LSB bits per component
	Colourspace colourspace;             // Base colourspace
	const char *suffix;                  // Vooya compatible suffix for files
} s_PictureDescInfo[] = {
    {0, 0, 0, {0, 0, 0}, {0, 0, 0}, 0, 0, Colourspace_YUV420, "none"}, // IMAGE_FORMAT_NONE

    {3, 1, 1, {0, 1, 1}, {0, 1, 1}, 1, 8, Colourspace_YUV420, "_p420.yuv"}, // IMAGE_FORMAT_YUV420P8
    {3, 1, 0, {0, 1, 1}, {0, 0, 0}, 1, 8, Colourspace_YUV422, "_p422.yuv"}, // IMAGE_FORMAT_YUV422P8
    {3, 1, 0, {0, 0, 0}, {0, 0, 0}, 1, 8, Colourspace_YUV444, "_p444.yuv"}, // IMAGE_FORMAT_YUV444P8
    {1, 0, 0, {0, 0, 0}, {0, 0, 0}, 1, 8, Colourspace_Y, ".y"},             // IMAGE_FORMAT_Y8

    {3, 1, 1, {0, 1, 1}, {0, 1, 1}, 2, 10, Colourspace_YUV420, "_10bit_p420.yuv"}, // IMAGE_FORMAT_YUV420P10
    {3, 1, 0, {0, 1, 1}, {0, 0, 0}, 2, 10, Colourspace_YUV422, "_10bit_p422.yuv"}, // IMAGE_FORMAT_YUV422P10
    {3, 1, 0, {0, 0, 0}, {0, 0, 0}, 2, 10, Colourspace_YUV444, "_10bit_p444.yuv"}, // IMAGE_FORMAT_YUV444P10
    {1, 0, 0, {0, 0, 0}, {0, 0, 0}, 2, 10, Colourspace_Y, "_10bit.y"},             // IMAGE_FORMAT_Y10

    {3, 1, 1, {0, 1, 1}, {0, 1, 1}, 2, 12, Colourspace_YUV420, "_12bit_p420.yuv"}, // IMAGE_FORMAT_YUV420P12
    {3, 1, 0, {0, 1, 1}, {0, 0, 0}, 2, 12, Colourspace_YUV422, "_12bit_p422.yuv"}, // IMAGE_FORMAT_YUV422P12
    {3, 1, 0, {0, 0, 0}, {0, 0, 0}, 2, 12, Colourspace_YUV444, "_12bit_p444.yuv"}, // IMAGE_FORMAT_YUV444P12
    {1, 0, 0, {0, 0, 0}, {0, 0, 0}, 2, 12, Colourspace_Y, "_12bit.y"},             // IMAGE_FORMAT_Y12

    {3, 1, 1, {0, 1, 1}, {0, 1, 1}, 2, 14, Colourspace_YUV420, "_14bit_p420.yuv"}, // IMAGE_FORMAT_YUV420P14
    {3, 1, 0, {0, 1, 1}, {0, 0, 0}, 2, 14, Colourspace_YUV422, "_14bit_p422.yuv"}, // IMAGE_FORMAT_YUV422P14
    {3, 1, 0, {0, 0, 0}, {0, 0, 0}, 2, 14, Colourspace_YUV444, "_14bit_p444.yuv"}, // IMAGE_FORMAT_YUV444P14
    {1, 0, 0, {0, 0, 0}, {0, 0, 0}, 2, 14, Colourspace_Y, "_14bit.y"},             // IMAGE_FORMAT_Y14

    {3, 1, 1, {0, 1, 1}, {0, 1, 1}, 2, 16, Colourspace_YUV420, "_-16bit_p420.yuv"}, // IMAGE_FORMAT_YUV420P16
    {3, 1, 0, {0, 1, 1}, {0, 0, 0}, 2, 16, Colourspace_YUV422, "_-16bit_p422.yuv"}, // IMAGE_FORMAT_YUV422P16
    {3, 1, 0, {0, 0, 0}, {0, 0, 0}, 2, 16, Colourspace_YUV444, "_-16bit_p444.yuv"}, // IMAGE_FORMAT_YUV444P16
    {1, 0, 0, {0, 0, 0}, {0, 0, 0}, 2, 16, Colourspace_Y, "_-16bit.y"}              // IMAGE_FORMAT_Y16
};

static_assert(ARRAY_SIZE(s_PictureDescInfo) == _IMAGE_FORMAT_MAX, "Image format mismatch");

const inline ImageDescInfo &ImageDescription::imagedesc() const {
	assert(format_ > 0 && format_ < ARRAY_SIZE(s_PictureDescInfo));
	return s_PictureDescInfo[format_];
}

// ImageDescription
//
ImageDescription::ImageDescription() : format_(IMAGE_FORMAT_NONE), width_(0), height_(0) {}

ImageDescription::ImageDescription(ImageFormat f, unsigned w, unsigned h) : format_(f), width_(w), height_(h) {}

ImageFormat ImageDescription::format() const { return format_; }

Colourspace ImageDescription::colourspace() const { return imagedesc().colourspace; }

unsigned ImageDescription::width(unsigned plane) const {
	assert(plane < num_planes());

	return width_ >> imagedesc().width_plane_shift[plane];
}

unsigned ImageDescription::height(unsigned plane) const {
	assert(plane < num_planes());

	return height_ >> imagedesc().height_plane_shift[plane];
}

// Return true if image dimensions are valid for format
//
bool ImageDescription::is_valid() const {
	const ImageDescInfo &info = imagedesc();
	return !(width_ & info.width_mask || height_ & info.height_mask);
}

// Total number of planes in image
unsigned ImageDescription::num_planes() const { return imagedesc().planes; }

unsigned ImageDescription::plane_size(unsigned int plane) const {
	assert(plane < num_planes());
	return row_stride(plane) * (height_ >> imagedesc().height_plane_shift[plane]);
}

// Total number of bytes to store an image
unsigned ImageDescription::byte_size() const {
	unsigned r = 0;
	for (unsigned p = 0; p < num_planes(); ++p)
		r += plane_size(p);

	return r;
}

// Return byte offset of pixel row within buffer
unsigned ImageDescription::row_offset(unsigned int plane, unsigned row) const {
	assert(plane < num_planes());
	assert(row < height_);

	unsigned r = 0;
	for (unsigned p = 0; p < plane; ++p)
		r += plane_size(p);

	return r + row * row_stride(plane);
}

// Return byte size of pixel row (may be less than stride due to alignment)
unsigned ImageDescription::row_size(unsigned int plane) const {
	assert(plane < num_planes());
	const ImageDescInfo &info = imagedesc();

	// Width of plane, rounded up to component units
	return info.bytes * (width_ >> imagedesc().width_plane_shift[plane]);
}

// Return byte stride of plane
unsigned ImageDescription::row_stride(unsigned int plane) const {
	assert(plane < num_planes());
	unsigned a = 1;

	// Align stride to required power-of-2
	assert(((a + 1) & a) == 0);
	return (row_size(plane) + a) & ~a;
}

bool ImageDescription::rows_are_contiguous(unsigned int p) const { return row_size(p) == row_stride(p); }

unsigned ImageDescription::bit_depth() const { return imagedesc().component_bits; }

unsigned ImageDescription::byte_depth() const { return imagedesc().bytes; }

std::string ImageDescription::make_name(const std::string &base) const {
	return lctm::format("%s_%dx%d%s", base.c_str(), width_, height_, imagedesc().suffix);
}

bool operator==(const ImageDescription &lhs, const ImageDescription &rhs) {
	return lhs.format() == rhs.format() && lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

Image::Image(const std::string &name, const ImageDescription &description, uint64_t timestamp, const std::vector<Surface> &planes)
    : name_(name), description_(description), timestamp_(timestamp), planes_(planes) {
	// XXXX check that relative plane dimensions are ok
}

Image::Image(const std::string &name, const ImageDescription &description, uint64_t timestamp, const Surface (&planes)[3])
    : name_(name), description_(description), timestamp_(timestamp) {
	// XXXX check that relative plane dimensions are ok
	planes_.push_back(planes[0]);
	planes_.push_back(planes[1]);
	planes_.push_back(planes[2]);
}

Image::Image() : name_(""), description_(ImageDescription(IMAGE_FORMAT_NONE, 0, 0)), timestamp_(0) {}

const Surface &Image::plane(uint32_t p) const {
	assert(p < planes_.size());
	return planes_[p];
}

// Make a new image description with different bit depth
ImageDescription ImageDescription::with_depth(unsigned depth) const {
	unsigned fmt = (format_ - IMAGE_FORMAT_YUV420P8) % (IMAGE_FORMAT_YUV420P10 - IMAGE_FORMAT_YUV420P8);
	fmt += IMAGE_FORMAT_YUV420P8;

	switch (depth) {
	case 8:
		break;
	case 10:
		fmt += IMAGE_FORMAT_YUV420P10 - IMAGE_FORMAT_YUV420P8;
		break;
	case 12:
		fmt += IMAGE_FORMAT_YUV420P12 - IMAGE_FORMAT_YUV420P8;
		break;
	case 14:
		fmt += IMAGE_FORMAT_YUV420P14 - IMAGE_FORMAT_YUV420P8;
		break;
	case 16:
		fmt += IMAGE_FORMAT_YUV420P16 - IMAGE_FORMAT_YUV420P8;
		break;
	default:
		CHECK(0);
	}

	return ImageDescription(static_cast<ImageFormat>(fmt), width(), height());
}

// Make a new image description with different size
ImageDescription ImageDescription::with_size(unsigned width, unsigned height) const {
	return ImageDescription(format_, width, height);
}

// Parsing
//
std::istream &operator>>(std::istream &in, ImageFormat &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "yuv")
		v = IMAGE_FORMAT_YUV420P8;

	else if (s == "yuv420")
		v = IMAGE_FORMAT_YUV420P8;
	else if (s == "yuv422")
		v = IMAGE_FORMAT_YUV422P8;
	else if (s == "yuv444")
		v = IMAGE_FORMAT_YUV444P8;

	else if (s == "yuv420p")
		v = IMAGE_FORMAT_YUV420P8;
	else if (s == "yuv422p")
		v = IMAGE_FORMAT_YUV422P8;
	else if (s == "yuv444p")
		v = IMAGE_FORMAT_YUV444P8;

	else if (s == "yuv420p8")
		v = IMAGE_FORMAT_YUV420P8;
	else if (s == "yuv422p8")
		v = IMAGE_FORMAT_YUV422P8;
	else if (s == "yuv444p8")
		v = IMAGE_FORMAT_YUV444P8;

	else if (s == "yuv420p10")
		v = IMAGE_FORMAT_YUV420P10;
	else if (s == "yuv422p10")
		v = IMAGE_FORMAT_YUV422P10;
	else if (s == "yuv444p10")
		v = IMAGE_FORMAT_YUV444P10;

	else if (s == "yuv420p12")
		v = IMAGE_FORMAT_YUV420P12;
	else if (s == "yuv422p12")
		v = IMAGE_FORMAT_YUV422P12;
	else if (s == "yuv444p12")
		v = IMAGE_FORMAT_YUV444P12;

	else if (s == "yuv420p14")
		v = IMAGE_FORMAT_YUV420P14;
	else if (s == "yuv422p14")
		v = IMAGE_FORMAT_YUV422P14;
	else if (s == "yuv444p14")
		v = IMAGE_FORMAT_YUV444P14;

	else if (s == "yuv420p16")
		v = IMAGE_FORMAT_YUV420P16;
	else if (s == "yuv422p16")
		v = IMAGE_FORMAT_YUV422P16;
	else if (s == "yuv444p16")
		v = IMAGE_FORMAT_YUV444P16;

	else if (s == "y")
		v = IMAGE_FORMAT_Y8;
	else if (s == "y8")
		v = IMAGE_FORMAT_Y8;
	else if (s == "y10")
		v = IMAGE_FORMAT_Y10;
	else if (s == "y12")
		v = IMAGE_FORMAT_Y12;
	else if (s == "y14")
		v = IMAGE_FORMAT_Y14;
	else if (s == "y16")
		v = IMAGE_FORMAT_Y16;

	else if (s == "yuv400")
		v = IMAGE_FORMAT_Y8;
	else if (s == "yuv400p")
		v = IMAGE_FORMAT_Y8;
	else if (s == "yuv400p10")
		v = IMAGE_FORMAT_Y10;
	else if (s == "yuv400p12")
		v = IMAGE_FORMAT_Y12;
	else if (s == "yuv400p14")
		v = IMAGE_FORMAT_Y14;
	else if (s == "yuv400p16")
		v = IMAGE_FORMAT_Y16;
	else
		ERR("not an image format: %s", s.c_str());

	return in;
}

std::istream &operator>>(std::istream &in, Colourspace &v) {
	ImageFormat f;
	in >> f;

	CHECK(f < ARRAY_SIZE(s_PictureDescInfo));

	v = s_PictureDescInfo[f].colourspace;

	return in;
}

} // namespace lctm
