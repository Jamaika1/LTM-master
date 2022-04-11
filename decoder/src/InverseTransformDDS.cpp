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
// InverseTransformDDS.cpp
//

#include "InverseTransformDDS.hpp"

namespace lctm {

// Basis for the Inverse DDS
// indexed by x%4, y%4, surface
// This has been inlines for performance.
//
// {{
// 		{+1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1, +1}, // 0,0
// 		{+1, +1, +1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1}, // 1,0
// 		{+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}, // 2,0
// 		{+1, -1, +1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1}, // 3,0
// 	}, {
// 		{+1, +1, +1, +1, +1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1}, // 0,1
// 		{+1, +1, +1, +1, -1, -1, -1, -1, -1, -1, -1, -1, +1, +1, +1, +1}, // 1,1
// 		{+1, -1, +1, -1, +1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1}, // 2,1
// 		{+1, -1, +1, -1, -1, +1, -1, +1, -1, +1, -1, +1, +1, -1, +1, -1}, // 3,1
// 	}, {

// 		{+1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1}, // 0,2
// 		{+1, +1, -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1, +1, +1}, // 1,2
// 		{+1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1, +1, -1, -1, +1}, // 2,2
// 		{+1, -1, -1, +1, -1, +1, +1, -1, +1, -1, -1, +1, -1, +1, +1, -1}, // 3,2
// 	}, {
// 		{+1, +1, -1, -1, +1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1}, // 0,3
// 		{+1, +1, -1, -1, -1, -1, +1, +1, -1, -1, +1, +1, +1, +1, -1, -1}, // 1,3
// 		{+1, -1, -1, +1, +1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1}, // 2,3
// 		{+1, -1, -1, +1, -1, +1, +1, -1, -1, +1, +1, -1, +1, -1, -1, +1}, // 3,3
// 	}};

Surface InverseTransformDDS::process(int width, int height, const Surface src_layers[]) {
	const SurfaceView<int16_t> coeffs[16] = {
	    SurfaceView<int16_t>(src_layers[0]),  SurfaceView<int16_t>(src_layers[1]),  SurfaceView<int16_t>(src_layers[2]),
	    SurfaceView<int16_t>(src_layers[3]),  SurfaceView<int16_t>(src_layers[4]),  SurfaceView<int16_t>(src_layers[5]),
	    SurfaceView<int16_t>(src_layers[6]),  SurfaceView<int16_t>(src_layers[7]),  SurfaceView<int16_t>(src_layers[8]),
	    SurfaceView<int16_t>(src_layers[9]),  SurfaceView<int16_t>(src_layers[10]), SurfaceView<int16_t>(src_layers[11]),
	    SurfaceView<int16_t>(src_layers[12]), SurfaceView<int16_t>(src_layers[13]), SurfaceView<int16_t>(src_layers[14]),
	    SurfaceView<int16_t>(src_layers[15])};

	auto dst = Surface::build_from<int16_t>();
	dst.reserve(width, height);
#if defined __OPT_MATRIX__
	for (unsigned y = 0; y < static_cast<unsigned>(height) / 4; y++) {
		unsigned dy = y * 4;
		const int16_t *pc00 = coeffs[0].data(0, y);
		const int16_t *pc01 = coeffs[1].data(0, y);
		const int16_t *pc02 = coeffs[2].data(0, y);
		const int16_t *pc03 = coeffs[3].data(0, y);
		const int16_t *pc04 = coeffs[4].data(0, y);
		const int16_t *pc05 = coeffs[5].data(0, y);
		const int16_t *pc06 = coeffs[6].data(0, y);
		const int16_t *pc07 = coeffs[7].data(0, y);
		const int16_t *pc08 = coeffs[8].data(0, y);
		const int16_t *pc09 = coeffs[9].data(0, y);
		const int16_t *pc10 = coeffs[10].data(0, y);
		const int16_t *pc11 = coeffs[11].data(0, y);
		const int16_t *pc12 = coeffs[12].data(0, y);
		const int16_t *pc13 = coeffs[13].data(0, y);
		const int16_t *pc14 = coeffs[14].data(0, y);
		const int16_t *pc15 = coeffs[15].data(0, y);
		int16_t *pd00 = dst.data(0, dy + 0);
		int16_t *pd01 = dst.data(0, dy + 1);
		int16_t *pd02 = dst.data(0, dy + 2);
		int16_t *pd03 = dst.data(0, dy + 3);
		for (unsigned x = 0; x < static_cast<unsigned>(width) / 4; x++) {
			// unsigned dx = x * 4;
			int16_t c00 = *pc00++;
			int16_t c01 = *pc01++;
			int16_t c02 = *pc02++;
			int16_t c03 = *pc03++;
			int16_t c04 = *pc04++;
			int16_t c05 = *pc05++;
			int16_t c06 = *pc06++;
			int16_t c07 = *pc07++;
			int16_t c08 = *pc08++;
			int16_t c09 = *pc09++;
			int16_t c10 = *pc10++;
			int16_t c11 = *pc11++;
			int16_t c12 = *pc12++;
			int16_t c13 = *pc13++;
			int16_t c14 = *pc14++;
			int16_t c15 = *pc15++;

			*pd00++ = c00 + c01 + c02 + c03 + c04 + c05 + c06 + c07 + c08 + c09 + c10 + c11 + c12 + c13 + c14 + c15;
			*pd00++ = c00 + c01 + c02 + c03 - c04 - c05 - c06 - c07 + c08 + c09 + c10 + c11 - c12 - c13 - c14 - c15;
			*pd00++ = c00 - c01 + c02 - c03 + c04 - c05 + c06 - c07 + c08 - c09 + c10 - c11 + c12 - c13 + c14 - c15;
			*pd00++ = c00 - c01 + c02 - c03 - c04 + c05 - c06 + c07 + c08 - c09 + c10 - c11 - c12 + c13 - c14 + c15;

			*pd01++ = c00 + c01 + c02 + c03 + c04 + c05 + c06 + c07 - c08 - c09 - c10 - c11 - c12 - c13 - c14 - c15;
			*pd01++ = c00 + c01 + c02 + c03 - c04 - c05 - c06 - c07 - c08 - c09 - c10 - c11 + c12 + c13 + c14 + c15;
			*pd01++ = c00 - c01 + c02 - c03 + c04 - c05 + c06 - c07 - c08 + c09 - c10 + c11 - c12 + c13 - c14 + c15;
			*pd01++ = c00 - c01 + c02 - c03 - c04 + c05 - c06 + c07 - c08 + c09 - c10 + c11 + c12 - c13 + c14 - c15;

			*pd02++ = c00 + c01 - c02 - c03 + c04 + c05 - c06 - c07 + c08 + c09 - c10 - c11 + c12 + c13 - c14 - c15;
			*pd02++ = c00 + c01 - c02 - c03 - c04 - c05 + c06 + c07 + c08 + c09 - c10 - c11 - c12 - c13 + c14 + c15;
			*pd02++ = c00 - c01 - c02 + c03 + c04 - c05 - c06 + c07 + c08 - c09 - c10 + c11 + c12 - c13 - c14 + c15;
			*pd02++ = c00 - c01 - c02 + c03 - c04 + c05 + c06 - c07 + c08 - c09 - c10 + c11 - c12 + c13 + c14 - c15;

			*pd03++ = c00 + c01 - c02 - c03 + c04 + c05 - c06 - c07 - c08 - c09 + c10 + c11 - c12 - c13 + c14 + c15;
			*pd03++ = c00 + c01 - c02 - c03 - c04 - c05 + c06 + c07 - c08 - c09 + c10 + c11 + c12 + c13 - c14 - c15;
			*pd03++ = c00 - c01 - c02 + c03 + c04 - c05 - c06 + c07 - c08 + c09 + c10 - c11 - c12 + c13 + c14 - c15;
			*pd03++ = c00 - c01 - c02 + c03 - c04 + c05 + c06 - c07 - c08 + c09 + c10 - c11 + c12 - c13 - c14 + c15;
		}
	}
#else
	for (unsigned y = 0; y < static_cast<unsigned>(height) / 4; y++) {
		for (unsigned x = 0; x < static_cast<unsigned>(width) / 4; x++) {
			unsigned dx = x * 4, dy = y * 4;
			dst.write(dx + 0, dy + 0,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 1, dy + 0,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 2, dy + 0,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 3, dy + 0,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 0, dy + 1,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 1, dy + 1,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 2, dy + 1,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 3, dy + 1,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (+coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 0, dy + 2,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 1, dy + 2,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 2, dy + 2,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 3, dy + 2,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (+coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (-coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 0, dy + 3,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
			dst.write(dx + 1, dy + 3,
			          (+coeffs[0].read(x, y)) + (+coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (-coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (-coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (+coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 2, dy + 3,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (+coeffs[4].read(x, y)) + (-coeffs[5].read(x, y)) + (-coeffs[6].read(x, y)) + (+coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (-coeffs[12].read(x, y)) + (+coeffs[13].read(x, y)) + (+coeffs[14].read(x, y)) +
			              (-coeffs[15].read(x, y)));
			dst.write(dx + 3, dy + 3,
			          (+coeffs[0].read(x, y)) + (-coeffs[1].read(x, y)) + (-coeffs[2].read(x, y)) + (+coeffs[3].read(x, y)) +
			              (-coeffs[4].read(x, y)) + (+coeffs[5].read(x, y)) + (+coeffs[6].read(x, y)) + (-coeffs[7].read(x, y)) +
			              (-coeffs[8].read(x, y)) + (+coeffs[9].read(x, y)) + (+coeffs[10].read(x, y)) + (-coeffs[11].read(x, y)) +
			              (+coeffs[12].read(x, y)) + (-coeffs[13].read(x, y)) + (-coeffs[14].read(x, y)) +
			              (+coeffs[15].read(x, y)));
		}
	}
#endif
	return dst.finish();
}

} // namespace lctm
