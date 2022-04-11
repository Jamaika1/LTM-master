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
// Downsampling.cpp
//

#include "Downsampling.hpp"
#include "Convert.hpp"
#include "Misc.hpp"

#include <algorithm>
#include <vector>

using namespace std;

namespace lctm {

struct DownsampleKernel {
	uint8_t length;
	int8_t offset;
	int16_t taps[12];
};

static const DownsampleKernel downsample_kernels[] = {
    {2, 0, {8192, 8192}},                                                           // Downsample_Area
    {8, -3, {-383, -399, 2466, 6506, 6506, 2466, -399, -383}},                      // Downsample_Lanczos
    {12, -5, {60, 247, -557, -1092, 2220, 7314, 7314, 2220, -1092, -557, 247, 60}}, // Downsample_Lanczos3
};

// Apply 1D kernel to a [src, src+size] stripe, writing to dest.
//
static void apply_kernel(int16_t *dest, unsigned dest_stride, const int16_t *src, unsigned src_stride, unsigned size,
                         const DownsampleKernel &kernel) {

	for (int s = 0; s < (int)size; ++s, dest += dest_stride) {

		int64_t d = 0;
		for (int k = 0; k < kernel.length; ++k) {
			const int o = clamp(s * 2 + k + kernel.offset, 0, (int)size * 2 - 1);
			d += kernel.taps[k] * (int32_t)src[src_stride * o];
		}

		*dest = shift_clamp_s16(d, 14);
	}
}

Surface Downsampling::process(const Surface &src_plane, Downsample downsample) {
	CHECK(downsample >= Downsample_Area && downsample <= Downsample_Lanczos3);

	const DownsampleKernel &kernel = downsample_kernels[downsample];
	const unsigned src_width = src_plane.width();
	const unsigned src_height = src_plane.height();
#if defined __OPT_DIVISION__
	const unsigned dst_width = (src_width + 1) >> 1;
	const unsigned dst_height = (src_height + 1) >> 1;
#else
	const unsigned dst_width = (src_width + 1) / 2;
	const unsigned dst_height = (src_height + 1) / 2;
#endif

	// Intermediate is w/2,h
	auto h_src = src_plane.view_as<int16_t>();
	auto h_dst = Surface::build_from<int16_t>();
	h_dst.reserve(dst_width, src_height);

	// Horizontal
	//
	for (unsigned y = 0; y < src_height; ++y)
		apply_kernel(h_dst.data(0, y), 1, h_src.data(0, y), 1, dst_width, kernel);

	Surface intermediate = h_dst.finish();

	// Final is w/2,h/2
	auto v_src = intermediate.view_as<int16_t>();
	auto v_dst = Surface::build_from<int16_t>();
	v_dst.reserve(dst_width, dst_height);

	// Vertical
	//
	for (unsigned x = 0; x < dst_width; ++x)
		apply_kernel(v_dst.data(x, 0), dst_width, v_src.data(x, 0), dst_width, dst_height, kernel);

	return v_dst.finish();
}

Surface Downsampling_1D::process(const Surface &src_plane, Downsample downsample) {
	CHECK(downsample >= Downsample_Area && downsample <= Downsample_Lanczos3);

	const DownsampleKernel &kernel = downsample_kernels[downsample];
	const unsigned src_width = src_plane.width();
#if defined __OPT_DIVISION__
	const unsigned dst_width = (src_width + 1) >> 1;
#else
	const unsigned dst_width = (src_width + 1) / 2;
#endif
	const unsigned height = src_plane.height();

	// Output is w/2,h
	auto src = src_plane.view_as<int16_t>();
	auto dst = Surface::build_from<int16_t>();
	dst.reserve(dst_width, height);

	for (unsigned y = 0; y < height; ++y)
		apply_kernel(dst.data(0, y), 1, src.data(0, y), 1, dst_width, kernel);

	return dst.finish();
}

// Downsample image according to current settings
//
Image DownsampleImage(const Image &src, Downsample downsample_luma, Downsample downsample_chroma, ScalingMode scaling_mode,
                      unsigned dst_bit_depth) {
	unsigned src_bit_depth = src.description().bit_depth();
	if (dst_bit_depth == 0 || dst_bit_depth == src_bit_depth) {
		if (scaling_mode == ScalingMode_None)
			return src;
		else
			dst_bit_depth = src_bit_depth; // Same input/output bit depths
	}

	vector<Surface> downsampled_surfaces;
		for (unsigned p = 0; p < src.description().num_planes(); ++p) {
		    if (scaling_mode != ScalingMode_None) {
			    const Surface s = ConvertToInternal().process(src.plane(p), src_bit_depth);
			    Surface scaled;

			    switch (scaling_mode) {
			    case ScalingMode_1D:
				    scaled = Downsampling_1D().process(s, p == 0 ? downsample_luma : downsample_chroma);
				    break;

			    case ScalingMode_2D:
				    scaled = Downsampling().process(s, p == 0 ? downsample_luma : downsample_chroma);
				    break;

			    default:
				    CHECK(0);
				    break;
			    }

			    downsampled_surfaces.push_back(ConvertFromInternal().process(scaled, dst_bit_depth));
		    } else {
			    // No Downsampling, only bit shifting
			    downsampled_surfaces.push_back(ConvertBitShift().process(src.plane(p), src_bit_depth, dst_bit_depth));
		    }
	    }

	    const ImageDescription ds_desc(src.description().with_depth(dst_bit_depth).format(), downsampled_surfaces[0].width(),
	                                   downsampled_surfaces[0].height());

	    return Image("downsampled", ds_desc, src.timestamp(), downsampled_surfaces);
}

} // namespace lctm
