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
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//
// Exnpand.cpp
//

#include "Expand.hpp"
#include "Misc.hpp"

#include <vector>

using namespace std;

namespace lctm {

// Expands a uint8 source surface - adding borders to each side
//
Surface ExpandU8::process(const Surface &plane, unsigned width, unsigned height, unsigned offset_x, unsigned offset_y) {
	const auto src = plane.view_as<uint8_t>();
	const unsigned src_width = src.width();
	const unsigned src_height = src.height();

	return Surface::build_from<uint8_t>()
	    .generate(width, height,
	              [&](unsigned x, unsigned y) -> int8_t {
		              return src.read(clamp(x - offset_x, 0u, src_width - 1), clamp(y - offset_y, 0u, src_height - 1));
	              })
	    .finish();
}

// Expands a uint16 source surface - adding borders to each side
//
Surface ExpandU16::process(const Surface &plane, unsigned width, unsigned height, unsigned offset_x, unsigned offset_y) {
	const auto src = plane.view_as<uint16_t>();
	const unsigned src_width = src.width();
	const unsigned src_height = src.height();

	return Surface::build_from<uint16_t>()
	    .generate(width, height,
	              [&](unsigned x, unsigned y) -> uint16_t {
		              return src.read(clamp(x - offset_x, 0u, src_width - 1), clamp(y - offset_y, 0u, src_height - 1));
	              })
	    .finish();
}

// Add any necessary padding to image to make it conformant
//
Image ExpandImage(const Image &src, const ImageDescription &description) {
	// Check if image is already the right size?
	if (src.description() == description)
		return src;

	vector<Surface> surfaces;
	for (unsigned p = 0; p < src.description().num_planes(); ++p) {
		switch (src.description().bit_depth()) {
		case 8:
			surfaces.push_back(ExpandU8().process(src.plane(p), description.width(p), description.height(p), 0, 0));
			break;
		case 10:
		case 12:
		case 14:
		case 16:
			surfaces.push_back(ExpandU16().process(src.plane(p), description.width(p), description.height(p), 0, 0));
			break;
		default:
			CHECK(0);
			return Image();
		}
	}

	return Image("expanded-" + src.name(), description, src.timestamp(), surfaces);
}

} // namespace lctm
