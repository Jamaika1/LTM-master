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
// Add.cpp
//

#include "Add.hpp"
#include "Config.hpp"

namespace lctm {

// Generate a new plane as sum of two 16 bit planes
//
Surface Add::process(const Surface &plane_a, const Surface &plane_b) {
	const struct Context {
		SurfaceView<int16_t> a;
		SurfaceView<int16_t> b;
	} ctx = {plane_a.view_as<int16_t>(), plane_b.view_as<int16_t>()};

	CHECK(plane_a.width() == plane_b.width() && plane_a.height() == plane_b.height());

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(plane_a.width(), plane_a.height());
	for (unsigned y = 0; y < plane_a.height(); ++y) {
		const int16_t *__restrict psrca = ctx.a.data(0, y);
		const int16_t *__restrict psrcb = ctx.b.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < plane_a.width(); ++x) {
			// dest.write(x, y, ctx.a.read(x, y) + ctx.b.read(x, y));
			*pdst++ = (*psrca++ + *psrcb++);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        plane_a.width(), plane_b.height(),
	        [](unsigned x, unsigned y, const Context &ctx) -> int16_t { return ctx.a.read(x, y) + ctx.b.read(x, y); }, ctx)
	    .finish();
#endif
}

Surface AddHighlight::process(const Surface &plane_a, const Surface &plane_b) {

	const struct Context {
		SurfaceView<int16_t> a;
		SurfaceView<int16_t> b;
	} ctx = {plane_a.view_as<int16_t>(), plane_b.view_as<int16_t>()};

	CHECK(plane_a.width() == plane_b.width() && plane_a.height() == plane_b.height());

	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        plane_a.width(), plane_a.height(),
	        [](unsigned x, unsigned y, const Context &ctx) -> int16_t {
		        int16_t v = ctx.b.read(x, y);
		        if (v)
			        return ~(ctx.a.read(x, y) + v);
		        else
			        return ctx.a.read(x, y);
	        },
	        ctx)
	    .finish();
}

} // namespace lctm
