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
// Compare.cpp
//

#include "Compare.hpp"
#include "ResidualMap.hpp"
#include "TemporalDecode.hpp"

namespace lctm {

// Generate a new u8 mask plane from a "greater or equal" comparison of two other planes
// sad(I) >= sad(P) --> use pred
Surface CompareGE::process(const Surface &plane_a, const Surface &plane_b) {
	const auto a = plane_a.view_as<int16_t>();
	const auto b = plane_b.view_as<int16_t>();

	CHECK(a.width() == b.width() && a.height() == b.height());

	return Surface::build_from<uint8_t>()
	    .generate(a.width(), a.height(),
	              [&](unsigned x, unsigned y) -> uint8_t {
		              uint8_t ucReturn = TemporalType::TEMPORAL_INTR;
		              if ((int32_t)(a.read(x, y)) >= (int32_t)(b.read(x, y))) {
			              ucReturn = TemporalType::TEMPORAL_PRED;
		              }
		              return ucReturn;
	              })
	    .finish();
}

// Generate a new u8 mask plane from a "lower or equal" comparison of two other planes
// sad(P) <= sad(I) --> use pred
Surface CompareLE::process(const Surface &plane_a, const Surface &plane_b) {
	const auto a = plane_a.view_as<int16_t>();
	const auto b = plane_b.view_as<int16_t>();

	CHECK(a.width() == b.width() && a.height() == b.height());

	return Surface::build_from<uint8_t>()
	    .generate(a.width(), a.height(),
	              [&](unsigned x, unsigned y) -> uint8_t {
		              uint8_t ucReturn = TemporalType::TEMPORAL_INTR;
		              if ((int32_t)(b.read(x, y)) <= (int32_t)(a.read(x, y))) {
			              ucReturn = TemporalType::TEMPORAL_PRED;
		              }
		              return ucReturn;
	              })
	    .finish();
}

// Generate a new u8 mask plane
Surface CompareSkip::process(const Surface &plane_a, const Surface &plane_b, unsigned block_size) {
	const auto a = plane_a.view_as<int16_t>();
	const auto b = plane_b.view_as<int16_t>();

	CHECK(a.width() == b.width() && a.height() == b.height());

	return Surface::build_from<uint8_t>()
	    .generate(a.width(), a.height(),
	              [&](unsigned x, unsigned y) -> uint8_t {
		              int iReturn = ResidualLabel::RESIDUAL_LIVE;

		              if ((int32_t)(a.read(x, y)) <= (int32_t)(b.read(x, y))) {
			              if ((int32_t)(a.read(x, y)) <= (int)(4 * (1 << 7) * block_size * block_size))
				              iReturn = ResidualLabel::RESIDUAL_KILL;
		              } else {
			              if ((int32_t)(b.read(x, y)) <= (int)(4 * (1 << 7) * block_size * block_size))
				              iReturn = ResidualLabel::RESIDUAL_KILL;
		              }

		              return iReturn;
	              })
	    .finish();
}

} // namespace lctm
