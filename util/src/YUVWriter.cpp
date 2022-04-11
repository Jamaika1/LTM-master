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
// YUVWriter.cpp
//
#include "YUVWriter.hpp"

namespace lctm {

YUVWriter::YUVWriter(const std::string &basename) : filename_(basename) {}

YUVWriter::YUVWriter(const std::string &basename, const ImageDescription &image_description, bool decorate)
    : image_description_(image_description) {

	if (decorate && file_extension(basename).empty())
		filename_ = image_description_.make_name(basename);
	else
		filename_ = basename;

	file_.reset(std::fopen(filename_.c_str(), "wb"));

	if (!file_) {
		ERR("Cannot open %s for writing", filename_.c_str());
	}
}

void YUVWriter::update_data(const ImageDescription &image_description) {
	image_description_ = image_description;

	file_.reset(std::fopen(filename_.c_str(), "wb"));

	if (!file_) {
		ERR("Cannot open %s for writing", filename_.c_str());
	}
}

void YUVWriter::write_surface(const Surface &surface) {
	auto v = surface.view_as<int8_t>();

	if (v.rows_are_contiguous()) {
		if (std::fwrite(v.data(), v.size(), 1, file_.get()) != 1)
			ERR("Cannot write to %s", filename_.c_str());
	} else {
		for (unsigned y = 0; y < v.height(); ++y) {
			if (fwrite(v.data(0, y), v.row_size(), 1, file_.get()) != 1)
				ERR("Cannot write to %s", filename_.c_str());
		}
	}
}

void YUVWriter::write(const Image &image) {
	CHECK(!!file_);

	if (!(image.description() == image_description_))
		WARN("WARNING: Output format changed!");

	for (unsigned p = 0; p < image_description_.num_planes(); ++p)
		write_surface(image.plane(p));

	std::fflush(file_.get());
}

void YUVWriter::write(const Surface &surface) {
	CHECK(image_description_.num_planes() == 1);

	write_surface(surface);

	std::fflush(file_.get());
}

void YUVWriter::close() {
	std::fflush(file_.get());
	file_.reset(nullptr);
}

std::unique_ptr<YUVWriter> CreateYUVWriter(const std::string &name, const ImageDescription &image_description, bool decorate) {
	return std::unique_ptr<YUVWriter>(new YUVWriter(name, image_description, decorate));
}

std::unique_ptr<YUVWriter> CreateYUVWriter(const std::string &name) { return std::unique_ptr<YUVWriter>(new YUVWriter(name)); }

} // namespace lctm
