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
// Crop.cpp
//

#include "Crop.hpp"

namespace lctm {

// Extract a rectangular region of int16 source surface
//
Surface CropResiduals::process(const Surface &plane, unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
	const auto src = plane.view_as<int16_t>();

	CHECK(x0 >= 0);
	CHECK(y0 >= 0);
	CHECK(x1 >= x0);
	CHECK(y1 >= y0);
	CHECK(x1 <= src.width());
	CHECK(y1 <= src.height());

	return Surface::build_from<int16_t>()
	    .generate(x1 - x0, y1 - y0, [&](unsigned x, unsigned y) -> int16_t { return src.read(x0 + x, y0 + y); })
	    .finish();
}

// Extract a rectangular region of uint8 source surface
//
Surface CropTemporal::process(const Surface &plane, unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
	const auto src = plane.view_as<uint8_t>();

	CHECK(x0 >= 0);
	CHECK(y0 >= 0);
	CHECK(x1 >= x0);
	CHECK(y1 >= y0);
	CHECK(x1 <= src.width());
	CHECK(y1 <= src.height());

	return Surface::build_from<uint8_t>()
	    .generate(x1 - x0, y1 - y0, [&](unsigned x, unsigned y) -> uint8_t { return src.read(x0 + x, y0 + y); })
	    .finish();
}

} // namespace lctm
