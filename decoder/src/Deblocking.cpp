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
// Deblocking.cpp
//

#include "Deblocking.hpp"

namespace lctm {

// Apply 4x4 deblocking filter
//
Surface Deblocking::process(const Surface &src_plane, unsigned corner, unsigned side) {

	corner = 16 - corner;
	side = 16 - side;
	const auto src = src_plane.view_as<int16_t>();

	// clang-format off
	const uint32_t coeffs[4][4] = {
	    {corner, side, side, corner},
	    {side,     16,   16,   side},
	    {side,     16,   16,   side},
	    {corner, side, side, corner},
	};
	// clang-format on

	return Surface::build_from<int16_t>()
	    .generate(src.width(), src.height(),
#if defined __OPT_MODULO__
	              [&](unsigned x, unsigned y) -> int16_t { return (coeffs[x & 0x03][y & 0x03] * src.read(x, y)) >> 4; })
#else 
			[&](unsigned x, unsigned y) -> int16_t { return (coeffs[x % 4][y % 4] * src.read(x, y)) >> 4; })
#endif
	    .finish();
}

} // namespace lctm
