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
// InverseTransformDDS_1D.cpp
//

#include "InverseTransformDDS_1D.hpp"

namespace lctm {

Surface InverseTransformDDS_1D::process(int width, int height, const Surface src_layers[]) {
	// clang-format off

	const SurfaceView<int16_t, 2> srcs[16] = {
		SurfaceView<int16_t, 2>(src_layers[0]),  SurfaceView<int16_t, 2>(src_layers[1]),
		SurfaceView<int16_t, 2>(src_layers[2]),  SurfaceView<int16_t, 2>(src_layers[3]),
		SurfaceView<int16_t, 2>(src_layers[4]),  SurfaceView<int16_t, 2>(src_layers[5]),
		SurfaceView<int16_t, 2>(src_layers[6]),  SurfaceView<int16_t, 2>(src_layers[7]),
		SurfaceView<int16_t, 2>(src_layers[8]),  SurfaceView<int16_t, 2>(src_layers[9]),
		SurfaceView<int16_t, 2>(src_layers[10]), SurfaceView<int16_t, 2>(src_layers[11]),
		SurfaceView<int16_t, 2>(src_layers[12]), SurfaceView<int16_t, 2>(src_layers[13]),
		SurfaceView<int16_t, 2>(src_layers[14]), SurfaceView<int16_t, 2>(src_layers[15])};

	static const int16_t basis[4][4][16] = {{
			{+1, +1, +1, +1, +1, +1, +1, +1, 0, 0, 0, 0, +1, +1, +1, +1},
			{+1, +1, +1, +1, -1, -1, -1, -1, 0, 0, 0, 0, -1, -1, -1, -1},
			{+1, -1, +1, -1, +1, -1, +1, -1, 0, 0, 0, 0, +1, -1, +1, -1},
			{+1, -1, +1, -1, -1, +1, -1, +1, 0, 0, 0, 0, -1, +1, -1, +1}
		},{
			{0, 0, 0, 0, +1, +1, +1, +1, +1, +1, +1, +1, -1, -1, -1, -1},
			{0, 0, 0, 0, -1, -1, -1, -1, +1, +1, +1, +1, +1, +1, +1, +1},
			{0, 0, 0, 0, +1, -1, +1, -1, +1, -1, +1, -1, -1, +1, -1, +1},
			{0, 0, 0, 0, -1, +1, -1, +1, +1, -1, +1, -1, +1, -1, +1, -1}
		},{
			{+1, +1, -1, -1, +1, +1, -1, -1, 0, 0, 0, 0, +1, +1, -1, -1},
			{+1, +1, -1, -1, -1, -1, +1, +1, 0, 0, 0, 0, -1, -1, +1, +1},
			{+1, -1, -1, +1, +1, -1, -1, +1, 0, 0, 0, 0, +1, -1, -1, +1},
			{+1, -1, -1, +1, -1, +1, +1, -1, 0, 0, 0, 0, -1, +1, +1, -1}
		},{
			{0, 0, 0, 0, +1, +1, -1, -1, +1, +1, -1, -1, -1, -1, +1, +1},
			{0, 0, 0, 0, -1, -1, +1, +1, +1, +1, -1, -1, +1, +1, -1, -1},
			{0, 0, 0, 0, +1, -1, -1, +1, +1, -1, -1, +1, -1, +1, +1, -1},
			{0, 0, 0, 0, -1, +1, +1, -1, +1, -1, -1, +1, +1, -1, -1, +1}
		}};

	// clang-format on

	return Surface::build_from<int16_t>()
	    .generate(width, height,
	              [&](unsigned x, unsigned y) -> int16_t {
		              int16_t acc = 0;
		              for (unsigned s = 0; s < 16; ++s)
#if defined __OPT_MODULO__
			              acc += basis[y & 0x03][x & 0x03][s] * srcs[s].read(x, y);
#else 
			              acc += basis[y % 4][x % 4][s] * srcs[s].read(x, y);
#endif
		              return acc;
	              })
	    .finish();
}

} // namespace lctm
