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
//               Stefano Battista (bautz66@gmail.com)
//
// TransformDDS.cpp
//

#include "TransformDDS.hpp"
#include "Config.hpp"

namespace lctm {

void TransformDDS::process(const Surface &residuals, EncodingMode mode, Surface layers[]) {

	const LayerEncodeFlags encode_flags(TransformType_DDS, mode);
#if defined __OPT_MODULO__
	CHECK((residuals.width() & 0x03) == 0);
	CHECK((residuals.height() & 0x03) == 0);
#else
	CHECK((residuals.width() % 4) == 0);
	CHECK((residuals.height() % 4) == 0);
#endif
#if defined __OPT_DIVISION__
	unsigned width = residuals.width() >> 2;
	unsigned height = residuals.height() >> 2;
#else
	unsigned width = residuals.width() / 4;
	unsigned height = residuals.height() / 4;
#endif

	SurfaceView<int16_t> src(residuals);

	static const int32_t basis[16][16] = {
	    {+1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1}, // 0,0
	    {+1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1}, // 1,0
	    {+1, +1, +1, +1, +1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1}, // 2,0
	    {+1, +1, -1, -1, +1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1}, // 3,0

	    {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}, // 0,1
	    {+1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1}, // 1,1
	    {+1, -1, +1, -1, +1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1}, // 2,1
	    {+1, -1, -1, +1, +1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1}, // 3.1

	    {+1, +1, +1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1}, // 0,2
	    {+1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1, +1, +1}, // 1,2
	    {+1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1, +1, +1, +1, +1}, // 2,2
	    {+1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1, +1, +1, -1, -1}, // 3,2

	    {+1, -1, +1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1}, // 0,3
	    {+1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1, +1, -1}, // 1,3
	    {+1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1, +1, -1, +1, -1}, // 2,3
	    {+1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1, +1, -1, -1, +1}, // 3,3
	};

#if defined __OPT_MATRIX__

	if (encode_flags.encode_residual(0)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1},  // 0,0
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) + src.read(h + 2, k + 0) + src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) + src.read(h + 2, k + 1) + src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) + src.read(h + 2, k + 2) + src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) + src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[0] = dest.finish();
	} else {
		layers[0] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(1)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1},  // 1,0
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) - src.read(h + 2, k + 0) - src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) - src.read(h + 2, k + 1) - src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) - src.read(h + 2, k + 2) - src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) - src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[1] = dest.finish();
	} else {
		layers[1] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(2)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, +1, +1, +1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1},  // 2,0
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) + src.read(h + 2, k + 0) + src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) + src.read(h + 2, k + 1) + src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) - src.read(h + 2, k + 2) - src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) - src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[2] = dest.finish();
	} else {
		layers[2] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(3)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, -1, -1, +1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1},  // 3,0
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) - src.read(h + 2, k + 0) - src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) - src.read(h + 2, k + 1) - src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) + src.read(h + 2, k + 2) + src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) + src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[3] = dest.finish();
	} else {
		layers[3] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(4)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1},  // 0,1
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) + src.read(h + 2, k + 0) - src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) + src.read(h + 2, k + 1) - src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) + src.read(h + 2, k + 2) - src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) + src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[4] = dest.finish();
	} else {
		layers[4] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(5)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1},  // 1,1
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) - src.read(h + 2, k + 0) + src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) - src.read(h + 2, k + 1) + src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) - src.read(h + 2, k + 2) + src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) - src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[5] = dest.finish();
	} else {
		layers[5] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(6)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, +1, -1, +1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1},  // 2,1
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) + src.read(h + 2, k + 0) - src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) + src.read(h + 2, k + 1) - src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) - src.read(h + 2, k + 2) + src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) - src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[6] = dest.finish();
	} else {
		layers[6] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(7)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, -1, +1, +1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1},  // 3.1
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) - src.read(h + 2, k + 0) + src.read(h + 3, k + 0) +
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) - src.read(h + 2, k + 1) + src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) + src.read(h + 2, k + 2) - src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) + src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[7] = dest.finish();
	} else {
		layers[7] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(8)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, +1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1},  // 0,2
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) + src.read(h + 2, k + 0) + src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) - src.read(h + 2, k + 1) - src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) + src.read(h + 2, k + 2) + src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) - src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[8] = dest.finish();
	} else {
		layers[8] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(9)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1, +1, +1},  // 1,2
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) - src.read(h + 2, k + 0) - src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) + src.read(h + 2, k + 1) + src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) - src.read(h + 2, k + 2) - src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) + src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[9] = dest.finish();
	} else {
		layers[9] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(10)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1, +1, +1, +1, +1},  // 2,2
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) + src.read(h + 2, k + 0) + src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) - src.read(h + 2, k + 1) - src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) - src.read(h + 2, k + 2) - src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) + src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[10] = dest.finish();
	} else {
		layers[10] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(11)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1, +1, +1, -1, -1},  // 3,2
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) + src.read(h + 1, k + 0) - src.read(h + 2, k + 0) - src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) - src.read(h + 1, k + 1) + src.read(h + 2, k + 1) + src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) + src.read(h + 2, k + 2) + src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) - src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[11] = dest.finish();
	} else {
		layers[11] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(12)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, +1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1},  // 0,3
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) + src.read(h + 2, k + 0) - src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) - src.read(h + 2, k + 1) + src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) + src.read(h + 2, k + 2) - src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) - src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[12] = dest.finish();
	} else {
		layers[12] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(13)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1, +1, -1},  // 1,3
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) - src.read(h + 2, k + 0) + src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) + src.read(h + 2, k + 1) - src.read(h + 3, k + 1) +
				                src.read(h + 0, k + 2) - src.read(h + 1, k + 2) - src.read(h + 2, k + 2) + src.read(h + 3, k + 2) -
				                src.read(h + 0, k + 3) + src.read(h + 1, k + 3) + src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[13] = dest.finish();
	} else {
		layers[13] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(14)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1, +1, -1, +1, -1},  // 2,3
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) + src.read(h + 2, k + 0) - src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) - src.read(h + 2, k + 1) + src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) - src.read(h + 2, k + 2) + src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) + src.read(h + 2, k + 3) - src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[14] = dest.finish();
	} else {
		layers[14] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

	if (encode_flags.encode_residual(15)) {
		auto dest = Surface::build_from<int16_t>();
		dest.reserve(width, height);
		for (unsigned y = 0; y < height; ++y) {
			int k = 4 * y;
			for (unsigned x = 0; x < width; ++x) {
				// {+1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1, +1, -1, -1, +1},  // 3,3
				int h = 4 * x;
				int32_t coef = (src.read(h + 0, k + 0) - src.read(h + 1, k + 0) - src.read(h + 2, k + 0) + src.read(h + 3, k + 0) -
				                src.read(h + 0, k + 1) + src.read(h + 1, k + 1) + src.read(h + 2, k + 1) - src.read(h + 3, k + 1) -
				                src.read(h + 0, k + 2) + src.read(h + 1, k + 2) + src.read(h + 2, k + 2) - src.read(h + 3, k + 2) +
				                src.read(h + 0, k + 3) - src.read(h + 1, k + 3) - src.read(h + 2, k + 3) + src.read(h + 3, k + 3)) /
				               16;
				dest.write(x, y, coef);
			}
		}
		layers[15] = dest.finish();
	} else {
		layers[15] =
		    Surface::build_from<int16_t>().generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; }).finish();
	}

#else

	for (unsigned l = 0; l < 16; ++l) {
		if (encode_flags.encode_residual(l)) {
			layers[l] =
			    Surface::build_from<int16_t>()
			        .generate(
			            width, height,
			            [&](unsigned x, unsigned y) -> int16_t {
				            return (src.read(x * 4 + 0, y * 4 + 0) * basis[l][0] + src.read(x * 4 + 1, y * 4 + 0) * basis[l][1] +
				                    src.read(x * 4 + 2, y * 4 + 0) * basis[l][2] + src.read(x * 4 + 3, y * 4 + 0) * basis[l][3] +

				                    src.read(x * 4 + 0, y * 4 + 1) * basis[l][4] + src.read(x * 4 + 1, y * 4 + 1) * basis[l][5] +
				                    src.read(x * 4 + 2, y * 4 + 1) * basis[l][6] + src.read(x * 4 + 3, y * 4 + 1) * basis[l][7] +

				                    src.read(x * 4 + 0, y * 4 + 2) * basis[l][8] + src.read(x * 4 + 1, y * 4 + 2) * basis[l][9] +
				                    src.read(x * 4 + 2, y * 4 + 2) * basis[l][10] + src.read(x * 4 + 3, y * 4 + 2) * basis[l][11] +

				                    src.read(x * 4 + 0, y * 4 + 3) * basis[l][12] + src.read(x * 4 + 1, y * 4 + 3) * basis[l][13] +
				                    src.read(x * 4 + 2, y * 4 + 3) * basis[l][14] + src.read(x * 4 + 3, y * 4 + 3) * basis[l][15]) /
				                   16;
			            })
			        .finish();
		} else {
			layers[l] = Surface::build_from<int16_t>()
			                .generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; })
			                .finish();
		}
	}

#endif
}

} // namespace lctm
