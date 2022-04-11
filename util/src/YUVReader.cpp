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
// YUVReader.cpp
//
// Read raw YUV video files
//
#include "YUVReader.hpp"

#include <cstdio>
#include <memory>
#include <mutex>
#include <regex>
#include <string>

#include "Diagnostics.hpp"
#include "Image.hpp"
#include "Misc.hpp"
#include "Platform.hpp"

namespace lctm {

using namespace std;

YUVReader::YUVReader(const std::string &name, float rate, FILE *file, uintmax_t fileSize)
    : name_(name), length_(0), rate_(rate), file_(file), fileSize_(fileSize) {}

YUVReader::YUVReader(const std::string &name, const ImageDescription &image_description, unsigned length, float rate, FILE *file,
                     uintmax_t fileSize)
    : name_(name), image_description_(image_description), length_(length), rate_(rate), file_(file), fileSize_(fileSize) {}

void YUVReader::update_data(const ImageDescription &image_description) {
	if (fileSize_ == static_cast<uintmax_t>(-1))
		ERR("Cannot open YUV file");

	// Figure length
	unsigned length = static_cast<unsigned>(fileSize_ / image_description.byte_size());

	if (length == 0)
		ERR("YUV file is too small");

	image_description_ = image_description;
	length_ = length;

	return;
}

void YUVReader::set_position(unsigned position) const {
	CHECK(position < length_);
	position_ = position;
	CHECK(fseeko(file_.get(), (uint64_t)position_ * (uint64_t)image_description_.byte_size(), SEEK_SET) == 0);
}

// Get image from frame position
Image YUVReader::read(unsigned position, uint64_t timestamp) const {
	set_position(position);

	std::vector<Surface> surfaces;

	for (unsigned p = 0; p < image_description_.num_planes(); ++p) {
		auto b = Surface::build_from<int8_t>();
		b.reserve_bpp(image_description_.width(p), image_description_.height(p), image_description_.byte_depth(),
		              image_description_.row_stride(p));
		if (image_description_.rows_are_contiguous(p)) {
			if (fread(b.data(), image_description_.plane_size(p), 1, file_.get()) != 1)
				ERR("Cannot read YUV file");
		} else {
			for (unsigned y = 0; y < image_description_.height(p); ++y) {
				if (fread(b.data(0, y), image_description_.row_size(p), 1, file_.get()) != 1)
					ERR("Cannot read YUV file");
			}
		}
		surfaces.push_back(b.finish());
	}

	return Image(format("%s:%d", name_.c_str(), position_), image_description_, timestamp, surfaces);
}

// Parse the picture details from the filename
//
// Uses roughly the same conventions as Vooya and YUVDeluxe
//
// Returns a description - if format == PF_NONE, then no format was recognised
// If a rate is recognized, and pRate != NULL, then the frame rate will be stored through that pointer
//

// Map of format names to internal version
static const struct {
	const char *name;
	int bits;
	ImageFormat format;
} KnownFormats[] = {
    {"420", -1, IMAGE_FORMAT_YUV420P8},  {"420p", -1, IMAGE_FORMAT_YUV420P8},  {"p420", -1, IMAGE_FORMAT_YUV420P8},
    {"yuv", -1, IMAGE_FORMAT_YUV420P8},

    {"420", 8, IMAGE_FORMAT_YUV420P8},   {"420p", 8, IMAGE_FORMAT_YUV420P8},   {"p420", 8, IMAGE_FORMAT_YUV420P8},
    {"yuv", 8, IMAGE_FORMAT_YUV420P8},

    {"420", 10, IMAGE_FORMAT_YUV420P10}, {"420p", 10, IMAGE_FORMAT_YUV420P10}, {"p420", 10, IMAGE_FORMAT_YUV420P10},
    {"yuv", 10, IMAGE_FORMAT_YUV420P10},

    {"y", 8, IMAGE_FORMAT_Y8},           {"y", 10, IMAGE_FORMAT_Y10},          {"y", 16, IMAGE_FORMAT_Y16},

};

static ImageDescription ParseYUVFilename(const string &name, float *pRate) {

	ImageFormat image_format = IMAGE_FORMAT_NONE;
	unsigned width = 0;
	unsigned height = 0;

	// Parse filename for picture description
	//
	vector<string> parts;
	split(parts, name, ("-_."));

	static const regex dimensions_re("([0-9]+)x([0-9]+)");                     // Size
	static const regex fps_re("([0-9]+)(fps|hz)");                             // Hz
	static const regex bits_re("([0-9]+)(bits?|bpp)");                         // Bit depth
	static const regex format_re("(420|420p|p420|422|p422|422p|yuv|yuyv|y|)"); // Format

	string format;
	int bits = -1;

	for (auto const p : parts) {
		cmatch m;
		const string lp = lowercase(p);

		if (regex_match(lp.c_str(), m, dimensions_re)) {
			width = stoi(m[1]);
			height = stoi(m[2]);
		}

		if (regex_match(p.c_str(), m, fps_re)) {
			if (pRate)
				*pRate = stof(m[1].str());
		}

		if (regex_match(p.c_str(), m, bits_re)) {
			bits = stoi(m[1].str());
		}

		if (regex_match(p.c_str(), m, format_re)) {
			if (format.empty())
				format = lowercase(m[1].str());
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(KnownFormats); ++i)
		if (format == KnownFormats[i].name && bits == KnownFormats[i].bits)
			image_format = KnownFormats[i].format;

	return ImageDescription(image_format, width, height);
}

// Open file for reading, given explicit format
//
std::unique_ptr<YUVReader> CreateYUVReader(const std::string &name, const ImageDescription &description, unsigned rate) {
	// Is file there?
	const uintmax_t fileSize = file_size(name);
	if (fileSize == static_cast<uintmax_t>(-1))
		return 0;

	// Figure length
	unsigned length = static_cast<unsigned>(fileSize / description.byte_size());

	if (length == 0)
		ERR("YUV file is too small");

	UniquePtrFile yuvFile(std::fopen(name.c_str(), "rb"));

	if (yuvFile)
		return unique_ptr<YUVReader>(new YUVReader(name, description, length, (float)rate, yuvFile.release(), fileSize));

	ERR("Cannot open YUV file");
	return 0;
}

// Open file for reading, format is inferred from filename
//
unique_ptr<YUVReader> CreateYUVReader(const string &name) {
	// Does it have a sensible name?
	float rate = 25.0f;
	ImageDescription description = ParseYUVFilename(name, &rate);
	if (description.format() == IMAGE_FORMAT_NONE)
		ERR("Cannot parse YUV filename");

	return CreateYUVReader(name, description, (unsigned)rate);
}

// Create Dummy Reader, will be filled later
//
unique_ptr<YUVReader> CreateYUVReader(const string &name, unsigned rate) {
	// Is file there?
	const uintmax_t fileSize = file_size(name);
	if (fileSize == static_cast<uintmax_t>(-1))
		return 0;

	UniquePtrFile yuvFile(std::fopen(name.c_str(), "rb"));

	if (yuvFile) {
		return unique_ptr<YUVReader>(new YUVReader(name, (float)rate, yuvFile.release(), fileSize));
	}

	ERR("Cannot open YUV file");
	return 0;
};

} // namespace lctm
