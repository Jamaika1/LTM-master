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
// InverseQuantize.cpp
//

#include "InverseQuantize.hpp"

#include <cmath>

#include "Misc.hpp"
#include "Surface.hpp"
#include "TemporalDecode.hpp"

// clang-format off
const uint8_t default_qm_coefficient_4x4[3][16] = {
    {   13u,    26u,    19u,    32u,    52u,     1u,    78u,     9u,    13u,    26u,    19u,    32u,   150u,    91u,    91u,    19u},
	{   13u,    26u,    19u,    32u,    52u,     1u,    78u,     9u,    26u,    72u,     0u,     3u,   150u,    91u,    91u,    19u},
	{    0u,     0u,     0u,     2u,    52u,     1u,    78u,     9u,    26u,    72u,     0u,     3u,   150u,    91u,    91u,    19u}
};

const uint8_t default_qm_coefficient_2x2[3][4] = {
	{    0u,     2u,     0u,     0u},
	{   32u,     3u,     0u,    32u},
	{    0u,     3u,     0u,    32u}
};
// clang-format on

namespace lctm {

int32_t find_quant_matrix_coeff(const PictureConfiguration &picture_configuration, unsigned num_layers, bool horizontal_only,
                                unsigned loq, unsigned layer, bool is_idr, int32_t previous) {

	const uint8_t *d = nullptr;
	if (num_layers == 4) {
		// 2x2 case
		if (loq == LOQ_LEVEL_1) {
			d = default_qm_coefficient_2x2[2]; // LOQ1
		} else {
			if (horizontal_only)
				d = default_qm_coefficient_2x2[0]; // LOQ2 1D
			else
				d = default_qm_coefficient_2x2[1]; // LOQ2 2D
		}
	} else {
		// 4x4 case
		if (loq == LOQ_LEVEL_1) {
			d = default_qm_coefficient_4x4[2]; // LOQ1
		} else {
			if (horizontal_only)
				d = default_qm_coefficient_4x4[0]; // LOQ2 1D
			else
				d = default_qm_coefficient_4x4[1]; // LOQ2 2D
		}
	}

	switch (picture_configuration.quant_matrix_mode) {
	case QuantMatrix_BothPrevious:
		if (is_idr)
			return (int32_t)d[layer];
		else
			return (previous == -1) ? (int32_t)d[layer] : previous;

	case QuantMatrix_BothDefault:
		return (int32_t)d[layer];

	case QuantMatrix_SameAndCustom:
		return picture_configuration.qm_coefficient_2[layer];

	case QuantMatrix_Level2CustomLevel1Default:
		if (loq == LOQ_LEVEL_2)
			return picture_configuration.qm_coefficient_2[layer];
		else
			return d[layer];

	case QuantMatrix_Level2DefaultLevel1Custom:
		if (loq == LOQ_LEVEL_1)
			return picture_configuration.qm_coefficient_1[layer];
		else
			return d[layer];

	case QuantMatrix_DifferentAndCustom:
		if (loq == LOQ_LEVEL_2)
			return picture_configuration.qm_coefficient_2[layer];
		else
			return picture_configuration.qm_coefficient_1[layer];

	default:
		CHECK(0);
		return 0;
	}
}

static const int32_t Aconst = 39;
static const int32_t Bconst = 126484;
static const int32_t Cconst = 5242;
static const int32_t Dconst = 99614;

int32_t find_dirq_step_width(int32_t orig_step_width, int32_t quant_matrix_coeff) {
	int64_t dirq_step_width_64 = quant_matrix_coeff;
	dirq_step_width_64 = (dirq_step_width_64 * orig_step_width) + (1 << 16);
	dirq_step_width_64 = clamp(dirq_step_width_64, (int64_t)0, (int64_t)(3 << 16));
	dirq_step_width_64 = (dirq_step_width_64 * orig_step_width) >> 16;
	int32_t dirq_step_width_32 = (int32_t)clamp(dirq_step_width_64, (int64_t)MIN_STEP_WIDTH, (int64_t)MAX_STEP_WIDTH);
	return dirq_step_width_32;
}

int32_t find_invq_offset(const PictureConfiguration &picture_configuration, int32_t orig_step_width, int32_t dirq_step_width) {
	const bool dequant_offset_signalled = picture_configuration.dequant_offset_signalled;
	const DequantOffset dequant_offset_mode = picture_configuration.dequant_offset_mode;
	const int32_t dequant_offset = picture_configuration.dequant_offset;
	int32_t dequant_offset_return = 0;
	int32_t log_orig_sw = 0;
	int32_t log_dirq_sw = 0;
	int64_t dequant_offset64 = 0;

	if (dequant_offset_signalled) {
		if (dequant_offset == 0) {
			dequant_offset_return = dequant_offset;
		} else {
			log_orig_sw = static_cast<int32_t>(((-1) * Cconst) * log(dirq_step_width));
			log_dirq_sw = static_cast<int32_t>(((1) * Cconst) * log(orig_step_width));

			if (dequant_offset_mode == DequantOffset_ConstOffset) {
				dequant_offset64 = (dequant_offset << 9);
			} else if (dequant_offset_mode == DequantOffset_Default) {
				dequant_offset64 = (dequant_offset << 11);
			}
			dequant_offset64 = ((log_orig_sw + log_dirq_sw + dequant_offset64) * dirq_step_width);
			dequant_offset_return = static_cast<int32_t>(dequant_offset64 >> 16);
		}
	}

	return dequant_offset_return;
}

int32_t find_invq_step_width(const PictureConfiguration &picture_configuration, int32_t dirq_step_width, int32_t invq_offset) {
	const bool dequant_offset_signalled = picture_configuration.dequant_offset_signalled;
	const DequantOffset dequant_offset_mode = picture_configuration.dequant_offset_mode;
	const int32_t dequant_offset = picture_configuration.dequant_offset;
	int32_t invq_step_width = 0;

	if (dequant_offset_signalled == false) {
		int64_t modi_step_width = (int64_t)(Dconst - Cconst * log(dirq_step_width));
		modi_step_width = (modi_step_width * dirq_step_width * dirq_step_width);
#if defined __OPT_DIVISION__
		modi_step_width = (modi_step_width >> 31);
#else
		modi_step_width = (modi_step_width / 32768) >> 16;
#endif
		invq_step_width = (int32_t)clamp(dirq_step_width + modi_step_width, (int64_t)MIN_STEP_WIDTH, (int64_t)MAX_STEP_WIDTH);
	} else {
		if (dequant_offset_mode == DequantOffset_ConstOffset) {
			invq_step_width = dirq_step_width;
		} else if (dequant_offset_mode == DequantOffset_Default) {
			int64_t modi_step_width = (invq_offset * dirq_step_width);
#if defined __OPT_DIVISION__
			modi_step_width = (modi_step_width >> 15);
#else
			modi_step_width = modi_step_width / 32768;
#endif
			invq_step_width = (int32_t)clamp(dirq_step_width + modi_step_width, (int64_t)MIN_STEP_WIDTH, (int64_t)MAX_STEP_WIDTH);
		}
	}

	return invq_step_width;
}

// Derive the appropriate dead_zone from current configuration
//
int32_t find_layer_deadzone(int32_t orig_step_width, int32_t step_width) {

	int64_t deadzone;

	if (orig_step_width > 16) {
		deadzone = (((Aconst * step_width) + Bconst) >> 1);
		deadzone = ((1 << 16) - deadzone);
		deadzone = (deadzone * step_width) >> 16;
	} else {
		deadzone = (orig_step_width >> 1);
	}

	return (int32_t)deadzone;
}

int32_t find_invq_applied_offset(const PictureConfiguration &picture_configuration, int32_t invq_offset, int32_t layer_deadzone) {
	const bool dequant_offset_signalled = picture_configuration.dequant_offset_signalled;
	const DequantOffset dequant_offset_mode = picture_configuration.dequant_offset_mode;

	if ((dequant_offset_signalled == false) || (dequant_offset_signalled == true && dequant_offset_mode == DequantOffset_Default))
		return (int32_t)((-1) * layer_deadzone);
	else if (dequant_offset_signalled == true && dequant_offset_mode == DequantOffset_ConstOffset)
		return (int32_t)(invq_offset - layer_deadzone);
	else {
		CHECK(0);
		return 0;
	}
}

Surface InverseQuantize::process(const Surface &src_plane, int32_t layer_step_width, int32_t applied_dequant_offset) {

	const struct Context {
		int32_t layer_step_width;
		int32_t applied_dequant_offset;
		SurfaceView<int16_t> src;
	} ctx = {layer_step_width, applied_dequant_offset, src_plane.view_as<int16_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_plane.width(), src_plane.height());
	for (unsigned y = 0; y < src_plane.height(); ++y) {
		const int16_t *__restrict psrc = ctx.src.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_plane.width(); ++x) {
			// int16_t c = ctx.src.read(x, y);
			int16_t c = *psrc++;
#if defined __QUANT_MULTIPLY__
			int16_t sign = (c > 0) ? 1 : ((c < 0) ? -1 : 0);
			*pdst++ = clamp_int16(c * ctx.layer_step_width + sign * ctx.applied_dequant_offset);
#else
			int16_t out = 0;
			if (c > 0)
				out = (int16_t)(c * ctx.layer_step_width + ctx.applied_dequant_offset);
			else if (c < 0)
				out = (int16_t)(c * ctx.layer_step_width - ctx.applied_dequant_offset);
			// dest.write(x, y, out);
			*pdst++ = clamp_int16(out);
#endif
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        src_plane.width(), src_plane.height(),
	        [](unsigned x, unsigned y, const Context &ctx) -> int16_t {
		        int16_t c = ctx.src.read(x, y);

		        if (c > 0)
			        return clamp_int16(c * ctx.layer_step_width + ctx.applied_dequant_offset);
		        else if (c < 0)
			        return clamp_int16(c * ctx.layer_step_width - ctx.applied_dequant_offset);
		        else
			        return 0;
	        },
	        ctx)
	    .finish();
#endif
}

Surface InverseQuantize_SWM::process(const Surface &src_plane, unsigned transform_block_size, int32_t *layer_step_width,
                                     int32_t *applied_dequant_offset, const Surface &temporal_map) {

	const struct Context {
		int32_t *layer_step_width;
		int32_t *applied_dequant_offset;
		const SurfaceView<int16_t> src;
		const SurfaceView<uint8_t> mask;
	} ctx = {layer_step_width, applied_dequant_offset, src_plane.view_as<int16_t>(), temporal_map.view_as<uint8_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_plane.width(), src_plane.height());
	for (unsigned y = 0; y < src_plane.height(); ++y) {
		const int16_t *__restrict psrc = ctx.src.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_plane.width(); ++x) {
			// int16_t c = ctx.src.read(x, y);
			int16_t c = *psrc++;
			int16_t out = 0;
			if (ctx.mask.read(x, y) == TEMPORAL_PRED) {
#if defined __QUANT_MULTIPLY__
				int16_t sign = (c > 0) ? 1 : ((c < 0) ? -1 : 0);
				*pdst++ = (c * ctx.layer_step_width[0] + sign * ctx.applied_dequant_offset[0]);
#else
				if (c > 0)
					out = clamp_int16(c * ctx.layer_step_width[0] + ctx.applied_dequant_offset[0]);
				else if (c < 0)
					out = clamp_int16(c * ctx.layer_step_width[0] - ctx.applied_dequant_offset[0]);
#endif
			} else {
#if defined __QUANT_MULTIPLY__
				int16_t sign = (c > 0) ? 1 : ((c < 0) ? -1 : 0);
				*pdst++ = (c * ctx.layer_step_width[1] + sign * ctx.applied_dequant_offset[1]);
#else
				if (c > 0)
					out = clamp_int16(c * ctx.layer_step_width[1] + ctx.applied_dequant_offset[1]);
				else if (c < 0)
					out = clamp_int16(c * ctx.layer_step_width[1] - ctx.applied_dequant_offset[1]);
#endif
			}
			// dest.write(x, y, out);
			*pdst++ = out;
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        src_plane.width(), src_plane.height(),
	        [](unsigned x, unsigned y, const Context &ctx) -> int16_t {
		        int16_t c = ctx.src.read(x, y);
		        int16_t out = 0;
		        if (ctx.mask.read(x, y) == TEMPORAL_PRED) {
			        if (c > 0)
				        out = clamp_int16(c * ctx.layer_step_width[0] + ctx.applied_dequant_offset[0]);
			        else if (c < 0)
				        out = clamp_int16(c * ctx.layer_step_width[0] - ctx.applied_dequant_offset[0]);
		        } else {
			        if (c > 0)
				        out = clamp_int16(c * ctx.layer_step_width[1] + ctx.applied_dequant_offset[1]);
			        else if (c < 0)
				        out = clamp_int16(c * ctx.layer_step_width[1] - ctx.applied_dequant_offset[1]);
		        }
		        return out;
	        },
	        ctx)
	    .finish();
#endif
}

} // namespace lctm
