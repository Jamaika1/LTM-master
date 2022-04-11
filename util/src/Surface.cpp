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
//// Surface.cpp
//
// NB: Most all this implementation is templated on data type in SurfaceImpl.hpp
//
#include "Surface.hpp"
#include "Convert.hpp"
#include "Diagnostics.hpp"
#include "YUVWriter.hpp"

#include <cstring>
#include <map>

namespace lctm {

//// Surface
//
//
Surface::Surface(const std::string name, uint64_t checksum, const std::shared_ptr<Buffer> &buffer, unsigned offset, unsigned width,
                 unsigned height, unsigned stride)
    : name_(name), checksum_(checksum), buffer_(buffer), offset_(offset), width_(width), height_(height), stride_(stride) {}

// Global debug state for surface dumping
//
static bool dump_surfaces = true;
static std::map<std::string, std::unique_ptr<YUVWriter>> yuv_writers;

void Surface::set_dump_surfaces(bool b) { dump_surfaces = b; }

bool Surface::get_dump_surfaces() { return dump_surfaces; }

// Dump surface as 8bit p420, with dummy chroma planes
//
void Surface::dump_p420(const std::string &name) const {
	if (!dump_surfaces)
		return;

	ImageDescription desc(IMAGE_FORMAT_YUV420P8, width(), height());

	if (yuv_writers.find(name) == yuv_writers.end())
		yuv_writers[name] = CreateYUVWriter(name, desc, true);

	Surface planes[3];

	if (bpp() == 1)
		// Just use base as bytes
		planes[0] = *this;
	else
		// Assume S87
		planes[0] = ConvertToU8().process(*this, 7);

	// Create a dummy surface for U&V
	auto sb = Surface::build_from<uint8_t>();
	// sb.reserve(width() / 2, height() / 2);
	sb.reserve(width() >> 1, height() >> 1);
	memset(sb.data(0, 0), 0x80, sb.width() * sb.height());
	planes[1] = planes[2] = sb.finish();

	yuv_writers[name]->write(Image(name, desc, 0, planes));
}

// Dump surface as 8bpp, 10bpp, 12bpp, or 14bpp with correct chroma subsampling and proper chroma planes
//
void Surface::dump_yuv(const std::string &name, Surface (&planes)[3], ImageDescription desc, bool decorate) const {
	if (desc.bit_depth() == 8) {
		Surface converted[3];
		if (yuv_writers.find(name) == yuv_writers.end())
			yuv_writers[name] = CreateYUVWriter(name, desc, decorate);
		for (unsigned p = 0; p < desc.num_planes(); p++)
			converted[p] = ConvertToU8().process(planes[p], 7);
		yuv_writers[name]->write(Image(name, desc, 0, converted));
	} else if (desc.bit_depth() == 10) {
		Surface converted[3];
		if (yuv_writers.find(name) == yuv_writers.end())
			yuv_writers[name] = CreateYUVWriter(name, desc, decorate);
		for (unsigned p = 0; p < desc.num_planes(); p++)
			converted[p] = ConvertToU16().process(planes[p], 5);
		yuv_writers[name]->write(Image(name, desc, 0, converted));
	} else if (desc.bit_depth() == 12) {
		Surface converted[3];
		if (yuv_writers.find(name) == yuv_writers.end())
			yuv_writers[name] = CreateYUVWriter(name, desc, decorate);
		for (unsigned p = 0; p < desc.num_planes(); p++)
			converted[p] = ConvertToU16().process(planes[p], 3);
		yuv_writers[name]->write(Image(name, desc, 0, converted));
	} else if (desc.bit_depth() == 14) {
		Surface converted[3];
		if (yuv_writers.find(name) == yuv_writers.end())
			yuv_writers[name] = CreateYUVWriter(name, desc, decorate);
		for (unsigned p = 0; p < desc.num_planes(); p++)
			converted[p] = ConvertToU16().process(planes[p], 1);
		yuv_writers[name]->write(Image(name, desc, 0, converted));
	} else {
		INFO("Image Format not supported %4d bpp", desc.bit_depth());
	}
}

// Dump surface as 8 or 16 bit Y
//
void Surface::dump(const std::string &name) const {
	if (!dump_surfaces)
		return;

	ImageFormat fmt = IMAGE_FORMAT_NONE;
	switch (bpp()) {
	case 2:
		fmt = IMAGE_FORMAT_Y16;
		break;
	case 1:
		fmt = IMAGE_FORMAT_Y8;
		break;
	default:
		assert(false);
	}

	ImageDescription desc(fmt, width(), height());

	if (yuv_writers.find(name) == yuv_writers.end())
		yuv_writers[name] = CreateYUVWriter(name, desc, true);

	Surface planes[3];

	planes[0] = *this;
	yuv_writers[name]->write(Image(name, desc, 0, planes));
}

// Dump array of layers/surfaces as 16 bit Y
//
void Surface::dump_layers(const Surface surface[], const std::string &name, const unsigned transform_block_size) {
	if (!dump_surfaces)
		return;

	CHECK(surface[0].bpp() == 2); // only 16 bit supported at the moment
	ImageDescription desc(IMAGE_FORMAT_Y16, surface[0].width() * transform_block_size, surface[0].height() * transform_block_size);

	if (yuv_writers.find(name) == yuv_writers.end())
		yuv_writers[name] = CreateYUVWriter(name, desc, true);

	Surface planes[3];

	if (transform_block_size == 2) {
		const auto src00_view = surface[0].view_as<int16_t>();
		const auto src01_view = surface[1].view_as<int16_t>();
		const auto src02_view = surface[2].view_as<int16_t>();
		const auto src03_view = surface[3].view_as<int16_t>();

		auto dst = Surface::build_from<int16_t>();
		dst.reserve(surface[0].width() * transform_block_size, surface[0].height() * transform_block_size);

		for (unsigned x = 0; x < surface[0].width(); x++) {
			for (unsigned y = 0; y < surface[0].height(); y++) {
				dst.write(x * 2 + 0, y * 2 + 0, src00_view.read(x, y));
				dst.write(x * 2 + 1, y * 2 + 0, src01_view.read(x, y));
				dst.write(x * 2 + 0, y * 2 + 1, src02_view.read(x, y));
				dst.write(x * 2 + 1, y * 2 + 1, src03_view.read(x, y));
			}
		}
		planes[0] = dst.finish();
	} else {
		const auto src00_view = surface[0].view_as<int16_t>();
		const auto src01_view = surface[1].view_as<int16_t>();
		const auto src02_view = surface[2].view_as<int16_t>();
		const auto src03_view = surface[3].view_as<int16_t>();
		const auto src04_view = surface[4].view_as<int16_t>();
		const auto src05_view = surface[5].view_as<int16_t>();
		const auto src06_view = surface[6].view_as<int16_t>();
		const auto src07_view = surface[7].view_as<int16_t>();
		const auto src08_view = surface[8].view_as<int16_t>();
		const auto src09_view = surface[9].view_as<int16_t>();
		const auto src10_view = surface[10].view_as<int16_t>();
		const auto src11_view = surface[11].view_as<int16_t>();
		const auto src12_view = surface[12].view_as<int16_t>();
		const auto src13_view = surface[13].view_as<int16_t>();
		const auto src14_view = surface[14].view_as<int16_t>();
		const auto src15_view = surface[15].view_as<int16_t>();

		auto dst = Surface::build_from<int16_t>();
		dst.reserve(surface[0].width() * transform_block_size, surface[0].height() * transform_block_size);

		for (unsigned x = 0; x < surface[0].width(); x++) {
			for (unsigned y = 0; y < surface[0].height(); y++) {
				dst.write(x * 4 + 0, y * 4 + 0, src00_view.read(x, y));
				dst.write(x * 4 + 1, y * 4 + 0, src01_view.read(x, y));
				dst.write(x * 4 + 2, y * 4 + 0, src02_view.read(x, y));
				dst.write(x * 4 + 3, y * 4 + 0, src03_view.read(x, y));
				dst.write(x * 4 + 0, y * 4 + 1, src04_view.read(x, y));
				dst.write(x * 4 + 1, y * 4 + 1, src05_view.read(x, y));
				dst.write(x * 4 + 2, y * 4 + 1, src06_view.read(x, y));
				dst.write(x * 4 + 3, y * 4 + 1, src07_view.read(x, y));
				dst.write(x * 4 + 0, y * 4 + 2, src08_view.read(x, y));
				dst.write(x * 4 + 1, y * 4 + 2, src09_view.read(x, y));
				dst.write(x * 4 + 2, y * 4 + 2, src10_view.read(x, y));
				dst.write(x * 4 + 3, y * 4 + 2, src11_view.read(x, y));
				dst.write(x * 4 + 0, y * 4 + 3, src12_view.read(x, y));
				dst.write(x * 4 + 1, y * 4 + 3, src13_view.read(x, y));
				dst.write(x * 4 + 2, y * 4 + 3, src14_view.read(x, y));
				dst.write(x * 4 + 3, y * 4 + 3, src15_view.read(x, y));
			}
		}
		planes[0] = dst.finish();
	}

	yuv_writers[name]->write(Image(name, desc, 0, planes));
}

void Surface::dump_image(const std::string &name) const {
	ImageFormat fmt = IMAGE_FORMAT_NONE;
	switch (bpp()) {
	case 2:
		fmt = IMAGE_FORMAT_Y16;
		break;
	case 1:
		fmt = IMAGE_FORMAT_Y8;
		break;
	default:
		assert(false);
	}

	ImageDescription desc(fmt, width(), height());

	auto writer = CreateYUVWriter(name, desc, true);

	Surface planes[3];
	planes[0] = *this;

	writer->write(Image(name, desc, 0, planes));
}

uint64_t Surface::checksum() const {
	if (!checksummed_) {
		SurfaceView<uint8_t> v(*this);
		checksum_ = crc64(0, v.data(), v.size());
		checksummed_ = true;
	}

	return checksum_;
}

} // namespace lctm
