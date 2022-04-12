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
//               Florian Maurer (florian.maurer@v-nova.com)
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//

#include "ParameterDefaults.hpp"
#include "Types.hpp"

#include <array>
#include <math.h>

namespace lctm {

template <typename A, typename B, size_t N> B piecewise_linear(const std::array<A, N> &a, const std::array<B, N> &b, A v) {
	CHECK(N > 0);

	// Check top and bottom of range
	if (v <= a[0])
		return b[0];
	if (v >= a[N - 1])
		return b[N - 1];

	// Find pair that input falls between
	unsigned i = 0;
	for (i = 0; i < N - 1; ++i)
		if (v < a[i + 1])
			break;

	// Lerp
	return int(v - a[i]) * (b[i + 1] - b[i]) / int(a[i + 1] - a[i]) + b[i];
}

static const std::array<unsigned, 4> step_widths = {800, 1250, 1900, 2250};

static const std::array<int, 4> dequant_offset = {50, 50, 110, 110};
static const std::array<int, 4> temporal_cq_sw_multiplier = {500, 500, 600, 700};
static const std::array<int, 4> temporal_step_width_modifier = {48, 30, 20, 10};

Parameters parameter_defaults_global(const Parameters &p) {
	auto pb = Parameters::build();

	ParameterConfig config = p["parameter_config"].get_enum<ParameterConfig>(ParameterConfig_Default);
	if (config == ParameterConfig_Default) {
		const unsigned step_width_2 = p["cq_step_width_loq_0"].get<unsigned>(32767);
		unsigned base_qp = p["qp"].get<unsigned>(28);
		unsigned planes = p["num_processed_planes"].get<unsigned>(1);
		const unsigned width = p["width"].get<unsigned>(1920);
		const unsigned height = p["height"].get<unsigned>(1080);
		const unsigned resolution = (10000 * width) + (height);
		BaseCoding base_codec = BaseCoding_AVC;
		if (lowercase(p["base_encoder"].get<std::string>()).compare("hevc") == 0 ||
		    lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_hevc") == 0)
			base_codec = BaseCoding_HEVC;
		else if (lowercase(p["base_encoder"].get<std::string>()).compare("vvc") == 0 ||
		         lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_vvc") == 0)
			base_codec = BaseCoding_VVC;
		else if (lowercase(p["base_encoder"].get<std::string>()).compare("evc") == 0 ||
		         lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_evc") == 0)
			base_codec = BaseCoding_EVC;
		else if (lowercase(p["base_encoder"].get<std::string>()).compare("x265") == 0 ||
		         lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_x265") == 0)
			base_codec = BaseCoding_EVC;

		// Base qp
		if (p["qp"].empty()) {
			if (base_codec == BaseCoding_AVC) {
				if ((resolution <= ((10000 * 1920) + (1080))))
					base_qp = (int)((0.007f * (float)(step_width_2)) + 4.85f);
				else
					base_qp = (int)((0.018079f * (float)(step_width_2)) - 33.71f);
			} else if (base_codec == BaseCoding_HEVC) {
				if ((resolution <= ((10000 * 1920) + (1080))))
					base_qp = (int)((0.012f * (float)(step_width_2)) - 15.5f);
				else
					base_qp = (int)((0.0148f * (float)(step_width_2)) - 21.751f);
			} else if (base_codec == BaseCoding_EVC) {
				if ((resolution <= ((10000 * 1920) + (1080))))
					base_qp = (int)((0.0068f * (float)(step_width_2)) + 3.9939f);
				else
					base_qp = (int)((0.0131f * (float)(step_width_2)) - 16.669f);
			} else if (base_codec == BaseCoding_VVC) {
				if ((resolution <= ((10000 * 1920) + (1080))))
					base_qp = (int)((0.012f * (float)(step_width_2)) - 15.5f);
				else
					base_qp = (int)((0.0148f * (float)(step_width_2)) - 22.07f);
			}
			base_qp = clamp(base_qp, (unsigned)MIN_BASE_QP, (unsigned)MAX_BASE_QP);
		}
		pb.set("qp", (unsigned)base_qp);

		// YUV mode (chroma planes)
		if (base_qp >= 27) {
			planes = 1;
			pb.set("num_processed_planes", (unsigned)1); // Y only
		} else {
			// (base_qp <= 26)
			planes = 3;
			pb.set("num_processed_planes", (unsigned)3); // YUV
		}

		REPORT("QP                              %8d", base_qp);
		REPORT("width                           %8d", width);
		REPORT("height                          %8d", height);
		REPORT("num_processed_planes            %8d", planes);
	}

	else if (config == ParameterConfig_Conformance) {
		REPORT("Using Conformance Parameters");
		pb.set("encoding_upsample", std::string("modifiedcubic"));
		pb.set("encoding_transform_type", std::string("dds"));
		pb.set("num_processed_planes", (unsigned)1);
		pb.set("temporal_step_width_modifier", (unsigned)48);
		pb.set("qp", (unsigned)28);
	}

	return pb.finish();
}

Parameters parameter_defaults_picture(const Parameters &p, std::vector<std::unique_ptr<Image>> &src_image, unsigned base_qp) {
	auto pb = Parameters::build();

	ParameterConfig config = p["parameter_config"].get_enum<ParameterConfig>(ParameterConfig_Default);
	if (config == ParameterConfig_Default) {
		unsigned step_width_1 = p["cq_step_width_loq_1"].get<unsigned>(32767);
		unsigned step_width_2 = p["cq_step_width_loq_0"].get<unsigned>(32767);
		unsigned transform_size = 4;
		BaseCoding base_codec = BaseCoding_AVC;
		if (lowercase(p["base_encoder"].get<std::string>()).compare("hevc") == 0 ||
		    lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_hevc") == 0)
			base_codec = BaseCoding_HEVC;
		else if (lowercase(p["base_encoder"].get<std::string>()).compare("vvc") == 0 ||
		         lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_vvc") == 0)
			base_codec = BaseCoding_VVC;
		else if (lowercase(p["base_encoder"].get<std::string>()).compare("evc") == 0 ||
		         lowercase(p["base_encoder"].get<std::string>()).compare("baseyuv_evc") == 0)
			base_codec = BaseCoding_EVC;

		// encoding_transform_type
		if (p["encoding_transform_type"].empty()) {
			if (src_image.size() > 1) {
				// Calculate SAD between frame and next frame
				const Surface src = ConvertToInternal().process(src_image[0]->plane(0), src_image[0]->description().bit_depth());
				const Surface src_next = ConvertToInternal().process(src_image[src_image.size() - 1]->plane(0),
				                                                     src_image[src_image.size() - 1]->description().bit_depth());
				Surface pixel_sad = TemporalCost_SAD().process(src, src_next, 2);
				Surface zero_sad = TemporalCost_SAD().process(src_image[0]->plane(0), Surface(), 2);
				// Derive transform type from SAD values
				CHECK(!pixel_sad.empty());
				const auto pixel_sad_view = pixel_sad.view_as<int16_t>();
				const auto zero_sad_view = zero_sad.view_as<int16_t>();
				unsigned counter_pixel = 0, counter_sad_low = 0, counter_sad_high = 0;
				int16_t threshold_high = 900 << (src_image[0]->description().bit_depth() - 8);
				for (unsigned x = 0; x < pixel_sad_view.width(); x++) {
					for (unsigned y = 0; y < pixel_sad_view.height(); y++) {
						if (pixel_sad_view.read(x, y) == 0)
							counter_pixel++;
						if (zero_sad_view.read(x, y) < 100)
							counter_sad_low++;
						else if (zero_sad_view.read(x, y) > threshold_high)
							counter_sad_high++;
					}
				}
				unsigned num_blocks = pixel_sad_view.width() * pixel_sad_view.height();
				if ((double)counter_pixel / num_blocks > 0.07 && (double)counter_sad_low / num_blocks < 0.5 &&
				    (double)counter_sad_high / num_blocks < 0.2) {
					transform_size = 2;
					pb.set("encoding_transform_type", std::string("dd"));
				} else {
					transform_size = 4;
					pb.set("encoding_transform_type", std::string("dds"));
					if (base_codec == BaseCoding_AVC || base_codec == BaseCoding_HEVC)
						step_width_2 = clamp((unsigned)(0.0001 * (double)step_width_2 * (double)step_width_2 +
						                                0.0671 * (double)step_width_2 - 0.9075),
						                     (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					else if (base_codec == BaseCoding_VVC || base_codec == BaseCoding_EVC)
						step_width_2 = clamp((unsigned)(0.0002 * (double)step_width_2 * (double)step_width_2 -
						                                0.1127 * (double)step_width_2 + 160.51),
						                     (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);

					pb.set("cq_step_width_loq_2", step_width_2);
				}
			}
		} else {
			// Transform type specified on the cmd line, read type
			const TransformType transform_type = p["encoding_transform_type"].get_enum<TransformType>(TransformType_DDS);
			switch (transform_type) {
			case TransformType_DD:
				transform_size = 2;
				break;
			case TransformType_DDS:
				transform_size = 4;
				break;
			default:
				CHECK(0);
			}
		}

		if (p["cq_step_width_loq_1"].empty()) {
			// Sub-layer 1 SW
			if (base_codec == BaseCoding_AVC) {
				if (base_qp >= 37) {
					step_width_1 = (unsigned)3600;
					pb.set("cq_step_width_loq_1", (unsigned)3600);
				} else {
					step_width_1 = (unsigned)32767;
					pb.set("cq_step_width_loq_1", (unsigned)32767);
				}
			} else if (base_codec == BaseCoding_HEVC) {
				if (base_qp >= 39) {
					step_width_1 = (unsigned)3600;
					pb.set("cq_step_width_loq_1", (unsigned)3600);
				} else {
					step_width_1 = (unsigned)32767;
					pb.set("cq_step_width_loq_1", (unsigned)32767);
				}
			} else if (base_codec == BaseCoding_EVC) {
				if (base_qp >= 40) {
					step_width_1 = (unsigned)3600;
					pb.set("cq_step_width_loq_1", (unsigned)3600);
				} else {
					step_width_1 = (unsigned)32767;
					pb.set("cq_step_width_loq_1", (unsigned)32767);
				}
			} else if (base_codec == BaseCoding_VVC) {
				if (base_qp >= 41) {
					step_width_1 = (unsigned)3600;
					pb.set("cq_step_width_loq_1", (unsigned)3600);
				} else {
					step_width_1 = (unsigned)32767;
					pb.set("cq_step_width_loq_1", (unsigned)32767);
				}
			}
		}

		// priority_map default ON
		unsigned priority_mode = 20;
		pb.set("priority_mode", std::string("mode_2_0"));

		unsigned a, b, c;
		if (transform_size == 4) {
			// parameters settings for 4x4

			// dequant_offset
			if (step_width_2 < 1900)
				pb.set("dequant_offset_mode", std::string("const_offset"));
			else
				pb.set("dequant_offset_mode", std::string("default"));
			pb.set("dequant_offset", a = (unsigned)piecewise_linear(step_widths, dequant_offset, step_width_2));

			// temporal_cq_sw_multiplier
			pb.set("temporal_cq_sw_multiplier",
			       b = (unsigned)piecewise_linear(step_widths, temporal_cq_sw_multiplier, step_width_2));

			// temporal_step_width_modifier
			pb.set("temporal_step_width_modifier",
			       c = (unsigned)piecewise_linear(step_widths, temporal_step_width_modifier, step_width_2));

			// SAD threshold
			pb.set("sad_threshold", (unsigned)12000);
			pb.set("sad_coeff_threshold", (unsigned)2);

			// upsampling kernel
			pb.set("encoding_upsample", std::string("modifiedcubic"));

			// quant_matrix
			pb.set("quant_matrix_mode", std::string("previous"));
		} else if (transform_size == 2) {
			// parameters settings for 2x2
			float x = (float)step_width_2 / 1000.0f;

			// dequant_offset
			pb.set("dequant_offset_mode", std::string("default"));
			a = (unsigned)clamp(round(45.6f * x - 73.9f), 0.0f, 100.0f);
			pb.set("dequant_offset", a);

			// temporal_cq_sw_multiplier
			b = (unsigned)(1000 * clamp((0.1290f * x * x - 0.6145f * x + 0.9270f), 0.2000f, 1.0000f));
			pb.set("temporal_cq_sw_multiplier", b);

			// temporal_step_width_modifier
			if (step_width_2 >= 3700) {
				c = 0;
			} else {
				c = (unsigned)clamp(round(-59.1f * x * x + 328.3f * x - 374.0f), 0.0f, 100.0f);
			}
			pb.set("temporal_step_width_modifier", c);

			// SAD threshold
			pb.set("sad_threshold", (unsigned)6000);
			pb.set("sad_coeff_threshold", (unsigned)2);

			// upsampling kernel
			pb.set("encoding_upsample", std::string("adaptivecubic"));
			pb.set("upsampling_coefficients", std::string("1752 14672 4049 585"));

			// quant_matrix
			pb.set("quant_matrix_mode", std::string("custom_custom"));
			pb.set("qm_coefficient_1", std::string(" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "));
			pb.set("qm_coefficient_2", std::string(" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "));
		}

		// Activate residual promotion
		pb.set("quant_reduced_deadzone", (unsigned)3);

		pb.set("dithering_control", false);
		pb.set("dithering_strength", (unsigned)0);
		pb.set("dithering_type", std::string("none"));

		REPORT("SW1                             %8d", step_width_1);
		REPORT("SW2                             %8d", step_width_2);
		REPORT("transform                       %8d", transform_size);
		REPORT("priority_mode                   %8d", priority_mode);
		REPORT("dequant_offset                  %8d", a);
		REPORT("temporal_cq_sw_multiplier       %8d", b);
		REPORT("temporal_step_width_modifier    %8d", c);
	}

	else if (config == ParameterConfig_Conformance) {
		pb.set("cq_step_width_loq_1", (unsigned)32767);
		pb.set("cq_step_width_loq_0", (unsigned)32767);
		pb.set("priority_mode", std::string("mode_0_0"));
		pb.set("temporal_cq_sw_multiplier", (unsigned)1000);
		pb.set("dequant_offset", (unsigned)0);
		pb.set("dequant_offset_mode", std::string("default"));
		pb.set("dithering_control", false);
		pb.set("dithering_strength", (unsigned)0);
		pb.set("dithering_type", std::string("none"));
		pb.set("quant_matrix_mode", std::string("previous"));
		pb.set("sad_threshold", (unsigned)0);
		pb.set("sad_coeff_threshold", (unsigned)0);
		pb.set("quant_reduced_deadzone", (unsigned)5);
	}

	return pb.finish();
}

} // namespace lctm
