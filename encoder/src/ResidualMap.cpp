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
// ResidualMap.cpp
//
// Process used kill coefficients depending on preprocessed residual map
//

#include "ResidualMap.hpp"
#include "Misc.hpp"
#include "TemporalDecode.hpp"
#include "Types.hpp"

namespace lctm {

// For each block in map - kill coefficients if corresponding block is set to 0 in map
//
Surface ApplyPreprocessedMap::process(const Surface &src_plane, const Surface &map_plane) {

	// Scale the residual map to cover source image
	//
	const unsigned block_w = tile_size(map_plane.width(), src_plane.width());
	const unsigned block_h = tile_size(map_plane.height(), src_plane.height());
	const unsigned shift_bw = log2(block_w);
	const unsigned shift_bh = log2(block_h);

	CHECK(src_plane.width() == map_plane.width() * block_w);
	CHECK(src_plane.height() == map_plane.height() * block_h);

	const auto src = src_plane.view_as<int16_t>();
	const auto map = map_plane.view_as<uint8_t>();

	return Surface::build_from<int16_t>()
	    .generate(src.width(), src.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              if (map.read(x >> shift_bw, y >> shift_bh) == ResidualLabel::RESIDUAL_KILL)
			              return 0;
		              else
			              return src.read(x, y);
	              })
	    .finish();
}

// For each block in map - zero residual if corresponding block is set to 0 in map
//
Surface ApplyResidualMap::process(const Surface &src_plane, Surface &map_plane, unsigned transform_block_size) {

	// Scale the residual map to cover source image
	//
	const unsigned block_w = tile_size(map_plane.width(), src_plane.width());
	const unsigned block_h = tile_size(map_plane.height(), src_plane.height());
	const unsigned shift_bw = log2(block_w);
	const unsigned shift_bh = log2(block_h);

	CHECK(src_plane.width() == map_plane.width() * block_w);
	CHECK(src_plane.height() == map_plane.height() * block_h);

	const auto src = src_plane.view_as<int16_t>();
	const auto map = map_plane.view_as<uint8_t>();

	return Surface::build_from<int16_t>()
	    .generate(src.width(), src.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              if (map.read(x >> shift_bw, y >> shift_bh) == ResidualLabel::RESIDUAL_KILL)
			              return 0;
		              else
			              return src.read(x, y);
	              })
	    .finish();
}

} // namespace lctm
