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
// Decoder.cpp
//
// Decoder implementation - marshals other objects to decode base+enhancement to YUV image
//

#include "Decoder.hpp"

#include "Add.hpp"
#include "Conform.hpp"
#include "Convert.hpp"
#include "Deblocking.hpp"
#include "Deserializer.hpp"
#include "Diagnostics.hpp"
#include "Dithering.hpp"
#include "Expand.hpp"
#include "InverseQuantize.hpp"
#include "InverseTransformDD.hpp"
#include "InverseTransformDDS.hpp"
#include "InverseTransformDDS_1D.hpp"
#include "InverseTransformDD_1D.hpp"
#include "LcevcMd5.hpp"
#include "PredictedResidual.hpp"
#include "TemporalDecode.hpp"
#include "Upsampling.hpp"

#include <cstring>
#include <memory>
#include <queue>
#include <vector>

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern priority_queue<ReportStructure, vector<ReportStructure, allocator<ReportStructure>>, ReportStructureComp> goReportQueue;

namespace lctm {

Decoder::Decoder() {
	// Make sure all configuration settings are at a sensible setting
	memset(&configuration_, 0, sizeof(configuration_));
}

// Size of transform block
//
unsigned Decoder::transform_block_size() const { return configuration_.global_configuration.transform_block_size; }

unsigned Decoder::num_residual_layers() const { return configuration_.global_configuration.num_residual_layers; }

bool Decoder::is_tiled() const { return configuration_.global_configuration.tile_dimensions_type != TileDimensions_None; }

// Does this layer has user_data embedded?
//
bool Decoder::is_user_data_layer(unsigned loq, unsigned layer) const {
	if (loq == LOQ_LEVEL_1 && configuration_.global_configuration.user_data_enabled != UserData_None) {
		if (configuration_.global_configuration.transform_block_size == 4)
			return (layer == 5);
		else
			return (layer == 1);
	}
	return false;
}

// Derive and return temporal mask
//
Surface Decoder::get_temporal_mask(Surface temporal_symbols) {
	if (!configuration_.global_configuration.temporal_enabled)
		return Surface();
	else if (configuration_.picture_configuration.temporal_signalling_present)
		return temporal_symbols;
	else if (configuration_.picture_configuration.temporal_refresh)
		return Surface::build_from<uint8_t>()
		    .fill(TEMPORAL_INTR, configuration_.global_configuration.resolution_width / transform_block_size(),
		          configuration_.global_configuration.resolution_height / transform_block_size())
		    .finish();
	else
		return Surface::build_from<uint8_t>()
		    .fill(TEMPORAL_PRED, configuration_.global_configuration.resolution_width / transform_block_size(),
		          configuration_.global_configuration.resolution_height / transform_block_size())
		    .finish();
}

// Decode residuals of an enhancement sub-layer
//
Surface Decoder::decode_residuals(unsigned plane, unsigned loq, Surface &temporal_mask, Surface symbols[MAX_NUM_LAYERS]) {
#if defined __OPT_INPLACE__
#else
	Surface coefficients[MAX_NUM_LAYERS];
#endif
	int32_t dirq_step_width[MAX_NUM_LAYERS][2];
	int32_t invq_step_width[MAX_NUM_LAYERS][2];
	int32_t invq_offset[MAX_NUM_LAYERS][2];
	int32_t invq_deadzone[MAX_NUM_LAYERS][2];
	int32_t invq_applied_offset[MAX_NUM_LAYERS][2];
	const bool horizontal_only = (configuration_.global_configuration.scaling_mode[loq] == ScalingMode_1D ? true : false);
	unsigned passes = 1;

	// Get temporal mask
	if (loq == LOQ_LEVEL_2 && configuration_.global_configuration.temporal_enabled) {
		temporal_mask = get_temporal_mask(symbols[num_residual_layers()]);
	}

	// Apply temporal step width modifier, if required
	{
		int16_t step_width[2];
		int16_t calculated_step_width = configuration_.picture_configuration.step_width_loq[loq];

		if (loq == LOQ_LEVEL_2 && plane > 0) {
			calculated_step_width =
			    clamp((int16_t)((calculated_step_width * configuration_.global_configuration.chroma_step_width_multiplier) >> 6),
			          (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
		}

		if (loq == LOQ_LEVEL_2 && configuration_.global_configuration.temporal_enabled &&
		    !configuration_.picture_configuration.temporal_refresh) {
			step_width[0] =
			    clamp((int16_t)(calculated_step_width *
			                    (1 - clamp(((float)configuration_.global_configuration.temporal_step_width_modifier / (float)255),
			                               (float)0, (float)0.5))),
			          (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
			step_width[1] = calculated_step_width;
			passes = 2;
		} else {
			step_width[0] = calculated_step_width;
		}

		for (unsigned pass = 0; pass < passes; pass++) {
			for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
				dirq_step_width[layer][pass] = find_dirq_step_width(step_width[pass], quant_matrix_coeffs_[plane][loq][layer]);
				invq_offset[layer][pass] =
				    find_invq_offset(configuration_.picture_configuration, step_width[pass], dirq_step_width[layer][pass]);
				invq_step_width[layer][pass] = find_invq_step_width(configuration_.picture_configuration,
				                                                    dirq_step_width[layer][pass], invq_offset[layer][pass]);
				invq_deadzone[layer][pass] = find_layer_deadzone(step_width[pass], invq_step_width[layer][pass]);
				invq_applied_offset[layer][pass] = find_invq_applied_offset(configuration_.picture_configuration,
				                                                            invq_offset[layer][pass], invq_deadzone[layer][pass]);
			}
		}
	}

	for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
#if defined __OPT_INPLACE__
		auto view = symbols[layer].view_as<int16_t>();
		// Extract UserData if it is embedded in the coefficients
		if (is_user_data_layer(loq, layer)) {
			unsigned size;
			switch (configuration_.global_configuration.user_data_enabled) {
			case lctm::UserData_2bits:
				size = 2;
				break;
			case lctm::UserData_6bits:
				size = 6;
				break;
			default:
				CHECK(0);
				break;
			}

			{
#if USER_DATA_EXTRACTION
				FILE *file = fopen("userdata_dec.bin", "ab");
#endif
				int16_t *__restrict pview = (int16_t *)view.data(0, 0);
				for (unsigned y = 0; y < view.height() * view.width(); ++y) {
					uint16_t value = *pview;
#if USER_DATA_EXTRACTION
					unsigned char data = value & (size == 6 ? 0x3f : 0x03);
					fwrite(&data, 1, 1, file);
#endif
					value >>= size;
					bool sign = (value & 0x01) == 0x00 ? false : true;
					value >>= 1;
					*pview++ = (int16_t)(sign ? (-value) : (value));
				}
#if USER_DATA_EXTRACTION
				fclose(file);
#endif
			}
		}
#else
		Surface syms;
		// Extract UserData if it is embedded in the coefficients
		if (is_user_data_layer(loq, layer)) {
			syms = UserDataClear().process(symbols[layer], configuration_.global_configuration.user_data_enabled);
		} else {
			syms = symbols[layer];
		}
#endif

		// Dequantize
		if (passes == 1) {
#if defined __OPT_INPLACE__
			{
				int16_t *__restrict pview = (int16_t *)view.data(0, 0);
				for (unsigned y = 0; y < view.height() * view.width(); ++y) {
					int16_t coef = *pview;
					int32_t res = (coef * invq_step_width[layer][0]) +
					              (coef > 0 ? invq_applied_offset[layer][0] : (coef < 0 ? -invq_applied_offset[layer][0] : 0));
					*pview++ = clamp_int16(res);
				}
			}
#else
			coefficients[layer] = InverseQuantize().process(syms, invq_step_width[layer][0], invq_applied_offset[layer][0]);
#endif
		} else {
#if defined __OPT_INPLACE__
			{
				auto mask = temporal_mask.view_as<uint8_t>();
				int16_t *__restrict pview = (int16_t *)view.data(0, 0);
				for (unsigned y = 0; y < view.height(); ++y) {
					for (unsigned x = 0; x < view.width(); ++x) {
						unsigned sw_index = (mask.read(x, y) == TEMPORAL_PRED) ? 0 : 1;
						int16_t coef = *pview;

						*pview++ = clamp_int16(coef * invq_step_width[layer][sw_index] +
						                       (coef > 0 ? invq_applied_offset[layer][sw_index]
						                                 : (coef < 0 ? -invq_applied_offset[layer][sw_index] : 0)));
					}
				}
			}
#else
			coefficients[layer] = InverseQuantize_SWM().process(syms, transform_block_size(), invq_step_width[layer],
			                                                    invq_applied_offset[layer], temporal_mask);
#endif
		}
	}

	Surface residuals;

#if defined __OPT_INPLACE__
	if (!horizontal_only) {
		// Inverse Transform Horizontal and Vertical
		//
		if (configuration_.global_configuration.transform_block_size == 4)
			residuals =
			    InverseTransformDDS().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq), symbols);
		else if (configuration_.global_configuration.transform_block_size == 2)
			residuals =
			    InverseTransformDD().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq), symbols);
	} else {
		// Inverse Transform Horizontal only
		//
		if (configuration_.global_configuration.transform_block_size == 4)
			residuals = InverseTransformDDS_1D().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq),
			                                             symbols);
		else if (configuration_.global_configuration.transform_block_size == 2)
			residuals =
			    InverseTransformDD_1D().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq), symbols);
	}
#else
	if (!horizontal_only) {
		// Inverse Transform Horizontal and Vertical
		//
		if (configuration_.global_configuration.transform_block_size == 4)
			residuals = InverseTransformDDS().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq),
			                                          coefficients);
		else if (configuration_.global_configuration.transform_block_size == 2)
			residuals = InverseTransformDD().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq),
			                                         coefficients);
	} else {
		// Inverse Transform Horizontal only
		//
		if (configuration_.global_configuration.transform_block_size == 4)
			residuals = InverseTransformDDS_1D().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq),
			                                             coefficients);
		else if (configuration_.global_configuration.transform_block_size == 2)
			residuals = InverseTransformDD_1D().process(dimensions_.plane_width(plane, loq), dimensions_.plane_height(plane, loq),
			                                            coefficients);
	}
#endif

	return residuals;
}

void Decoder::initialize_decode(const Packet &enhancement_data, Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {
	// Parse the bitstream -- XXX check for seeing blocks in correct order
	// Deserializer will popluate configuration_ and symbols during parsing

	// Initialize quantization matrix
	std::fill_n(&quant_matrix_coeffs_[0][0][0], MAX_NUM_PLANES * MAX_NUM_LOQS * MAX_NUM_LAYERS, -1);

	Deserializer deserializer(enhancement_data, configuration_, symbols);

	while (deserializer.has_more()) {
		const unsigned block = deserializer.parse_block();

		// Has there been a new global config?
		if (block == SyntaxBlock_Global) {
			// Resize surfaces
			dimensions_.set(configuration_, configuration_.global_configuration.resolution_width,
			                configuration_.global_configuration.resolution_height);

			// Copy coordinates and sizes into layer surface info
			for (unsigned plane = 0; plane < configuration_.global_configuration.num_image_planes; ++plane) {
				for (unsigned loq = 0; loq < 2; ++loq) {
					for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
						configuration_.surface_configuration[plane][loq][layer].width = dimensions_.layer_width(plane, loq);
						configuration_.surface_configuration[plane][loq][layer].height = dimensions_.layer_height(plane, loq);
					}
				}
				// Is there a temporal layer?
				if (configuration_.global_configuration.temporal_enabled &&
				    plane < configuration_.global_configuration.num_processed_planes) {

					configuration_.surface_configuration[plane][LOQ_LEVEL_2][num_residual_layers()].width =
					    dimensions_.layer_width(plane, LOQ_LEVEL_2);
					configuration_.surface_configuration[plane][LOQ_LEVEL_2][num_residual_layers()].height =
					    dimensions_.layer_height(plane, LOQ_LEVEL_2);

					if (temporal_buffer_[plane].empty() ||
					    dimensions_.plane_width(plane, LOQ_LEVEL_2) != temporal_buffer_[plane].width() ||
					    dimensions_.plane_height(plane, LOQ_LEVEL_2) != temporal_buffer_[plane].height())
						temporal_buffer_[plane] =
						    Surface::build_from<int16_t>()
						        .reserve(dimensions_.plane_width(plane, LOQ_LEVEL_2), dimensions_.plane_height(plane, LOQ_LEVEL_2))
						        .finish();
				}
			}
		}
	}
}

Image Decoder::decode(const Image &ext_base, Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS],
                      const Image &src_image, bool report, bool dithering_switch, bool dithering_fixed, bool apply_enhancement) {

	CHECK(configuration_.global_configuration.transform_block_size == 4 ||
	      configuration_.global_configuration.transform_block_size == 2);

	if (dithering_.getInitialised() == false) {
		INFO("Dither init %4d  bitdepth %d", configuration_.picture_configuration.dithering_strength,
		     configuration_.global_configuration.enhancement_depth);
		dithering_.make_buffer(configuration_.picture_configuration.dithering_strength,
		                       configuration_.global_configuration.enhancement_depth, dithering_fixed);
		dithering_.setInitialised(true);
	}

	const bool is_idr = configuration_.picture_configuration.coding_type == CodingType::CodingType_IDR;

	// Verify dimensions of base image
	CHECK(ext_base.description().width() == dimensions_.base_width());
	CHECK(ext_base.description().height() == dimensions_.base_height());

	// Process each plane...
	//
	Surface base_reco[MAX_NUM_PLANES];
	Surface full_reco[MAX_NUM_PLANES];
	Surface outp_reco[MAX_NUM_PLANES];

	for (unsigned plane = 0; plane < ext_base.description().num_planes(); ++plane) {
		const bool enhancement_enabled = configuration_.picture_configuration.enhancement_enabled &&
		                                 plane < configuration_.global_configuration.num_processed_planes;

		// Convert between base and enhancement bit depth
		Surface base_plane;
		unsigned base_bit_depth = configuration_.global_configuration.base_depth;
		if (configuration_.global_configuration.enhancement_depth > configuration_.global_configuration.base_depth &&
		    configuration_.global_configuration.level1_depth_flag) {
			base_plane = ConvertBitShift().process(ext_base.plane(plane), configuration_.global_configuration.base_depth,
			                                       configuration_.global_configuration.enhancement_depth);
			base_bit_depth = configuration_.global_configuration.enhancement_depth;
		}

		// Base + Correction
		base_plane = ConvertToInternal().process(
		    base_bit_depth == configuration_.global_configuration.base_depth ? ext_base.plane(plane) : base_plane, base_bit_depth);

		//// Upsample from decoded base picture to preliminary intermediate picture
		//
		Surface base_upsampled;
		switch (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_1]) {
		case ScalingMode_1D:
			base_upsampled = Upsampling_1D().process(base_plane, configuration_.global_configuration.upsample,
			                                         configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				base_upsampled = PredictedResidualAdjust_1D().process(base_plane, base_upsampled,
				                                                      PredictedResidualSum_1D().process(base_upsampled));
			}
			base_plane.dump(format("dec_base_deco_P%1d", plane));
			break;
		case ScalingMode_2D:
			base_upsampled = Upsampling().process(base_plane, configuration_.global_configuration.upsample,
			                                      configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				base_upsampled =
				    PredictedResidualAdjust().process(base_plane, base_upsampled, PredictedResidualSum().process(base_upsampled));
			}
			base_plane.dump(format("dec_base_deco_P%1d", plane));
			break;
		case ScalingMode_None:
			base_upsampled = base_plane;
			break;
		default:
			CHECK(0);
		}

		base_upsampled.dump(format("dec_base_pred_P%1d", plane));

		// Work out quantization matrix for each LoQ
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
			const bool horizontal_only = (configuration_.global_configuration.scaling_mode[loq] == ScalingMode_1D ? true : false);
			for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
				quant_matrix_coeffs_[plane][loq][layer] =
				    find_quant_matrix_coeff(configuration_.picture_configuration, num_residual_layers(), horizontal_only, loq,
				                            layer, is_idr, quant_matrix_coeffs_[plane][loq][layer]);
			}
		}

		//// Enhancement sub-layer 1 decoding
		//
		if (enhancement_enabled && apply_enhancement) {
			// Base residuals
			Surface unused_mask;
			Surface residuals = decode_residuals(plane, LOQ_LEVEL_1, unused_mask, symbols[plane][LOQ_LEVEL_1]);

			// Deblocking
			if (configuration_.picture_configuration.level_1_filtering_enabled &&
			    configuration_.global_configuration.transform_block_size == 4) {
				residuals = Deblocking().process(residuals, configuration_.global_configuration.level_1_filtering_first_coefficient,
				                                 configuration_.global_configuration.level_1_filtering_second_coefficient);
			}

			// Add to (upsampled) base
			residuals.dump(format("dec_base_resi_reco_P%1d", plane));
#if defined __OPT_INPLACE__
			{
				auto viewa = base_upsampled.view_as<int16_t>();
				auto viewb = residuals.view_as<int16_t>();
				const int16_t *__restrict psrcb = viewb.data(0, 0);
				int16_t *__restrict pdst = (int16_t *)viewa.data(0, 0);
				for (unsigned y = 0; y < base_upsampled.height() * base_upsampled.width(); ++y) {
					*pdst++ += (*psrcb++);
				}
			}
			base_reco[plane] = base_upsampled;
#else
			base_reco[plane] = Add().process(base_upsampled, residuals);
#endif
		} else {
			base_reco[plane] = base_upsampled;
		}
	}

	//// Upsample from combined intermediate picture to preliminary output picture
	//
	Surface upsampled_planes[MAX_NUM_PLANES];
	for (unsigned plane = 0; plane < ext_base.description().num_planes(); ++plane) {
		switch (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2]) {
		case ScalingMode_1D:
			upsampled_planes[plane] = Upsampling_1D().process(base_reco[plane], configuration_.global_configuration.upsample,
			                                                  configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				upsampled_planes[plane] = PredictedResidualAdjust_1D().process(
				    base_reco[plane], upsampled_planes[plane], PredictedResidualSum_1D().process(upsampled_planes[plane]));
			}
			break;
		case ScalingMode_2D:
			upsampled_planes[plane] = Upsampling().process(base_reco[plane], configuration_.global_configuration.upsample,
			                                               configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				upsampled_planes[plane] = PredictedResidualAdjust().process(
				    base_reco[plane], upsampled_planes[plane], PredictedResidualSum().process(upsampled_planes[plane]));
			}
			break;
		case ScalingMode_None:
			upsampled_planes[plane] = base_reco[plane];
			break;
		default:
			CHECK(0);
		}
		upsampled_planes[plane].dump(format("dec_full_pred_P%1d", plane));
	}

	for (unsigned plane = 0; plane < ext_base.description().num_planes(); ++plane) {
		const bool enhancement_enabled = configuration_.picture_configuration.enhancement_enabled &&
		                                 plane < configuration_.global_configuration.num_processed_planes;

		//// Enhancement sub-layer 2 decoding
		//
		if (enhancement_enabled && apply_enhancement) {
			// Enhacement residuals
			Surface temporal_mask;
			Surface residuals = decode_residuals(plane, LOQ_LEVEL_2, temporal_mask, symbols[plane][LOQ_LEVEL_2]);
			residuals.dump(format("dec_full_resi_reco_P%1d", plane));

			if (configuration_.global_configuration.temporal_enabled) {
				// Apply temporal map (intra / pred)
				CHECK(!temporal_mask.empty());
				if (temporal_buffer_[plane].empty()) {
					temporal_buffer_[plane] = Surface::build_from<int16_t>()
					                              .generate(upsampled_planes[plane].width(), upsampled_planes[plane].height(),
					                                        [&](unsigned x, unsigned y) -> int16_t { return 0; })
					                              .finish();
				}
				temporal_buffer_[plane] =
				    ApplyTemporalMap().process(temporal_buffer_[plane], temporal_mask, transform_block_size());
#if defined __OPT_INPLACE__
				{
					auto viewa = temporal_buffer_[plane].view_as<int16_t>();
					auto viewb = residuals.view_as<int16_t>();
					const int16_t *__restrict psrcb = viewb.data(0, 0);
					int16_t *__restrict pdst = (int16_t *)viewa.data(0, 0);
					for (unsigned y = 0; y < temporal_buffer_[plane].height() * temporal_buffer_[plane].width(); ++y) {
						*pdst++ += (*psrcb++);
					}
				}
#else
				temporal_buffer_[plane] = Add().process(temporal_buffer_[plane], residuals);
#endif
				temporal_buffer_[plane].dump(format("dec_full_temp_buff_P%1d", plane));
				temporal_mask.dump(format("dec_full_temp_mask_P%1d", plane));

#if defined __OPT_INPLACE__
				{
					auto viewa = upsampled_planes[plane].view_as<int16_t>();
					auto viewb = temporal_buffer_[plane].view_as<int16_t>();
					const int16_t *__restrict psrcb = viewb.data(0, 0);
					int16_t *__restrict pdst = (int16_t *)viewa.data(0, 0);
					for (unsigned y = 0; y < upsampled_planes[plane].height() * upsampled_planes[plane].width(); ++y) {
						*pdst++ += (*psrcb++);
					}
				}
				full_reco[plane] = upsampled_planes[plane];
#else
				full_reco[plane] = Add().process(upsampled_planes[plane], temporal_buffer_[plane]);
#endif
			} else {
				// Temporal disabled
#if defined __OPT_INPLACE__
				{
					auto viewa = upsampled_planes[plane].view_as<int16_t>();
					auto viewb = residuals.view_as<int16_t>();
					const int16_t *__restrict psrcb = viewb.data(0, 0);
					int16_t *__restrict pdst = (int16_t *)viewa.data(0, 0);
					for (unsigned y = 0; y < upsampled_planes[plane].height() * upsampled_planes[plane].width(); ++y) {
						*pdst++ += (*psrcb++);
					}
				}
				full_reco[plane] = upsampled_planes[plane];
#else
				full_reco[plane] = Add().process(upsampled_planes[plane], residuals);
#endif
			}

		} else if (plane < configuration_.global_configuration.num_processed_planes && apply_enhancement) {
			// No enhancement - but temporal layer can still be added
			if (configuration_.global_configuration.temporal_enabled) {
				Surface temporal_mask = get_temporal_mask(symbols[plane][LOQ_LEVEL_2][num_residual_layers()]);
				CHECK(!temporal_mask.empty());
				if (temporal_buffer_[plane].empty()) {
					temporal_buffer_[plane] = Surface::build_from<int16_t>()
					                              .generate(upsampled_planes[plane].width(), upsampled_planes[plane].height(),
					                                        [&](unsigned x, unsigned y) -> int16_t { return 0; })
					                              .finish();
				}

				// Apply temporal map (intra / pred)
				temporal_buffer_[plane] =
				    ApplyTemporalMap().process(temporal_buffer_[plane], temporal_mask, transform_block_size());
				temporal_buffer_[plane].dump(format("dec_full_temp_buff_P%1d", plane));
				temporal_mask.dump(format("dec_full_temp_mask_P%1d", plane));
#if defined __OPT_INPLACE__
				{
					auto viewa = upsampled_planes[plane].view_as<int16_t>();
					auto viewb = temporal_buffer_[plane].view_as<int16_t>();
					const int16_t *__restrict psrcb = viewb.data(0, 0);
					int16_t *__restrict pdst = (int16_t *)viewa.data(0, 0);
					for (unsigned y = 0; y < upsampled_planes[plane].height() * upsampled_planes[plane].width(); ++y) {
						*pdst++ += (*psrcb++);
					}
				}
				full_reco[plane] = upsampled_planes[plane];
#else
				full_reco[plane] = Add().process(upsampled_planes[plane], temporal_buffer_[plane]);
#endif
			} else {
				// Temporal disabled
				full_reco[plane] = upsampled_planes[plane];
			}
		} else {
			full_reco[plane] = upsampled_planes[plane];
		}
		// INFO("dither flag %4d type %4d stre %4d", configuration_.picture_configuration.dithering_control,
		// configuration_.picture_configuration.dithering_type, configuration_.picture_configuration.dithering_strength);
		if (dithering_switch && configuration_.picture_configuration.dithering_control && (plane == 0)) {
			outp_reco[plane] = dithering_.process(full_reco[plane], transform_block_size());
			if (dithering_fixed) {
				// PSNR calculation after dithering if using fixed seed
				full_reco[plane] = outp_reco[plane];
			}
		} else
			outp_reco[plane] = full_reco[plane];
	}

	Surface output[MAX_NUM_PLANES];
	for (unsigned p = 0; p < ext_base.description().num_planes(); ++p) {

		Surface o;
		if (configuration_.sequence_configuration.conformance_window) {
			// Apply conformance windowing
			const unsigned cw = dimensions_.crop_unit_width(p), ch = dimensions_.crop_unit_height(p);
			o = Conform().process(outp_reco[p], configuration_.sequence_configuration.conf_win_left_offset * cw,
			                      configuration_.sequence_configuration.conf_win_top_offset * ch,
			                      configuration_.sequence_configuration.conf_win_right_offset * cw,
			                      configuration_.sequence_configuration.conf_win_bottom_offset * ch);
		} else {
			o = outp_reco[p];
		}
		output[p] = ConvertFromInternal().process(o, configuration_.global_configuration.enhancement_depth);
	}

	const auto output_desc = ImageDescription(ext_base.description().format(), output[0].width(), output[0].height())
	                             .with_depth(configuration_.global_configuration.enhancement_depth);

	goReportStructure = goReportQueue.top();
	goReportQueue.pop();
	goPsnr.miBaseBytes += goReportStructure.miBaseSize;
	goPsnr.miEnhancementBytes += goReportStructure.miEnhancementSize;
	fprintf(stdout, "DEC. [pts. %4d] [type %4d] [base %8d] [enha %8d] ", goReportStructure.miTimeStamp,
	        goReportStructure.miPictureType, goReportStructure.miBaseSize, goReportStructure.miEnhancementSize);

	// XXX THis should get hoisted into App.
	if (report) {
		std::vector<SurfaceView<int16_t>> out;
		for (unsigned plane = 0; plane < src_image.description().num_planes(); plane++)
			out.push_back(full_reco[plane].view_as<int16_t>());
		if (!src_image.empty()) {
			for (unsigned plane = 0; plane < src_image.description().num_planes(); plane++) {
				const Surface src = ConvertToInternal().process(src_image.plane(plane), src_image.description().bit_depth());
				const auto in = src.view_as<int16_t>();
				PicturePsnr15bpp((int16_t *)in.data(), (int16_t *)out[plane].data(), plane, output_desc.width(plane),
				                 output_desc.height(plane), goPsnr);
			}
			fprintf(stdout, "[psnrY %8.4f] ", goPsnr.mfCurPsnr[0]);
			if (src_image.description().num_planes() > 1)
				fprintf(stdout, "[psnrU %8.4f] [psnrV %8.4f] \n", (goPsnr.mfCurPsnr[1]), (goPsnr.mfCurPsnr[2]));
			else
				fprintf(stdout, "\n");
		}

		LCEVC_IMGB oImageBuffer;
		oImageBuffer.np = output_desc.num_planes();
		oImageBuffer.y[0] = oImageBuffer.y[1] = oImageBuffer.y[2] = 0;
		oImageBuffer.x[0] = oImageBuffer.x[1] = oImageBuffer.x[2] = 0;
		for (unsigned plane = 0; plane < output_desc.num_planes(); plane++) {
			oImageBuffer.h[plane] = full_reco[plane].height();
			oImageBuffer.w[plane] = full_reco[plane].width();
			oImageBuffer.s[plane] = full_reco[plane].width() * 2;
			oImageBuffer.a[plane] = (void *)out[plane].data();
		}
		lcevc_md5_imgb(&oImageBuffer, gaucMd5Digest);

		// clang-format off
		fprintf(stdout, "[MD5Y %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] ",
				gaucMd5Digest[0][0],  gaucMd5Digest[0][1],  gaucMd5Digest[0][2],  gaucMd5Digest[0][3],
				gaucMd5Digest[0][4],  gaucMd5Digest[0][5],  gaucMd5Digest[0][6],  gaucMd5Digest[0][7],
				gaucMd5Digest[0][8],  gaucMd5Digest[0][9],  gaucMd5Digest[0][10], gaucMd5Digest[0][11],
				gaucMd5Digest[0][12], gaucMd5Digest[0][13], gaucMd5Digest[0][14], gaucMd5Digest[0][15]);
		if (output_desc.num_planes() > 1) {
			fprintf(stdout, "[MD5U %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] ",
					gaucMd5Digest[1][0],  gaucMd5Digest[1][1],  gaucMd5Digest[1][2],  gaucMd5Digest[1][3],
					gaucMd5Digest[1][4],  gaucMd5Digest[1][5],  gaucMd5Digest[1][6],  gaucMd5Digest[1][7],
					gaucMd5Digest[1][8],  gaucMd5Digest[1][9],  gaucMd5Digest[1][10], gaucMd5Digest[1][11],
					gaucMd5Digest[1][12], gaucMd5Digest[1][13], gaucMd5Digest[1][14], gaucMd5Digest[1][15]);
			fprintf(stdout, "[MD5V %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] \n",
					gaucMd5Digest[2][0],  gaucMd5Digest[2][1],  gaucMd5Digest[2][2],  gaucMd5Digest[2][3],
					gaucMd5Digest[2][4],  gaucMd5Digest[2][5],  gaucMd5Digest[2][6],  gaucMd5Digest[2][7],
					gaucMd5Digest[2][8],  gaucMd5Digest[2][9],  gaucMd5Digest[2][10], gaucMd5Digest[2][11],
					gaucMd5Digest[2][12], gaucMd5Digest[2][13], gaucMd5Digest[2][14], gaucMd5Digest[2][15]);
		}
		// clang-format on
	} else {
		fprintf(stdout, "\n");
	}

	fflush(stdout);

	return Image(std::string("output"), output_desc, ext_base.timestamp(), output);
}

} // namespace lctm
