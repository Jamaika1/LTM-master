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
// PredictedResidual.cpp
//

#include "PredictedResidual.hpp"
#include "Misc.hpp"

namespace lctm {

// Sum each 2x2 block of pels
//
Surface PredictedResidualSum::process(const Surface &src_plane) {

	typedef SurfaceView<int16_t> Context;

	const Context ctx = src_plane.view_as<int16_t>();

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int32_t>();
	dest.reserve(src_plane.width() / 2, src_plane.height() / 2);
	int32_t *__restrict pdst = dest.data(0, 0);
	for (unsigned y = 0; y < src_plane.height() / 2; ++y) {
		const int16_t *__restrict psrc0 = ctx.data(0, 2 * y + 0);
		const int16_t *__restrict psrc1 = ctx.data(0, 2 * y + 1);
		for (unsigned x = 0; x < src_plane.width() / 2; ++x) {
#if 1
			const int32_t sum = *(psrc0 + 0) + *(psrc0 + 1) + *(psrc1 + 0) + *(psrc1 + 1);
			psrc0 += 2;
			psrc1 += 2;
			*pdst++ = sum;
#else
			const int32_t sum = *psrc0++ + *psrc0++ + *psrc1++ + *psrc1++;
			*pdst++ = sum;
#endif
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int32_t>()
	    .generate<Context>(
	        src_plane.width() / 2, src_plane.height() / 2,
	        [](unsigned x, unsigned y, const Context &ctx) -> int32_t {
		        return ctx.read(x * 2 + 0, y * 2 + 0) + ctx.read(x * 2 + 0, y * 2 + 1) + ctx.read(x * 2 + 1, y * 2 + 0) +
		               ctx.read(x * 2 + 1, y * 2 + 1);
	        },
	        ctx)
	    .finish();
#endif
}

// Sum each 2x1 block of pels
//
Surface PredictedResidualSum_1D::process(const Surface &src_plane) {
	const auto src = src_plane.view_as<int16_t>();

	return Surface::build_from<int32_t>()
	    .generate(src.width() / 2, src.height(),
	              [&](unsigned x, unsigned y) -> int32_t { return src.read(x * 2 + 0, y) + src.read(x * 2 + 1, y); })
	    .finish();
}

// Produce adjusted 2x2 upsampled layer that averages to the base layer
Surface PredictedResidualAdjust::process(const Surface &base_plane, const Surface &enhanced_plane, const Surface &sum_plane) {
	const struct Context {
		SurfaceView<int16_t> base;
		SurfaceView<int16_t> enhanced;
		SurfaceView<int32_t> sum;
	} ctx = {base_plane.view_as<int16_t>(), enhanced_plane.view_as<int16_t>(), sum_plane.view_as<int32_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(enhanced_plane.width(), enhanced_plane.height());
	const int16_t *__restrict pbas = ctx.base.data(0, 0);
	const int32_t *__restrict psum = ctx.sum.data(0, 0);
	for (unsigned y = 0; y < enhanced_plane.height() / 2; ++y) {
		const int16_t *__restrict psrc0 = ctx.enhanced.data(0, 2 * y + 0);
		const int16_t *__restrict psrc1 = ctx.enhanced.data(0, 2 * y + 1);
		int16_t *__restrict pdst0 = dest.data(0, 2 * y + 0);
		int16_t *__restrict pdst1 = dest.data(0, 2 * y + 1);
		for (unsigned x = 0; x < enhanced_plane.width() / 2; ++x) {
			const int32_t adjust = *pbas++ - ((*psum++ + 2) >> 2);
#if 1
			*(pdst0 + 0) = clamp(*(psrc0 + 0) + adjust, -32767, 32767);
			*(pdst0 + 1) = clamp(*(psrc0 + 1) + adjust, -32767, 32767);
			*(pdst1 + 0) = clamp(*(psrc1 + 0) + adjust, -32767, 32767);
			*(pdst1 + 1) = clamp(*(psrc1 + 1) + adjust, -32767, 32767);
			psrc0 += 2;
			psrc1 += 2;
			pdst0 += 2;
			pdst1 += 2;
#else
			*pdst0++ = clamp(*psrc0++ + adjust, -32767, 32767);
			*pdst0++ = clamp(*psrc0++ + adjust, -32767, 32767);
			*pdst1++ = clamp(*psrc1++ + adjust, -32767, 32767);
			*pdst1++ = clamp(*psrc1++ + adjust, -32767, 32767);
#endif
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        enhanced_plane.width(), enhanced_plane.height(),
	        [](unsigned x, unsigned y, const Context &ctx) -> int16_t {
		        // Calculate 4*delta to add to enhanced pel
		        const int32_t adjust = ctx.base.read(x / 2, y / 2) - ((ctx.sum.read(x / 2, y / 2) + 2) >> 2);
		        return clamp(ctx.enhanced.read(x, y) + adjust, -32767, 32767);
	        },
	        ctx)
	    .finish();
#endif
}

// Produce adjusted 2x1 upsampled layer that averages to the base layer
Surface PredictedResidualAdjust_1D::process(const Surface &base_plane, const Surface &enhanced_plane, const Surface &sum_plane) {
	const auto base = base_plane.view_as<int16_t>();
	const auto enhanced = enhanced_plane.view_as<int16_t>();
	const auto sum = sum_plane.view_as<int32_t>();

	return Surface::build_from<int16_t>()
	    .generate(enhanced.width(), enhanced.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              // Calculate 2*delta to add to enhanced pel
		              const int32_t adjust = base.read(x / 2, y) - ((sum.read(x / 2, y) + 1) >> 1);
		              return clamp(enhanced.read(x, y) + adjust, -32767, 32767);
	              })
	    .finish();
}

} // namespace lctm
