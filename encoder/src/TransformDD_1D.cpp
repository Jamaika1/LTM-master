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
// TransformDD_1D.cpp
//

#include "TransformDD_1D.hpp"
#include "Config.hpp"

namespace lctm {

void TransformDD_1D::process(const Surface &residuals, EncodingMode mode, Surface layers[]) {

	const LayerEncodeFlags encode_flags(TransformType_DD, mode);
#if defined __OPT_MODULO__
	CHECK((residuals.width() & 0x01) == 0);
	CHECK((residuals.height() & 0x01) == 0);
#else
	CHECK((residuals.width() % 2) == 0);
	CHECK((residuals.height() % 2) == 0);
#endif
#if defined __OPT_DIVISION__
	unsigned width = residuals.width() >> 1;
	unsigned height = residuals.height() >> 1;
#else
	unsigned width = residuals.width() / 2;
	unsigned height = residuals.height() / 2;
#endif

	SurfaceView<int16_t> src(residuals);
	// clang-format off
  static const int32_t basis[4][4] = {
      {+2, +2,  0,  0},   // 0,0
      {+1, -1, +1, -1},   // 1,0
      {+1, -1, -1, +1},   // 0,1
      { 0,  0, +2, +2},   // 1,1
  };
	// clang-format on

	for (unsigned l = 0; l < 4; ++l) {
		if (encode_flags.encode_residual(l)) {
			layers[l] = Surface::build_from<int16_t>()
			                .generate(width, height,
			                          [&](unsigned x, unsigned y) -> int16_t {
				                          return (src.read(x * 2 + 0, y * 2 + 0) * basis[l][0] +
				                                  src.read(x * 2 + 1, y * 2 + 0) * basis[l][1] +
				                                  src.read(x * 2 + 0, y * 2 + 1) * basis[l][2] +
				                                  src.read(x * 2 + 1, y * 2 + 1) * basis[l][3]) /
				                                 4;
			                          })
			                .finish();
		} else {
			layers[l] = Surface::build_from<int16_t>()
			                .generate(width, height, [](unsigned x, unsigned y) -> int16_t { return 0; })
			                .finish();
		}
	}
}

} // namespace lctm
