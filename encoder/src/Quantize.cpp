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
// Quantize.cpp
//

#include <algorithm>

#include "Misc.hpp"
#include "Quantize.hpp"
#include "TemporalDecode.hpp"

#include "InverseQuantize.hpp"

namespace lctm {

Surface Quantize::process(const Surface &src_plane, int32_t dirq_step_width, int32_t deadzone, const Surface &pixel_sad_plane,
                          unsigned transform_block_size, unsigned threshold) {

	if (pixel_sad_plane.empty() || threshold == 5) {
		// Regular Quantization
		struct Context {
			const SurfaceView<int16_t> src;
			int32_t step_width;
			int32_t deadzone;
		} context = {
		    src_plane.view_as<int16_t>(),
		    dirq_step_width,
		    deadzone,
		};

		return Surface::build_from<int16_t>()
		    .generate<Context>(
		        src_plane.width(), src_plane.height(),
		        [](unsigned x, unsigned y, const Context &c) -> int16_t {
			        const int16_t in = c.src.read(x, y);

			        int16_t sign = ((in > 0) ? 1 : ((in < 0) ? (-1) : 0));
			        int16_t out = clamp((int16_t)(sign * (std::max(0, (((sign * in + c.deadzone) / c.step_width))))),
			                            (int16_t)-8192, (int16_t)8191);

			        return out;
		        },
		        context)
		    .finish();
	} else {
		// Motion-adaptive Quantization
		CHECK(threshold > 0 && threshold < 5);
		struct Context {
			const SurfaceView<int16_t> src;
			const SurfaceView<int16_t> pixel_sad;
			int32_t step_width;
			int32_t deadzone;
			unsigned transform_block_size;
			unsigned threshold;
		} context = {
		    src_plane.view_as<int16_t>(),
		    pixel_sad_plane.view_as<int16_t>(),
		    dirq_step_width,
		    deadzone,
		    transform_block_size,
		    threshold,
		};

		return Surface::build_from<int16_t>()
		    .generate<Context>(
		        src_plane.width(), src_plane.height(),
		        [](unsigned x, unsigned y, const Context &c) -> int16_t {
			        const int16_t in = c.src.read(x, y);
			        const int16_t sad = c.pixel_sad.read(x, y);

			        int16_t sign = ((in > 0) ? 1 : ((in < 0) ? (-1) : 0));
			        int16_t out = (sign * (std::max(0, (((sign * in + c.deadzone) / c.step_width)))));

			        if (sad > (c.transform_block_size == 4 ? 200 : 100))
				        return clamp(out, (int16_t)-8192, (int16_t)8191);
			        else {
				        int16_t r_deadzone = ((int16_t)c.threshold * c.deadzone) / 5;
				        int16_t reduction = sign * (std::min(1, std::max(0, (sign * in + r_deadzone) / c.step_width)));
				        int16_t correction = sign * (std::min(1, std::max(0, (sign * in + c.deadzone) / c.step_width)));

				        out = out + reduction - correction;
				        return clamp(out, (int16_t)-8192, (int16_t)8191);
			        }
		        },
		        context)
		    .finish();
	}
}

Surface Quantize_SWM::process(const Surface &src_plane, unsigned transform_block_size, int32_t *dirq_step_width, int32_t *deadzone,
                              const Surface &temporal_mask, const Surface &pixel_sad_plane, unsigned threshold) {

	if (pixel_sad_plane.empty() || threshold == 5) {
		// Regular Quantization
		struct Context {
			const SurfaceView<int16_t> src;
			const SurfaceView<uint8_t> mask;
			unsigned d;
			int32_t *step_width;
			int32_t *deadzone;
		} context = {
		    src_plane.view_as<int16_t>(), temporal_mask.view_as<uint8_t>(), 32 / transform_block_size, dirq_step_width, deadzone,
		};

		return Surface::build_from<int16_t>()
		    .generate<Context>(
		        src_plane.width(), src_plane.height(),
		        [](unsigned x, unsigned y, const Context &c) -> int16_t {
			        const uint8_t tile_refresh = c.mask.read((x / c.d) * c.d, (y / c.d) * c.d);
			        const int16_t in = c.src.read(x, y);
			        int16_t out;

			        int16_t sign = ((in > 0) ? 1 : ((in < 0) ? (-1) : 0));
			        if (tile_refresh == TEMPORAL_PRED)
				        out = clamp((int16_t)(sign * (std::max(0, (((sign * in + c.deadzone[0]) / c.step_width[0]))))),
				                    (int16_t)-8192, (int16_t)8191);
			        else
				        out = clamp((int16_t)(sign * (std::max(0, (((sign * in + c.deadzone[1]) / c.step_width[1]))))),
				                    (int16_t)-8192, (int16_t)8191);

			        return out;
		        },
		        context)
		    .finish();
	} else {
		// Motion-adaptive Quantization
		CHECK(threshold > 0 && threshold < 5);
		struct Context {
			const SurfaceView<int16_t> src;
			const SurfaceView<int16_t> pixel_sad;
			const SurfaceView<uint8_t> mask;
			unsigned d;
			int32_t *step_width;
			int32_t *deadzone;
			unsigned threshold;
		} context = {
		    src_plane.view_as<int16_t>(),
		    pixel_sad_plane.view_as<int16_t>(),
		    temporal_mask.view_as<uint8_t>(),
		    32 / transform_block_size,
		    dirq_step_width,
		    deadzone,
		    threshold,
		};

		return Surface::build_from<int16_t>()
		    .generate<Context>(
		        src_plane.width(), src_plane.height(),
		        [](unsigned x, unsigned y, const Context &c) -> int16_t {
			        const uint8_t tile_refresh = c.mask.read((x / c.d) * c.d, (y / c.d) * c.d);
			        const int16_t in = c.src.read(x, y);
			        const int16_t sad = c.pixel_sad.read(x, y);
			        int16_t out;

			        int16_t sign = ((in > 0) ? 1 : ((in < 0) ? (-1) : 0));
			        if (tile_refresh == TEMPORAL_PRED)
				        out = (sign * (std::max(0, (((sign * in + c.deadzone[0]) / c.step_width[0])))));
			        else
				        out = (sign * (std::max(0, (((sign * in + c.deadzone[1]) / c.step_width[1])))));

			        if (sad > (c.d == 8 ? 200 : 100))
				        return clamp(out, (int16_t)-8192, (int16_t)8191);
			        else {
				        if (tile_refresh == TEMPORAL_PRED) {
					        int16_t r_deadzone = ((int16_t)c.threshold * c.deadzone[0]) / 5;
					        int16_t reduction = sign * (std::min(1, std::max(0, (sign * in + r_deadzone) / c.step_width[0])));
					        int16_t correction = sign * (std::min(1, std::max(0, (sign * in + c.deadzone[0]) / c.step_width[0])));
					        out = out + reduction - correction;
				        } else {
					        int16_t r_deadzone = ((int16_t)c.threshold * c.deadzone[1]) / 5;
					        int16_t reduction = sign * (std::min(1, std::max(0, (sign * in + r_deadzone) / c.step_width[1])));
					        int16_t correction = sign * (std::min(1, std::max(0, (sign * in + c.deadzone[1]) / c.step_width[1])));
					        out = out + reduction - correction;
				        }
				        return clamp(out, (int16_t)-8192, (int16_t)8191);
			        }
		        },
		        context)
		    .finish();
	}
}

} // namespace lctm
