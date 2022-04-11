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
// Upsampling.cpp
//

#include "Upsampling.hpp"

#include "Convert.hpp"
#include "Misc.hpp"

#include <algorithm>
#include <vector>

using namespace std;

namespace lctm {

typedef int32_t UpsampleKernel[4];

static const UpsampleKernel upsample_kernels[] = {
    {0, 16384, 0, 0},            // Upsample_Nearest
    {0, 12288, 4096, 0},         // Upsample_Linear
    {-1382, 14285, 3942, -461},  // Upsample_Cubic
    {-2360, 15855, 4165, -1276}, // Upsample_ModifiedCubic
};

static void make_kernel(UpsampleKernel &kernel, Upsample upsample, const unsigned *coefficients) {
	CHECK(upsample >= Upsample_Nearest && upsample <= Upsample_AdaptiveCubic);

	if (upsample <= Upsample_ModifiedCubic)
		for (unsigned i = 0; i < ARRAY_SIZE(kernel); ++i)
			kernel[i] = upsample_kernels[upsample][i];
	else {
		CHECK(upsample == Upsample_AdaptiveCubic);
		CHECK(coefficients != nullptr);

		kernel[0] = -(int32_t)coefficients[0];
		kernel[1] = (int32_t)coefficients[1];
		kernel[2] = (int32_t)coefficients[2];
		kernel[3] = -(int32_t)coefficients[3];
	}
}

static inline int16_t us_shift_clamp_s16(int32_t v) {
#if 0
	v >>= 14;
	if(v > 32767)
		v = 32767;
	else if(v < -32768)
		v = -32768;
	return v;
#else
	return v >> 14;
#endif
}

// Specialised for a 4 tap kernel, offset by -1
//
static void inline apply_kernel(int16_t *__restrict dest, unsigned dest_stride, const int16_t *__restrict src, unsigned src_stride,
                                unsigned size, const UpsampleKernel &kernel) {
	if (size == 0)
		return;

	assert(size >= 4); // Head and tail have been unrolled

	// First sample
	int32_t ds = 0x2000;
	for (int k = 0; k < 4; ++k)
		ds += kernel[3 - k] * src[src_stride * clamp(k - 2, 0, (int)size - 1)];

	*dest = us_shift_clamp_s16(ds);
	dest += dest_stride;

	// Second pair
	int32_t ds0 = 0x2000, ds1 = 0x2000;
	for (int k = 0; k < 4; ++k) {
		const int32_t sample = src[src_stride * clamp(k - 1, 0, (int)size - 1)];
		ds0 += kernel[k] * sample;
		ds1 += kernel[3 - k] * sample;
	}
	*dest = us_shift_clamp_s16(ds0);
	dest += dest_stride;
	*dest = us_shift_clamp_s16(ds1);
	dest += dest_stride;

	// Pairs of output samples
	for (int s = 1; s < (int)(size - 2); ++s) {
		int32_t d0 = 0x2000, d1 = 0x2000;
		for (int k = 0; k < 4; ++k) {
			const int32_t sample = src[src_stride * (s + k - 1)];
			d0 += kernel[k] * sample;
			d1 += kernel[3 - k] * sample;
		}
		dest[0] = us_shift_clamp_s16(d0);
		dest[dest_stride] = us_shift_clamp_s16(d1);

		dest += 2 * dest_stride;
	}

	// Last pair
	int32_t de0 = 0x2000, de1 = 0x2000;
	for (int k = 0; k < 4; ++k) {
		const int32_t sample = src[src_stride * clamp(((int)size - 3) + k, 0, (int)size - 1)];
		de0 += kernel[k] * sample;
		de1 += kernel[3 - k] * sample;
	}
	*dest = us_shift_clamp_s16(de0);
	dest += dest_stride;
	*dest = us_shift_clamp_s16(de1);
	dest += dest_stride;

	// Last sample
	int32_t de = 0x2000;
	for (int k = 0; k < 4; ++k)
		de += kernel[k] * src[src_stride * clamp(((int)size - 2) + k, 0, (int)size - 1)];

	*dest = us_shift_clamp_s16(de);
}

Surface Upsampling::process(const Surface &src_plane, Upsample upsample, const unsigned *coefficients) {
	const unsigned width = src_plane.width();
	const unsigned height = src_plane.height();

	UpsampleKernel kernel;
	make_kernel(kernel, upsample, coefficients);

	// Intermediate is w,2h
	auto v_src = src_plane.view_as<int16_t>();
	auto v_dest = Surface::build_from<int16_t>();
	v_dest.reserve(width, height * 2);

	// Vertical scale
	//
	for (unsigned x = 0; x < width; ++x)
		apply_kernel(v_dest.data(x, 0), width, v_src.data(x, 0), width, height, kernel);

	Surface intermediate = v_dest.finish();

	// Final is 2w,2h
	auto h_src = intermediate.view_as<int16_t>();
	auto h_dst = Surface::build_from<int16_t>();
	h_dst.reserve(width * 2, height * 2);

	// Horizontal scale
	//
	for (unsigned y = 0; y < height * 2; ++y)
		apply_kernel(h_dst.data(0, y), 1, h_src.data(0, y), 1, width, kernel);

	return h_dst.finish();
}

Surface Upsampling_1D::process(const Surface &src_plane, Upsample upsample, const unsigned *coefficients) {
	const unsigned width = src_plane.width();
	const unsigned height = src_plane.height();

	UpsampleKernel kernel;
	make_kernel(kernel, upsample, coefficients);

	// Final is 2w,h
	auto h_src = src_plane.view_as<int16_t>();
	auto h_dest = Surface::build_from<int16_t>();
	h_dest.reserve(2 * width, height);

	// Horizontal scale
	//
	for (unsigned y = 0; y < height; ++y)
		apply_kernel(h_dest.data(0, y), 1, h_src.data(0, y), 1, width, kernel);

	return h_dest.finish();
}

Image UpsampleImage(const Image &src, Upsample upsample, const unsigned upsampling_coefficients[4], ScalingMode scaling_mode) {
	if (scaling_mode == ScalingMode_None)
		return src;

	vector<Surface> upsampled_surfaces;
	for (unsigned p = 0; p < src.description().num_planes(); ++p) {
		const Surface s = ConvertToInternal().process(src.plane(p), src.description().bit_depth());
		Surface scaled;

		switch (scaling_mode) {
		case ScalingMode_1D:
			scaled = Upsampling_1D().process(s, upsample, upsampling_coefficients);
			break;

		case ScalingMode_2D:
			scaled = Upsampling().process(s, upsample, upsampling_coefficients);
			break;

		default:
			CHECK(0);
			break;
		}

		upsampled_surfaces.push_back(ConvertFromInternal().process(scaled, src.description().bit_depth()));
	}

	const ImageDescription us_desc(src.description().format(), upsampled_surfaces[0].width(), upsampled_surfaces[0].height());

	return Image("upsampled", us_desc, src.timestamp(), upsampled_surfaces);
}

} // namespace lctm
