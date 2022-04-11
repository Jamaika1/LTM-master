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
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//               Florian Maurer (florian.maurer@v-nova.com)
//
// Encoder.cpp
//

#include "Encoder.hpp"

#include "Convert.hpp"
#include "Deblocking.hpp"
#include "Deserializer.hpp"
#include "Diagnostics.hpp"
#include "InverseQuantize.hpp"
#include "InverseTransformDD.hpp"
#include "InverseTransformDDS.hpp"
#include "InverseTransformDDS_1D.hpp"
#include "InverseTransformDD_1D.hpp"

#include "Add.hpp"
#include "Compare.hpp"
#include "Conform.hpp"
#include "Dithering.hpp"
#include "LcevcMd5.hpp"
#include "Misc.hpp"
#include "PredictedResidual.hpp"
#include "PriorityConfiguration.hpp"
#include "PriorityMap.hpp"
#include "Quantize.hpp"
#include "ResidualMap.hpp"
#include "Serializer.hpp"
#include "Subtract.hpp"
#include "TemporalDecode.hpp"
#include "TemporalEncode.hpp"
#include "TransformDD.hpp"
#include "TransformDDS.hpp"
#include "TransformDDS_1D.hpp"
#include "TransformDD_1D.hpp"
#include "Upsampling.hpp"

#include "HuffmanDecoder.hpp"

#include "YUVWriter.hpp"

#include "ParameterDefaults.hpp"

#include <cstring>
#include <ctime>
#include <memory>

#include <cassert>

#include <queue>
#include <vector>

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern priority_queue<ReportStructure, vector<ReportStructure, allocator<ReportStructure>>, ReportStructureComp> goReportQueue;

#define DELTA_SW

namespace lctm {

Encoder::Encoder(const ImageDescription &image_description, const Parameters &parameters) {
	memset(&encoder_configuration_, 0, sizeof(encoder_configuration_));
	memset(&configuration_, 0, sizeof(configuration_));

	// Build default parameters
	auto pb = Parameters::build();
	Parameters defaults = parameter_defaults_global(parameters);

	// Set base QP param as calculated before....
	encoder_configuration_.base_qp = parameters["qp"].get<unsigned>(defaults);

	update_global_configuration(parameters, defaults, image_description);

	// Build image descriptions
	src_image_description_ = image_description;
	base_image_description_ = ImageDescription(image_description.format(), dimensions_.base_width(), dimensions_.base_height())
	                              .with_depth(configuration_.global_configuration.base_depth);
	intermediate_image_description_ = ImageDescription(
	    configuration_.global_configuration.level1_depth_flag ? src_image_description_.format() : base_image_description_.format(),
	    dimensions_.intermediate_width(), dimensions_.intermediate_height());

	enhancement_image_description_ =
	    ImageDescription(image_description.format(), dimensions_.conformant_width(), dimensions_.conformant_height());
}

void Encoder::initialise_config(const Parameters &parameters, std::vector<std::unique_ptr<Image>> &src_image) {
	// Build default parameters
	auto pb = Parameters::build();
	Parameters defaults = parameter_defaults_picture(parameters, src_image, encoder_configuration_.base_qp);

	// Update Transform Type
	if (!defaults["encoding_transform_type"].empty()) {
		if (defaults["encoding_transform_type"].get<std::string>() == "dd") {
			configuration_.global_configuration.transform_block_size = 2;
			configuration_.global_configuration.num_residual_layers = 4;

			// Update image descriptions (conformance window might change with new transform)
			dimensions_.set(configuration_, src_image_description_.width(), src_image_description_.height());
			configuration_.global_configuration.resolution_width = dimensions_.conformant_width();
			configuration_.global_configuration.resolution_height = dimensions_.conformant_height();
			base_image_description_ = base_image_description_.with_size(dimensions_.base_width(), dimensions_.base_height());
			intermediate_image_description_ =
			    intermediate_image_description_.with_size(dimensions_.intermediate_width(), dimensions_.intermediate_height());
			enhancement_image_description_ =
			    enhancement_image_description_.with_size(dimensions_.conformant_width(), dimensions_.conformant_height());

		} else if (defaults["encoding_transform_type"].get<std::string>() == "dds") {
			configuration_.global_configuration.transform_block_size = 4;
			configuration_.global_configuration.num_residual_layers = 16;
		}
	}

	update_sequence_configuration(parameters, defaults, src_image_description_);
	update_encoder_configuration(parameters, defaults);
	update_picture_configuration(parameters, defaults);

	const unsigned step_width_2 = defaults["cq_step_width_loq_2"].get<unsigned>(MAX_STEP_WIDTH);
	if (step_width_2 != (unsigned)MAX_STEP_WIDTH) {
		configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
		    clamp(step_width_2, (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
		configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] =
		    clamp(step_width_2, (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
	}
	if (parameters["temporal_step_width_modifier"].empty()) {
		configuration_.global_configuration.temporal_step_width_modifier =
		    defaults["temporal_step_width_modifier"].get<unsigned>(48);
	}
	if (parameters["encoding_upsample"].empty()) {
		const Upsample upsample = defaults["encoding_upsample"].get_enum<Upsample>(Upsample_ModifiedCubic);
		configuration_.global_configuration.upsample = upsample;
		if (upsample == Upsample_AdaptiveCubic) {
			defaults["upsampling_coefficients"].get_vector<unsigned>(
			    configuration_.global_configuration.upsampling_coefficients,
			    ARRAY_SIZE(configuration_.global_configuration.upsampling_coefficients));
		}
	}

	// Initialize quantization matrix
	std::fill_n(&quant_matrix_coeffs_[0][0][0], MAX_NUM_PLANES * MAX_NUM_LOQS * MAX_NUM_LAYERS, -1);

	// Report final configurations
	REPORT("#### #### encoding_transform_type %4d", configuration_.global_configuration.transform_block_size);
	REPORT("#### #### num_processed_planes %4d", configuration_.global_configuration.num_processed_planes);
	REPORT("#### #### dequant_offset_mode %4d", configuration_.picture_configuration.dequant_offset_mode);
	REPORT("#### #### dequant_offset  %4d", configuration_.picture_configuration.dequant_offset);
	REPORT("#### #### temporal_cq_sw_multiplier %4d", encoder_configuration_.temporal_cq_sw_multiplier);
	REPORT("#### #### temporal_step_width_modifier %4d", configuration_.global_configuration.temporal_step_width_modifier);
	// REPORT("#### #### priority_mode %s", parameters["priority_mode"].get<std::string>().c_str());
	REPORT("#### #### encoding_upsample %4d", configuration_.global_configuration.upsample);
	REPORT("#### #### upsampling_coefficients %4d %4d %4d %4d", configuration_.global_configuration.upsampling_coefficients[0],
	       configuration_.global_configuration.upsampling_coefficients[1],
	       configuration_.global_configuration.upsampling_coefficients[2],
	       configuration_.global_configuration.upsampling_coefficients[3]);
	REPORT("#### #### SW1 %4d", configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_1]);
	REPORT("#### #### SW2 %4d", configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2]);
	REPORT("#### #### quant_matrix_mode %4d", configuration_.picture_configuration.quant_matrix_mode);
	REPORT("#### #### qm_coefficient_1 %4d %4d %4d %4d", configuration_.picture_configuration.qm_coefficient_1_par[0],
	       configuration_.picture_configuration.qm_coefficient_1_par[1],
	       configuration_.picture_configuration.qm_coefficient_1_par[2],
	       configuration_.picture_configuration.qm_coefficient_1_par[3]);
	REPORT("#### #### qm_coefficient_2 %4d %4d %4d %4d", configuration_.picture_configuration.qm_coefficient_2_par[0],
	       configuration_.picture_configuration.qm_coefficient_2_par[1],
	       configuration_.picture_configuration.qm_coefficient_2_par[2],
	       configuration_.picture_configuration.qm_coefficient_2_par[3]);
}

void Encoder::update_encoder_configuration(const Parameters &p, const Parameters &d) {
	// Encoder only configuration - not signalled
	// clang-format off
	encoder_configuration_.temporal_cq_sw_multiplier = p["temporal_cq_sw_multiplier"].get<unsigned>(d);
	CHECK(encoder_configuration_.temporal_cq_sw_multiplier >= 200 && encoder_configuration_.temporal_cq_sw_multiplier <= 1000);
	encoder_configuration_.delta_sw_mult_gop08 = p["delta_sw_mult_gop08"].get<unsigned>(d);
	encoder_configuration_.delta_sw_mult_gop08 = clamp(encoder_configuration_.delta_sw_mult_gop08, (uint32_t)1000, (uint32_t)2000);
	encoder_configuration_.delta_sw_mult_gop04 = p["delta_sw_mult_gop04"].get<unsigned>(d);
	encoder_configuration_.delta_sw_mult_gop04 = clamp(encoder_configuration_.delta_sw_mult_gop04, (uint32_t)1000, (uint32_t)2000);
	encoder_configuration_.delta_sw_mult_gop02 = p["delta_sw_mult_gop02"].get<unsigned>(d);
	encoder_configuration_.delta_sw_mult_gop02 = clamp(encoder_configuration_.delta_sw_mult_gop02, (uint32_t)1000, (uint32_t)2000);
	encoder_configuration_.delta_sw_mult_gop01 = p["delta_sw_mult_gop01"].get<unsigned>(d);
	encoder_configuration_.delta_sw_mult_gop01 = clamp(encoder_configuration_.delta_sw_mult_gop01, (uint32_t)1000, (uint32_t)2000);

	encoder_configuration_.no_enhancement_temporal_layer = p["temporal_signalling_present"].get<bool>(false);
	encoder_configuration_.user_data_method = p["user_data_method"].get_enum<UserDataMethod>(UserDataMethod_Zeros);

	// PriorityConfiguration priority_map_config(p["priority_mode"].get<std::string>("mode_3_1"), p["priority_type_sl_1"].get<std::string>("type_4"), p["priority_type_sl_2"].get<std::string>("type_5"));
	PriorityConfiguration priority_map_config(p["priority_mode"].get<std::string>(d), p["priority_type_sl_1"].get<std::string>("type_4"), p["priority_type_sl_2"].get<std::string>("type_5"));
	encoder_configuration_.encoding_mode = priority_map_config.encoding_mode();
	encoder_configuration_.temporal_use_priority_map_sl_1 = priority_map_config.use_priority_map_sl_1();
	encoder_configuration_.temporal_use_priority_map_sl_2 = priority_map_config.use_priority_map_sl_2();
	encoder_configuration_.priority_type_sl_1 = priority_map_config.priority_map_type_sl_1();
	encoder_configuration_.priority_type_sl_2 = priority_map_config.priority_map_type_sl_2();
	encoder_configuration_.sad_threshold = p["sad_threshold"].get<unsigned>(d);
	encoder_configuration_.sad_coeff_threshold = p["sad_coeff_threshold"].get<unsigned>(d);
	encoder_configuration_.quant_reduced_deadzone = p["quant_reduced_deadzone"].get<unsigned>(d);
	// clang-format on
}

// Per stream configuration
//
void Encoder::update_global_configuration(const Parameters &p, const Parameters &d, const ImageDescription &image_description) {
	// Signalled
	// clang-format off
	configuration_.global_configuration.enhancement_depth = image_description.bit_depth();
	configuration_.global_configuration.base_depth = p["base_depth"].get<unsigned>(image_description.bit_depth());
	configuration_.global_configuration.colourspace = image_description.colourspace();

	configuration_.global_configuration.num_image_planes = p["num_image_planes"].get<unsigned>(3);
	configuration_.global_configuration.num_processed_planes = p["num_processed_planes"].get<unsigned>(d);
	configuration_.global_configuration.predicted_residual_enabled = p["predicted_residual"].get<bool>(true);
	configuration_.global_configuration.temporal_enabled = p["temporal_enabled"].get<bool>(true);
	configuration_.global_configuration.temporal_tile_intra_signalling_enabled = p["temporal_use_reduced_signalling"].get<bool>(true);
	configuration_.global_configuration.upsample = p["encoding_upsample"].get_enum<Upsample>(Upsample_ModifiedCubic);
	d["upsampling_coefficients"].get_vector<unsigned>(configuration_.global_configuration.upsampling_coefficients,
		ARRAY_SIZE(configuration_.global_configuration.upsampling_coefficients));
	configuration_.global_configuration.temporal_step_width_modifier = p["temporal_step_width_modifier"].get<unsigned>(48);
	configuration_.global_configuration.level_1_filtering_first_coefficient = p["level_1_filtering_first_coefficient"].get<unsigned>(0);
	configuration_.global_configuration.level_1_filtering_second_coefficient = p["level_1_filtering_second_coefficient"].get<unsigned>(0);
	configuration_.global_configuration.scaling_mode[LOQ_LEVEL_1] = p["scaling_mode_level1"].get_enum<ScalingMode>(ScalingMode_None);
	configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2] = p["scaling_mode_level2"].get_enum<ScalingMode>(ScalingMode_2D);
	configuration_.global_configuration.user_data_enabled = p["user_data_enabled"].get_enum<UserDataMode>(UserData_None);
	configuration_.global_configuration.level1_depth_flag = p["level1_depth_flag"].get<bool>(false);
	configuration_.global_configuration.tile_width = p["tile_width"].get<unsigned>(0);
	configuration_.global_configuration.tile_height = p["tile_height"].get<unsigned>(0);
	configuration_.global_configuration.compression_type_entropy_enabled_per_tile = p["compression_type_entropy_enabled_per_tile"].get<bool>(false);
	configuration_.global_configuration.compression_type_size_per_tile = p["compression_type_size_per_tile"].get_enum<CompressionType>(CompressionType_None);

	const TransformType transform_type = p["encoding_transform_type"].get_enum<TransformType>(TransformType_DDS);
	if (transform_type == TransformType_DD) {
		configuration_.global_configuration.transform_block_size = 2;
		configuration_.global_configuration.num_residual_layers = 4;
	} else if (transform_type == TransformType_DDS) {
		configuration_.global_configuration.transform_block_size = 4;
		configuration_.global_configuration.num_residual_layers = 16;
	} else {
		ERR("Unknown transform type");
	}

	if (configuration_.global_configuration.tile_width == 0 && configuration_.global_configuration.tile_height == 0)
		configuration_.global_configuration.tile_dimensions_type = TileDimensions_None;
	else if (configuration_.global_configuration.tile_width == 512 && configuration_.global_configuration.tile_height == 256)
		configuration_.global_configuration.tile_dimensions_type = TileDimensions_512x256;
	else if (configuration_.global_configuration.tile_width == 1024 && configuration_.global_configuration.tile_height == 512)
		configuration_.global_configuration.tile_dimensions_type = TileDimensions_1024x512;
	else
		configuration_.global_configuration.tile_dimensions_type = TileDimensions_Custom;

	if(configuration_.global_configuration.upsample == Upsample_AdaptiveCubic) {
		p["upsampling_coefficients"].get_vector<unsigned>(configuration_.global_configuration.upsampling_coefficients,
														  ARRAY_SIZE(configuration_.global_configuration.upsampling_coefficients));
	}

	// Work out encoded image size - conformance window will be used to produce correct decoded size
	dimensions_.set(configuration_, image_description.width(), image_description.height());

	configuration_.global_configuration.resolution_width = dimensions_.conformant_width();
	configuration_.global_configuration.resolution_height = dimensions_.conformant_height();

	configuration_.global_configuration.chroma_step_width_multiplier = p["chroma_step_width_multiplier"].get<unsigned>(64);

	configuration_.global_configuration.additional_info_present = p["additional_info_present"].get<bool>(false);
	configuration_.global_configuration.sei_message_present = p["sei_message_present"].get<bool>(false);
	configuration_.global_configuration.vui_message_present = p["vui_message_present"].get<bool>(false);
	if (configuration_.global_configuration.additional_info_present) {
		configuration_.additional_info.additional_info_type = p["additional_info_type"].get<unsigned>(2);
	}

	INFO("additional %4d - type %4d - sei %4d - vui %4d", configuration_.global_configuration.additional_info_present, configuration_.global_configuration.additional_info_type, 
	     configuration_.global_configuration.sei_message_present, configuration_.global_configuration.vui_message_present);
}

// Per sequence configuration
//
void Encoder::update_sequence_configuration(const Parameters &p, const Parameters &d, const ImageDescription &image_description) {
	// Signalled
	// clang-format off
	configuration_.sequence_configuration.profile_idc = p["profile_idc"].get_enum<Profile>(Profile_Auto);
	configuration_.sequence_configuration.level_idc = p["level_idc"].get<unsigned>(0);
	configuration_.sequence_configuration.sublevel_idc = p["sublevel_idc"].get<unsigned>(1);
	configuration_.sequence_configuration.conformance_window = p["conformance_window"].get<bool>(false);
	configuration_.sequence_configuration.extended_profile_idc = p["extended_profile_idc"].get<unsigned>(0);
	configuration_.sequence_configuration.extended_level_idc = p["extended_level_idc"].get<unsigned>(0);
	configuration_.sequence_configuration.conf_win_left_offset = p["conf_win_left_offset"].get<unsigned>(0);
	configuration_.sequence_configuration.conf_win_right_offset = p["conf_win_right_offset"].get<unsigned>(0);
	configuration_.sequence_configuration.conf_win_top_offset = p["conf_win_top_offset"].get<unsigned>(0);
	configuration_.sequence_configuration.conf_win_bottom_offset = p["conf_win_bottom_offset"].get<unsigned>(0);
	// clang-format on

	// Check for correct profile and level
	if (configuration_.sequence_configuration.profile_idc == Profile_Auto) {
		if (image_description.colourspace() == Colourspace_YUV422 || image_description.colourspace() == Colourspace_YUV444) {
			configuration_.sequence_configuration.profile_idc = Profile_Main444;
			INFO("Using Main 4:4:4 Profile with chroma_format_idc %u.", (unsigned)image_description.colourspace());
		} else {
			configuration_.sequence_configuration.profile_idc = Profile_Main;
			INFO("Using Main Profile with chroma_format_idc %u.", (unsigned)image_description.colourspace());
		}
	} else if (configuration_.sequence_configuration.profile_idc == Profile_Main &&
	           (image_description.colourspace() == Colourspace_YUV422 || image_description.colourspace() == Colourspace_YUV444))
		ERR("Colourspace not supported with selected profile (see Annex A.3)");
	const unsigned fps = p["fps"].get<unsigned>(50);
	const unsigned long output_sample_rate =
	    configuration_.global_configuration.resolution_width * configuration_.global_configuration.resolution_height * fps;
	if (configuration_.sequence_configuration.level_idc == 0) {
		// Choose minimum level automatically
		if (output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_1)
			configuration_.sequence_configuration.level_idc = 1;
		else if (output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_2)
			configuration_.sequence_configuration.level_idc = 2;
		else if (output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_3)
			configuration_.sequence_configuration.level_idc = 3;
		else if (output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_4)
			configuration_.sequence_configuration.level_idc = 4;
		else
			ERR("Cannot detect level automatically. Output sample rate not supported by any level. (see Annex A.4)");
		INFO("Using Level %u.%u", configuration_.sequence_configuration.level_idc,
		     configuration_.sequence_configuration.sublevel_idc);
	} else {
		switch (configuration_.sequence_configuration.level_idc) {
		case 1:
			CHECK(output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_1);
			break;
		case 2:
			CHECK(output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_2);
			break;
		case 3:
			CHECK(output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_3);
			break;
		case 4:
			CHECK(output_sample_rate <= MAX_OUTPUT_RATE_LEVEL_4);
			break;
		default:
			ERR("Not a supported Level (see Annex A.4)");
		}
	}

	if (configuration_.global_configuration.resolution_width != image_description.width() ||
	    configuration_.global_configuration.resolution_height != image_description.height()) {
		INFO("Resolution has been adjusted from %ux%u to %ux%u to conform.", image_description.width(), image_description.height(),
		     configuration_.global_configuration.resolution_width, configuration_.global_configuration.resolution_height);
		configuration_.sequence_configuration.conformance_window = true;
		configuration_.sequence_configuration.conf_win_right_offset =
		    (configuration_.global_configuration.resolution_width - image_description.width()) / 2;
		configuration_.sequence_configuration.conf_win_bottom_offset =
		    (configuration_.global_configuration.resolution_height - image_description.height()) / 2;
	}
}

// Per picture configuration
//
void Encoder::update_picture_configuration(const Parameters &p, const Parameters &d) {
	// Signalled
	// clang-format off
	configuration_.picture_configuration.enhancement_enabled = p["enhancement"].get<bool>(true); // NB: may be overridden in encoder
	configuration_.picture_configuration.temporal_refresh = p["temporal_refresh"].get<bool>(false); // NB: may be overridden in encoder
	configuration_.picture_configuration.temporal_signalling_present = p["temporal_signalling_present"].get<bool>(true); // NB: may be overridden in encoder

	//configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_1] = p["cq_step_width_loq_1"].get<unsigned>(MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_1] = p["cq_step_width_loq_1"].get<unsigned>(d);
	configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_1] = clamp(configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_1], (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] = p["cq_step_width_loq_0"].get<unsigned>(MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] = clamp(configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2], (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_1] = p["cq_step_width_loq_1"].get<unsigned>(d);
	configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_1] = clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_1], (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] = p["cq_step_width_loq_0"].get<unsigned>(MAX_STEP_WIDTH);
	configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] = clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2], (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);

	configuration_.picture_configuration.picture_type = p["picture_type"].get_enum<PictureType>(PictureType_Frame);
	configuration_.picture_configuration.field_type = FieldType_Top;
	configuration_.picture_configuration.coding_type = CodingType_IDR;

	configuration_.picture_configuration.level_1_filtering_enabled = p["level_1_filtering_enabled"].get<bool>(false);

	// configuration_.picture_configuration.dithering_control = p["dithering_control"].get<bool>(false);
	configuration_.picture_configuration.dithering_control = p["dithering_control"].get<bool>(d);
	// configuration_.picture_configuration.dithering_type = p["dithering_type"].get_enum<DitheringType>(Dithering_None);
	configuration_.picture_configuration.dithering_type = p["dithering_type"].get_enum<DitheringType>(d);
	// configuration_.picture_configuration.dithering_strength = p["dithering_strength"].get<unsigned>(0);
	configuration_.picture_configuration.dithering_strength = p["dithering_strength"].get<unsigned>(d);
	configuration_.picture_configuration.dequant_offset_mode = p["dequant_offset_mode"].get_enum<DequantOffset>(d);
	configuration_.picture_configuration.dequant_offset = p["dequant_offset"].get<int>(d);

	configuration_.picture_configuration.quant_matrix_mode = p["quant_matrix_mode"].get_enum<QuantMatrix>(d);
	d["qm_coefficient_2"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_2_par,
	                                            configuration_.global_configuration.num_residual_layers);
	d["qm_coefficient_1"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_1_par,
	                                            configuration_.global_configuration.num_residual_layers);

	// clang-format on

	if (configuration_.picture_configuration.dequant_offset == 0 &&
	    configuration_.picture_configuration.dequant_offset_mode == DequantOffset_Default) {
		configuration_.picture_configuration.dequant_offset_signalled = false;
	} else {
		configuration_.picture_configuration.dequant_offset_signalled = true;
	}

	// first, set the QM to default values
	const uint8_t *d2 = nullptr;
	const uint8_t *d1 = nullptr;
	const bool horizontal_only = (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2] == ScalingMode_1D ? true : false);
	if (configuration_.global_configuration.num_residual_layers == 4) {
		// 2x2 case
		d1 = default_qm_coefficient_2x2[2]; // LOQ1
		// 4x4 case
		if (horizontal_only)
			d2 = default_qm_coefficient_2x2[0]; // LOQ2 1D
		else
			d2 = default_qm_coefficient_2x2[1]; // LOQ2 2D
	} else {
		// 4x4 case
		d1 = default_qm_coefficient_4x4[2]; // LOQ1
		if (horizontal_only)
			d2 = default_qm_coefficient_4x4[0]; // LOQ2 1D
		else
			d2 = default_qm_coefficient_4x4[1]; // LOQ2 2D
	}
	for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
		configuration_.picture_configuration.qm_coefficient_2[i] = d2[i];
		configuration_.picture_configuration.qm_coefficient_1[i] = d1[i];
	}

	{
		// set the QM to default or specific custom values
		if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_BothDefault) {
			// parameters read from input (command line or config file) initialised to default
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2_par[i] = d2[i];
				configuration_.picture_configuration.qm_coefficient_1_par[i] = d1[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_SameAndCustom) {
			p["qm_coefficient_2"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_2_par,
			                                           configuration_.global_configuration.num_residual_layers);
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_1_par[i] =
				    configuration_.picture_configuration.qm_coefficient_2_par[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_Level2CustomLevel1Default) {
			p["qm_coefficient_2"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_2_par,
			                                           configuration_.global_configuration.num_residual_layers);
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_Level2DefaultLevel1Custom) {
			p["qm_coefficient_1"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_1_par,
			                                           configuration_.global_configuration.num_residual_layers);
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
			p["qm_coefficient_2"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_2_par,
			                                           configuration_.global_configuration.num_residual_layers);
			p["qm_coefficient_1"].get_vector<unsigned>(configuration_.picture_configuration.qm_coefficient_1_par,
			                                           configuration_.global_configuration.num_residual_layers);
		}
	}

	// third, save the QM in memory for the next round
	for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
		configuration_.picture_configuration.qm_coefficient_2_mem[i] = configuration_.picture_configuration.qm_coefficient_2[i];
		configuration_.picture_configuration.qm_coefficient_1_mem[i] = configuration_.picture_configuration.qm_coefficient_1[i];
	}
}

// Per picture configuration (in loop)
//
void Encoder::update_picture_configuration_in_loop() {
	// first, set the QM to default values
	const uint8_t *d2 = nullptr;
	const uint8_t *d1 = nullptr;
	const bool horizontal_only = (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2] == ScalingMode_1D ? true : false);
	if (configuration_.global_configuration.num_residual_layers == 4) {
		// 2x2 case
		d1 = default_qm_coefficient_2x2[2]; // LOQ1
		// 4x4 case
		if (horizontal_only)
			d2 = default_qm_coefficient_2x2[0]; // LOQ2 1D
		else
			d2 = default_qm_coefficient_2x2[1]; // LOQ2 2D
	} else {
		// 4x4 case
		d1 = default_qm_coefficient_4x4[2]; // LOQ1
		if (horizontal_only)
			d2 = default_qm_coefficient_4x4[0]; // LOQ2 1D
		else
			d2 = default_qm_coefficient_4x4[1]; // LOQ2 2D
	}
	for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
		configuration_.picture_configuration.qm_coefficient_2[i] = d2[i];
		configuration_.picture_configuration.qm_coefficient_1[i] = d1[i];
	}

	if (configuration_.picture_configuration.coding_type != CodingType_IDR ||
	    configuration_.picture_configuration.enhancement_enabled) {
		// if it is not IDR or enhancement_enabled is true, use the quant_matrix_mode value for the QM choice
		// second, set the QM to specific custom values
		if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_BothPrevious) {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] =
				    configuration_.picture_configuration.qm_coefficient_2_mem[i];
				configuration_.picture_configuration.qm_coefficient_1[i] =
				    configuration_.picture_configuration.qm_coefficient_1_mem[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_BothDefault) {
			const uint8_t *d2 = nullptr;
			const uint8_t *d1 = nullptr;
			const bool horizontal_only =
			    (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2] == ScalingMode_1D ? true : false);
			if (configuration_.global_configuration.num_residual_layers == 4) {
				// 2x2 case
				d1 = default_qm_coefficient_2x2[2]; // LOQ1
				// 4x4 case
				if (horizontal_only)
					d2 = default_qm_coefficient_2x2[0]; // LOQ2 1D
				else
					d2 = default_qm_coefficient_2x2[1]; // LOQ2 2D
			} else {
				// 4x4 case
				d1 = default_qm_coefficient_4x4[2]; // LOQ1
				if (horizontal_only)
					d2 = default_qm_coefficient_4x4[0]; // LOQ2 1D
				else
					d2 = default_qm_coefficient_4x4[1]; // LOQ2 2D
			}
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] = d2[i];
				configuration_.picture_configuration.qm_coefficient_1[i] = d1[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_SameAndCustom) {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] =
				    configuration_.picture_configuration.qm_coefficient_2_par[i];
				configuration_.picture_configuration.qm_coefficient_1[i] = configuration_.picture_configuration.qm_coefficient_2[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_Level2CustomLevel1Default) {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] =
				    configuration_.picture_configuration.qm_coefficient_2_par[i];
				configuration_.picture_configuration.qm_coefficient_1[i] =
				    configuration_.picture_configuration.qm_coefficient_1_mem[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_Level2DefaultLevel1Custom) {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] =
				    configuration_.picture_configuration.qm_coefficient_2_mem[i];
				configuration_.picture_configuration.qm_coefficient_1[i] =
				    configuration_.picture_configuration.qm_coefficient_1_par[i];
			}
		} else if (configuration_.picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				configuration_.picture_configuration.qm_coefficient_2[i] =
				    configuration_.picture_configuration.qm_coefficient_2_par[i];
				configuration_.picture_configuration.qm_coefficient_1[i] =
				    configuration_.picture_configuration.qm_coefficient_1_par[i];
			}
		}
	}
	// else -- if it is IDR and enhancement_enabled is false, reset to QM default // nothing to do: keep the QM default

	// third, save the QM in memory for the next round
	for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
		configuration_.picture_configuration.qm_coefficient_2_mem[i] = configuration_.picture_configuration.qm_coefficient_2[i];
		configuration_.picture_configuration.qm_coefficient_1_mem[i] = configuration_.picture_configuration.qm_coefficient_1[i];
	}
	configuration_.picture_configuration.temporal_signalling_present = false;
}

// Does this layer have user_data embedded?
//
bool Encoder::is_user_data_layer(unsigned loq, unsigned layer) const {
	if (loq == LOQ_LEVEL_1 && configuration_.global_configuration.user_data_enabled != UserData_None) {
		if (configuration_.global_configuration.transform_block_size == 4)
			return layer == 5;
		else
			return layer == 1;
	}
	// No embedded user data
	return false;
}

void Encoder::encode_residuals(unsigned plane, unsigned loq, const Surface &residuals, Surface symbols[MAX_NUM_LAYERS],
                               Temporal_SWM swm_type, EncodingMode mode, const Surface &temporal_mask, const Surface &priority_type,
                               PriorityMBType priority_mb_type, const bool final, const Surface &pixel_sad) const {
	//// Transform
	//
	Surface coefficients[MAX_NUM_LAYERS];
	const bool horizontal_only = (configuration_.global_configuration.scaling_mode[loq] == ScalingMode_1D ? true : false);

	if (!horizontal_only) {
		// Horizontal and Vertical
		//
		if (configuration_.global_configuration.transform_block_size == 4) {
			TransformDDS().process(residuals, EncodingMode::ENCODE_ALL, coefficients);
		} else if (configuration_.global_configuration.transform_block_size == 2) {
			TransformDD().process(residuals, EncodingMode::ENCODE_ALL, coefficients);
		}
	} else {
		// Horizontal only
		//
		if (configuration_.global_configuration.transform_block_size == 4) {
			TransformDDS_1D().process(residuals, EncodingMode::ENCODE_ALL, coefficients);
		} else if (configuration_.global_configuration.transform_block_size == 2) {
			TransformDD_1D().process(residuals, EncodingMode::ENCODE_ALL, coefficients);
		}
	}

	if (final && loq == LOQ_LEVEL_1)
		Surface::dump_layers(coefficients, format("enc_base_coeff_transform_output_P%1d", plane), transform_block_size());
	else if (final && loq == LOQ_LEVEL_2)
		Surface::dump_layers(coefficients, format("enc_full_coeff_transform_output_P%1d", plane), transform_block_size());

	Surface coefficients_sad[MAX_NUM_LAYERS];
	if (!pixel_sad.empty() && encoder_configuration_.sad_threshold != 0 && encoder_configuration_.sad_coeff_threshold != 0) {
		StaticResiduals().process(coefficients, coefficients_sad, pixel_sad, num_residual_layers(),
		                          configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2],
		                          encoder_configuration_.sad_threshold, encoder_configuration_.sad_coeff_threshold);
		if (final && loq == LOQ_LEVEL_2)
			Surface::dump_layers(coefficients_sad, format("enc_full_coeff_SAD_output_P%1d", plane), transform_block_size());
	} else {
		for (unsigned layer = 0; layer < num_residual_layers(); layer++) {
			const auto coeff = coefficients[layer].view_as<int16_t>();
			coefficients_sad[layer] = Surface::build_from<int16_t>()
			                              .generate(coefficients[layer].width(), coefficients[layer].height(),
			                                        [&](unsigned x, unsigned y) -> int16_t { return coeff.read(x, y); })
			                              .finish();
		}
	}

#if defined PRIORITY_COEF
	Surface priority_coefficients[MAX_NUM_LAYERS];
	if (priority_type.width() != 0 && priority_type.height() != 0) {
		PriorityMap().ApplyPriorityCoefficients(coefficients_sad[0].width(), coefficients_sad[0].height(),
		                                        configuration_.global_configuration.transform_block_size, loq, coefficients_sad,
		                                        priority_coefficients, priority_type, pixel_sad,
		                                        configuration_.picture_configuration.step_width_loq[loq], priority_mb_type, mode);
		if (final && loq == LOQ_LEVEL_1)
			Surface::dump_layers(priority_coefficients, format("enc_base_coeff_PM_output_P%1d", plane), transform_block_size());
		else if (final && loq == LOQ_LEVEL_2)
			Surface::dump_layers(priority_coefficients, format("enc_full_coeff_PM_output_P%1d", plane), transform_block_size());
	} else {
		for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
			const auto coef = coefficients_sad[i].view_as<int16_t>();
			priority_coefficients[i] = Surface::build_from<int16_t>()
			                               .generate(coefficients_sad[i].width(), coefficients_sad[i].height(),
			                                         [&](unsigned x, unsigned y) -> int16_t { return coef.read(x, y); })
			                               .finish();
		}
	}
#endif // defined PRIORITY_COEF

	//// Step widths
	//
	int32_t dirq_step_width[MAX_NUM_LAYERS][2] = {};
	int32_t dirq_deadzone[MAX_NUM_LAYERS][2] = {};
	unsigned passes = 1;

	{
		// XXX This looks to be replicated twice here and once in the decoder
		unsigned step_width[2];
		int16_t calculated_step_width = configuration_.picture_configuration.step_width_loq[loq];
		if (loq == LOQ_LEVEL_2 && plane > 0) {
			calculated_step_width =
				clamp((int16_t)((calculated_step_width * configuration_.global_configuration.chroma_step_width_multiplier) >> 6),
					  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
		}
		if (loq == LOQ_LEVEL_2) {
			switch (swm_type) {
			case Temporal_SWM::SWM_Disabled:
				step_width[0] = calculated_step_width;
				break;
			case Temporal_SWM::SWM_Active:
				step_width[0] =
					clamp((int16_t)(calculated_step_width *
									(1 - clamp(((float)configuration_.global_configuration.temporal_step_width_modifier / (float)255),
											   (float)0, (float)0.5))),
						  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
				break;
			case Temporal_SWM::SWM_Dependent:
				step_width[0] =
					clamp((int16_t)(calculated_step_width *
									(1 - clamp(((float)configuration_.global_configuration.temporal_step_width_modifier / (float)255),
											   (float)0, (float)0.5))),
						  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
				step_width[1] = calculated_step_width;
				passes = 2;
				break;
			default:
				CHECK(0);
				break;
			}
		} else
			step_width[0] = calculated_step_width;

		for (unsigned pass = 0; pass < passes; pass++) {
			for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
				dirq_step_width[layer][pass] = find_dirq_step_width(step_width[pass], quant_matrix_coeffs_[plane][loq][layer]);
				dirq_deadzone[layer][pass] = find_layer_deadzone(step_width[pass], dirq_step_width[layer][pass]);
			}
		}
	}

	//// Quantize each layer
	//
	for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {

		Surface syms;
		if (passes == 1) {
#if defined PRIORITY_COEF
			syms = Quantize().process(priority_coefficients[layer], dirq_step_width[layer][0], dirq_deadzone[layer][0], pixel_sad,
			                          transform_block_size(), encoder_configuration_.quant_reduced_deadzone);
#else  // defined PRIORITY_COEF
			syms = Quantize().process(coefficients_sad[layer], dirq_step_width[layer][0], dirq_deadzone[layer][0], pixel_sad,
			                          transform_block_size(), encoder_configuration_.quant_reduced_deadzone);
#endif // defined PRIORITY_COEF
		} else {
#if defined PRIORITY_COEF
			syms = Quantize_SWM().process(priority_coefficients[layer], transform_block_size(), dirq_step_width[layer],
			                              dirq_deadzone[layer], temporal_mask, pixel_sad,
			                              encoder_configuration_.quant_reduced_deadzone);
#else  // defined PRIORITY_COEF
			syms = Quantize_SWM().process(coefficients_sad[layer], transform_block_size(), dirq_step_width[layer],
			                              dirq_deadzone[layer], temporal_mask, pixel_sad,
			                              encoder_configuration_.quant_reduced_deadzone);
#endif // defined PRIORITY_COEF
		}
		if (is_user_data_layer(loq, layer)) {
			//// Insert User Data
#if USER_DATA_EXTRACTION
			FILE *file = fopen("userdata_enc.bin", "ab");
			symbols[layer] = UserDataInsert().process(syms, encoder_configuration_.user_data_method,
			                                          configuration_.global_configuration.user_data_enabled, file);
			fclose(file);
#else
			symbols[layer] = UserDataInsert().process(syms, encoder_configuration_.user_data_method,
			                                          configuration_.global_configuration.user_data_enabled);
#endif
		} else
			symbols[layer] = syms;
	}
	if (final && loq == LOQ_LEVEL_1)
		Surface::dump_layers(symbols, format("enc_base_coeff_quant_output_P%1d", plane), transform_block_size());
	else if (final && loq == LOQ_LEVEL_2)
		Surface::dump_layers(symbols, format("enc_full_coeff_quant_output_P%1d", plane), transform_block_size());

	if (final && !pixel_sad.empty() && pixel_sad.get_dump_surfaces() && loq == LOQ_LEVEL_2) {
		// Creation of debug maps

		Surface priority_debug[MAX_NUM_LAYERS];
		if (priority_type.width() != 0 && priority_type.height() != 0) {
			PriorityMap().ApplyPriorityCoefficients(
			    coefficients[0].width(), coefficients[0].height(), configuration_.global_configuration.transform_block_size, loq,
			    coefficients, priority_debug, priority_type, pixel_sad, configuration_.picture_configuration.step_width_loq[loq],
			    priority_mb_type, mode);
		} else {
			for (unsigned i = 0; i < configuration_.global_configuration.num_residual_layers; i++) {
				const auto coef = coefficients[i].view_as<int16_t>();
				priority_debug[i] = Surface::build_from<int16_t>()
				                        .generate(coefficients[i].width(), coefficients[i].height(),
				                                  [&](unsigned x, unsigned y) -> int16_t { return coef.read(x, y); })
				                        .finish();
			}
		}

		Surface syms_anchor[MAX_NUM_LAYERS], syms_no_sad[MAX_NUM_LAYERS];

		for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
			Surface syms_a, syms_b;
			if (passes == 1) {
				syms_a = Quantize().process(priority_coefficients[layer], dirq_step_width[layer][0], dirq_deadzone[layer][0],
				                            pixel_sad, transform_block_size(), 5u);
				syms_b = Quantize().process(priority_debug[layer], dirq_step_width[layer][0], dirq_deadzone[layer][0], pixel_sad,
				                            transform_block_size(), 5u);
			} else {
				syms_a = Quantize_SWM().process(priority_coefficients[layer], transform_block_size(), dirq_step_width[layer],
				                                dirq_deadzone[layer], temporal_mask, pixel_sad, 5u);
				syms_b = Quantize_SWM().process(priority_debug[layer], transform_block_size(), dirq_step_width[layer],
				                                dirq_deadzone[layer], temporal_mask, pixel_sad, 5u);
			}
			syms_anchor[layer] = syms_a;
			syms_no_sad[layer] = syms_b;
		}

		const Surface static_residuals_debug =
		    Surface::build_from<int16_t>()
		        .generate(residuals.width(), residuals.height(),
		                  [&](unsigned x, unsigned y) -> int16_t {
			                  const unsigned layer =
			                      (x % transform_block_size() + transform_block_size() * (y % transform_block_size()));
			                  const auto syms_anchor_view = syms_anchor[layer].view_as<int16_t>();
			                  const auto syms_view = symbols[layer].view_as<int16_t>();
			                  const auto syms_no_sad_view = syms_no_sad[layer].view_as<int16_t>();

			                  if (syms_no_sad_view.read(x / transform_block_size(), y / transform_block_size()) !=
			                      syms_anchor_view.read(x / transform_block_size(), y / transform_block_size()))
				                  return 20000;
			                  else if (syms_anchor_view.read(x / transform_block_size(), y / transform_block_size()) !=
			                           syms_view.read(x / transform_block_size(), y / transform_block_size()))
				                  return -20000;
			                  else
				                  return 0;
		                  })
		        .finish();
		static_residuals_debug.dump(format("enc_full_coeff_SAD_changes_P%1d", plane));
	}
}

Surface Encoder::decode_residuals(unsigned plane, unsigned loq, const Surface symbols[MAX_NUM_LAYERS], Temporal_SWM temp_type,
                                  const Surface &temporal_mask) const {

	Surface coefficients[MAX_NUM_LAYERS] = {};
	int32_t dirq_step_width[MAX_NUM_LAYERS][2] = {};
	int32_t invq_step_width[MAX_NUM_LAYERS][2] = {};
	int32_t invq_offset[MAX_NUM_LAYERS][2] = {};
	int32_t invq_deadzone[MAX_NUM_LAYERS][2] = {};
	const bool horizontal_only = (configuration_.global_configuration.scaling_mode[loq] == ScalingMode_1D ? true : false);

	// Apply temporal step width modifier, if required
	unsigned passes = 1;
	{
		int16_t step_width[2];
		int16_t calculated_step_width = configuration_.picture_configuration.step_width_loq[loq];
		if (loq == LOQ_LEVEL_2 && plane > 0) {
			calculated_step_width =
				clamp((int16_t)((calculated_step_width * configuration_.global_configuration.chroma_step_width_multiplier) >> 6),
					  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
		}
		if (loq == LOQ_LEVEL_2) {
			switch (temp_type) {
			case Temporal_SWM::SWM_Disabled:
				step_width[0] = calculated_step_width;
				break;
			case Temporal_SWM::SWM_Active:
				step_width[0] =
					clamp((int16_t)(calculated_step_width *
									(1 - clamp(((float)configuration_.global_configuration.temporal_step_width_modifier / (float)255),
											   (float)0, (float)0.5))),
						  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
				break;
			case Temporal_SWM::SWM_Dependent:
				step_width[0] =
					clamp((int16_t)(calculated_step_width *
									(1 - clamp(((float)configuration_.global_configuration.temporal_step_width_modifier / (float)255),
											   (float)0, (float)0.5))),
						  (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
				step_width[1] = calculated_step_width;
				passes = 2;
				break;
			default:
				break;
			}
		} else {
			step_width[0] = calculated_step_width;
		}

		for (unsigned pass = 0; pass < passes; pass++) {
			for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
				dirq_step_width[layer][pass] = find_dirq_step_width(step_width[pass], quant_matrix_coeffs_[plane][loq][layer]);
				invq_offset[layer][pass] =
					find_invq_offset(configuration_.picture_configuration, step_width[pass], dirq_step_width[layer][pass]);

				invq_step_width[layer][pass] =
					find_invq_step_width(configuration_.picture_configuration, dirq_step_width[layer][pass], invq_offset[layer][pass]);

				invq_deadzone[layer][pass] = find_layer_deadzone(step_width[pass], invq_step_width[layer][pass]);
			}
		}
	}

	// Dequantize
	for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
		Surface syms;
		if (is_user_data_layer(loq, layer))
			syms = UserDataClear().process(symbols[layer], configuration_.global_configuration.user_data_enabled);
		else
			syms = symbols[layer];

		if (passes == 1) {
			coefficients[layer] = InverseQuantize().process(
			    syms, invq_step_width[layer][0],
			    find_invq_applied_offset(configuration_.picture_configuration, invq_offset[layer][0], invq_deadzone[layer][0]));
		} else {
			int32_t invq_applied_offset[2] = {
			    find_invq_applied_offset(configuration_.picture_configuration, invq_offset[layer][0], invq_deadzone[layer][0]),
			    find_invq_applied_offset(configuration_.picture_configuration, invq_offset[layer][1], invq_deadzone[layer][1])};

			coefficients[layer] = InverseQuantize_SWM().process(symbols[layer], transform_block_size(), invq_step_width[layer],
			                                                    invq_applied_offset, temporal_mask);
		}
	}

	Surface residuals;

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

	return residuals;
}

// Decide what syntax blocks should be serialized
//
static unsigned syntax_blocks_mask(BaseFrameType frame_type, bool encoded_data_present, bool tiled, bool additional_info_present) {
	SyntaxBlocks data{};

	if (encoded_data_present)
		data = tiled ? SyntaxBlock_EncodedData : SyntaxBlock_EncodedData_Tiled;

	switch (frame_type) {
	case BaseFrame_IDR:
	case BaseFrame_Intra:
		if (additional_info_present)
			return SyntaxBlock_Global | SyntaxBlock_Picture | SyntaxBlock_Sequence | SyntaxBlock_Additional_Info | data;
		else
			return SyntaxBlock_Global | SyntaxBlock_Picture | SyntaxBlock_Sequence | data;

	case BaseFrame_Inter:
	case BaseFrame_Pred:
	case BaseFrame_Bidi:
		return SyntaxBlock_Picture | data;
	}

	return 0;
}

// Size of transform block
//
unsigned Encoder::transform_block_size() const { return configuration_.global_configuration.transform_block_size; }

unsigned Encoder::num_residual_layers() const { return configuration_.global_configuration.num_residual_layers; }

bool Encoder::level1_depth_flag() const { return configuration_.global_configuration.level1_depth_flag; }

unsigned Encoder::base_qp() const { return encoder_configuration_.base_qp; }

// Encode one enhancement frame, given source image, downsampled source, and the decoded base image
//
Packet Encoder::encode(std::vector<std::unique_ptr<Image>> &src_image, const Image &intermediate_src_image,
                       const Image &base_prediction_image, BaseFrameType frame_type, bool is_idr, int gop_frame_num,
                       const std::string &output_file) {

	if (dithering_.getInitialised() == false) {
		INFO("Dither init %4d bitdepth %d", configuration_.picture_configuration.dithering_strength,
		     configuration_.global_configuration.enhancement_depth);
		dithering_.make_buffer(configuration_.picture_configuration.dithering_strength,
		                       configuration_.global_configuration.enhancement_depth,
		                       configuration_.picture_configuration.dithering_type == Dithering_UniformFixed ? true : false);
		dithering_.setInitialised(true);
	}

	Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];

	unsigned frame = (unsigned)(intermediate_src_image.timestamp());

	// Figure out whether data should be sent
	if (configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_1] >= MAX_STEP_WIDTH &&
	    configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] >= MAX_STEP_WIDTH) {
		if (!configuration_.global_configuration.temporal_enabled)
			configuration_.picture_configuration.enhancement_enabled = false;
		else {
			if ((frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR) &&
			    encoder_configuration_.temporal_cq_sw_multiplier != 1000)
				configuration_.picture_configuration.enhancement_enabled = true;
			else
				configuration_.picture_configuration.enhancement_enabled = false;
		}
	} else
		configuration_.picture_configuration.enhancement_enabled = true;

	if (frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR) {
		configuration_.picture_configuration.coding_type = CodingType_IDR;
		update_picture_configuration_in_loop();
	} else {
		configuration_.picture_configuration.coding_type = CodingType_NonIDR;
		update_picture_configuration_in_loop();
	}

	// For each plane
	//
	Surface base_reco[MAX_NUM_PLANES];
	Surface full_reco[MAX_NUM_PLANES];
	Surface outp_reco[MAX_NUM_PLANES];

	// Needed to randomly generate user data (part of CE)
	if (configuration_.global_configuration.user_data_enabled) {
		if (encoder_configuration_.user_data_method == UserDataMethod_Random)
			Random().srand((unsigned)std::time(nullptr));
		else if (encoder_configuration_.user_data_method == UserDataMethod_FixedRandom)
			Random().srand(45721);
	}

	configuration_.picture_configuration.temporal_refresh = false;

	for (unsigned plane = 0; plane < src_image[0]->description().num_planes(); ++plane) {
		const bool enhancement_enabled = configuration_.picture_configuration.enhancement_enabled &&
		                                 plane < configuration_.global_configuration.num_processed_planes;

		//// Convert between base and enhancement bit depth
		Surface base_image;
		unsigned base_bit_depth = configuration_.global_configuration.base_depth;
		if (configuration_.global_configuration.enhancement_depth > configuration_.global_configuration.base_depth &&
		    configuration_.global_configuration.level1_depth_flag) {
			base_image =
			    ConvertBitShift().process(base_prediction_image.plane(plane), configuration_.global_configuration.base_depth,
			                              configuration_.global_configuration.enhancement_depth);
			base_bit_depth = configuration_.global_configuration.enhancement_depth;
		} else {
			base_image = base_prediction_image.plane(plane);
		}

		const Surface src = ConvertToInternal().process(src_image[0]->plane(plane), src_image[0]->description().bit_depth());
		const Surface intermediate_src =
		    ConvertToInternal().process(intermediate_src_image.plane(plane), intermediate_src_image.description().bit_depth());
		const Surface base_decoded = ConvertToInternal().process(base_image, base_bit_depth);
		Surface temporal_mask;

		// Work out quantization matrix for each LoQ
		for (unsigned loq = 0l; loq < MAX_NUM_LOQS; ++loq) {
			const bool horizontal_only = (configuration_.global_configuration.scaling_mode[loq] == ScalingMode_1D ? true : false);
			for (unsigned layer = 0; layer < num_residual_layers(); ++layer) {
				quant_matrix_coeffs_[plane][loq][layer] =
				    find_quant_matrix_coeff(configuration_.picture_configuration, num_residual_layers(), horizontal_only, loq,
				                            layer, is_idr, quant_matrix_coeffs_[plane][loq][layer]);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		//// Encoded Base
		//////////////////////////////////////////////////////////////////////////////////////

		//// Upsample from decoded base picture to preliminary intermediate picture
		//
		Surface base_prediction;
		switch (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_1]) {
		case ScalingMode_1D:
			base_prediction = Upsampling_1D().process(base_decoded, configuration_.global_configuration.upsample,
			                                          configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				base_prediction = PredictedResidualAdjust_1D().process(base_decoded, base_prediction,
				                                                       PredictedResidualSum_1D().process(base_prediction));
			}
			base_decoded.dump(format("enc_base_deco_P%1d", plane));
			break;
		case ScalingMode_2D:
			base_prediction = Upsampling().process(base_decoded, configuration_.global_configuration.upsample,
			                                       configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				base_prediction = PredictedResidualAdjust().process(base_decoded, base_prediction,
				                                                    PredictedResidualSum().process(base_prediction));
			}
			base_decoded.dump(format("enc_base_deco_P%1d", plane));
			break;
		case ScalingMode_None:
			base_prediction = base_decoded;
			break;
		default:
			CHECK(0);
		}

		base_prediction.dump(format("enc_base_pred_P%1d", plane));

		//////////////////////////////////////////////////////////////////////////////////////
		//// Enhancement sub-layer 1
		//////////////////////////////////////////////////////////////////////////////////////

		if (enhancement_enabled) {
			// Enhancement
			Surface base_residuals = Subtract().process(intermediate_src, base_prediction);

			base_residuals.dump(format("enc_base_resi_inpu_P%1d", plane));

			// Apply residual map
			if (encoder_configuration_.temporal_use_priority_map_sl_1 &&
			    (encoder_configuration_.encoding_mode != EncodingMode::ENCODE_NONE) && (plane == 0)) {
				// std::cout << "==> ENCODER::Going to use priority map" << std::endl;
#if defined PRIORITY_TEST
				const unsigned priority_tile_x = 8u;
				const unsigned priority_tile_y = 8u;
				const auto priority_map_loq1 = PriorityMap().process(intermediate_src_image.plane(0), priority_tile_x,
				                                                     priority_tile_y, KillingFunction::THRESHOLD_CUTOFF);

				Surface selected_base_residuals = ApplyPreprocessedMap().process(base_residuals, priority_map_loq1);

				encode_residuals(plane, LOQ_LEVEL_1, selected_base_residuals, symbols[plane][LOQ_LEVEL_1],
				                 Temporal_SWM::SWM_Disabled, encoder_configuration_.encoding_mode, Surface(), Surface(),
				                 encoder_configuration_.priority_type_sl_1);

				constexpr bool record_priority_map = false;
				if (record_priority_map) {
					// Dump priority map to file
					const auto priority_map_loq1_vis = PriorityMapVis().process(priority_map_loq1);

					const auto surface_dump_flag = Surface::get_dump_surfaces();
					Surface::set_dump_surfaces(true);
					priority_map_loq1_vis.dump(format("recorded_priority_map"));
					Surface::set_dump_surfaces(surface_dump_flag);
				}
#endif // defined PRIORITY_TEST
#if defined PRIORITY_RESI_SL1 || defined PRIORITY_COEF
				Surface priority_mean = PriorityMap().ComputeMeanValue(intermediate_src);
				Surface priority_type = PriorityMap().ComputeContrastTexture(intermediate_src, priority_mean);
				priority_type.dump(format("priority_map_layer1"));
#endif // defined PRIORITY_RESI || defined PRIORITY_COEF

#if defined PRIORITY_RESI_SL1 // NEED REVISITATION FOR SL-1 TO INCLUDE ADDITIONAL PM TYPE AND MODE
				Surface priority_residuals = PriorityMap().ApplyPriorityResiduals(
				    base_residuals, priority_type, configuration_.picture_configuration.step_width_loq[1]);
				encode_residuals(plane, LOQ_LEVEL_1, priority_residuals, symbols[plane][LOQ_LEVEL_1], Temporal_SWM::SWM_Disabled,
				                 encoder_configuration_.encoding_mode, Surface(), Surface(),
				                 encoder_configuration_.priority_type_sl_1, true);
#endif // defined PRIORITY_RESI
#if defined PRIORITY_COEF
				encode_residuals(plane, LOQ_LEVEL_1, base_residuals, symbols[plane][LOQ_LEVEL_1], Temporal_SWM::SWM_Disabled,
				                 encoder_configuration_.encoding_mode, Surface(), priority_type,
				                 encoder_configuration_.priority_type_sl_1, true);
#endif        // defined PRIORITY_COEF
			} // if plane == 0
			else {
				encode_residuals(plane, LOQ_LEVEL_1, base_residuals, symbols[plane][LOQ_LEVEL_1], Temporal_SWM::SWM_Disabled,
				                 EncodingMode::ENCODE_ALL, Surface(), Surface(), encoder_configuration_.priority_type_sl_1, true);
			}

			// Reconstruct base
			//
			Surface base_residuals_recon =
			    decode_residuals(plane, LOQ_LEVEL_1, symbols[plane][LOQ_LEVEL_1], Temporal_SWM::SWM_Disabled, Surface());

			base_residuals_recon.dump(format("enc_base_resi_reco_P%1d", plane));

			// Deblocking
			if (configuration_.picture_configuration.level_1_filtering_enabled &&
			    configuration_.global_configuration.transform_block_size == 4) {
				base_residuals_recon = Deblocking().process(
				    base_residuals_recon, configuration_.global_configuration.level_1_filtering_first_coefficient,
				    configuration_.global_configuration.level_1_filtering_second_coefficient);
			}

			base_reco[plane] = Add().process(base_prediction, base_residuals_recon);

		} else {
			// No enhancement, Just pass prediction through
			base_reco[plane] = base_prediction;
		}

		//// Upsample from combined intermediate picture to preliminary output picture
		//
		Surface enhanced_prediction;
		switch (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2]) {
		case ScalingMode_1D:
			enhanced_prediction = Upsampling_1D().process(base_reco[plane], configuration_.global_configuration.upsample,
			                                              configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				enhanced_prediction = PredictedResidualAdjust_1D().process(base_reco[plane], enhanced_prediction,
				                                                           PredictedResidualSum_1D().process(enhanced_prediction));
			}
			break;
		case ScalingMode_2D:
			enhanced_prediction = Upsampling().process(base_reco[plane], configuration_.global_configuration.upsample,
			                                           configuration_.global_configuration.upsampling_coefficients);
			if (configuration_.global_configuration.predicted_residual_enabled) {
				enhanced_prediction = PredictedResidualAdjust().process(base_reco[plane], enhanced_prediction,
				                                                        PredictedResidualSum().process(enhanced_prediction));
			}
			break;
		case ScalingMode_None:
			enhanced_prediction = base_reco[plane];
			break;
		default:
			CHECK(0);
		}

		enhanced_prediction.dump(format("enc_full_pred_P%1d", plane));

		//////////////////////////////////////////////////////////////////////////////////////
		//// Enhancement sub-layer 2
		//////////////////////////////////////////////////////////////////////////////////////

		if (enhancement_enabled) {
			/* const */ Surface enhanced_residuals = Subtract().process(src, enhanced_prediction);
			Surface temporal_mask_per_transform;
			Surface temporal_tile_map;

			const Surface src_next =
			    (src_image.size() > 1 && plane == 0 &&
			     ((encoder_configuration_.sad_threshold != 0 && encoder_configuration_.sad_coeff_threshold != 0) ||
			      (encoder_configuration_.temporal_use_priority_map_sl_2 &&
			       encoder_configuration_.encoding_mode != EncodingMode::ENCODE_NONE) ||
			      encoder_configuration_.quant_reduced_deadzone != 5))
			        ? ConvertToInternal().process(src_image[1]->plane(plane), src_image[1]->description().bit_depth())
			        : Surface();
			const Surface pixel_sad =
			    !src_next.empty()
			        ? TemporalCost_SAD().process(src, src_next, configuration_.global_configuration.transform_block_size)
			        : Surface();
			if (!pixel_sad.empty() && pixel_sad.get_dump_surfaces()) {
				pixel_sad.dump(format("enc_pixel_sad_P%1d", plane));

				// Dump pixel SAD map highlighting static and non-static areas
				const auto pixel_sad_view = pixel_sad.view_as<int16_t>();
				const Surface static_areas_map = Surface::build_from<int16_t>()
				                                     .generate(pixel_sad_view.width(), pixel_sad_view.height(),
				                                               [&](unsigned x, unsigned y) -> int16_t {
					                                               const int16_t src = pixel_sad_view.read(x, y);
					                                               if (src <= (transform_block_size() == 4 ? 200 : 100))
						                                               return -20000;
					                                               else if (src <= (transform_block_size() == 4 ? 12000 : 6000))
						                                               return 0;
					                                               else
						                                               return 20000;
				                                               })
				                                     .finish();
				static_areas_map.dump(format("enc_static_areas_map_P%1d", plane));
			}

			if (!configuration_.picture_configuration.temporal_signalling_present &&
			    (!configuration_.global_configuration.temporal_enabled || previous_residuals_[plane].empty() ||
			     frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR)) {
				//// No Temporal
				//
				configuration_.picture_configuration.temporal_refresh = true;
				configuration_.picture_configuration.temporal_signalling_present = false;

				if (frame_type == BaseFrame_IDR || frame_type == BaseFrame_Intra)
					last_idr_frame_num = gop_frame_num;
				if ((frame_type == BaseFrame_IDR || frame_type == BaseFrame_Intra) &&
				    configuration_.global_configuration.temporal_enabled) {
					configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
					    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
					              encoder_configuration_.temporal_cq_sw_multiplier / 1000,
					          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
				} else {
					configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
					    (configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2]);
				}

#if defined DELTA_SW
				if (!(frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR)) {
					if ((gop_frame_num - last_idr_frame_num) % 8 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop08 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else if ((gop_frame_num - last_idr_frame_num) % 4 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop04 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else if ((gop_frame_num - last_idr_frame_num) % 2 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop02 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop01 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					}
				}
#endif // defined DELTA_SW

				temporal_mask = Surface::build_from<uint8_t>()
				                    .fill(TemporalType::TEMPORAL_INTR, enhanced_prediction.width() / transform_block_size(),
				                          enhanced_prediction.height() / transform_block_size())
				                    .finish();

				temporal_mask.dump(format("enc_full_temp_mask_bloc_P%1d", plane));
				temporal_mask.dump(format("enc_full_temp_mask_P%1d", plane));

				enhanced_residuals.dump(format("enc_full_resi_inpu_P%1d", plane));

				// Apply residual map

				if (encoder_configuration_.temporal_use_priority_map_sl_2 &&
				    (encoder_configuration_.encoding_mode != EncodingMode::ENCODE_NONE) && (plane == 0)) {
#if defined PRIORITY_TEST
					encode_residuals(plane, LOQ_LEVEL_2, enhanced_residuals, symbols[plane][LOQ_LEVEL_2],
					                 Temporal_SWM::SWM_Disabled, EncodingMode::ENCODE_ALL, Surface(), Surface(),
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
#endif // defined PRIORITY_TEST
#if defined PRIORITY_RESI_SL2 || defined PRIORITY_COEF
					// source vs upsampled
					Surface priority_mean = PriorityMap().ComputeMeanValue(src);
					Surface priority_type = PriorityMap().ComputeContrastTexture(src, priority_mean);
					// Surface priority_mean = PriorityMap().ComputeMeanValue(enhanced_prediction);
					// Surface priority_type = PriorityMap().ComputeContrastTexture(enhanced_prediction, priority_mean);
					priority_type.dump(format("priority_map_layer2"));
#endif // defined PRIORITY_RESI_SL2 || defined PRIORITY_COEF

#if defined PRIORITY_RESI_SL2 // NEED REVISITATION FOR SL-2 TO INCLUDE ADDITIONAL PM TYPE AND MODE
					Surface priority_residuals = PriorityMap().ApplyPriorityResiduals(
					    enhanced_residuals, priority_type, configuration_.picture_configuration.step_width_loq[1]);
					encode_residuals(plane, LOQ_LEVEL_2, priority_residuals, symbols[plane][LOQ_LEVEL_2],
					                 Temporal_SWM::SWM_Disabled, EncodingMode::ENCODE_ALL, Surface(), Surface(),
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
#endif // defined PRIORITY_RESI
#if defined PRIORITY_COEF
					encode_residuals(plane, LOQ_LEVEL_2, enhanced_residuals, symbols[plane][LOQ_LEVEL_2],
					                 Temporal_SWM::SWM_Disabled, EncodingMode::ENCODE_ALL, Surface(), priority_type,
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
#endif            // defined PRIORITY_COEF
				} // if plane == 0
				else {
					encode_residuals(plane, LOQ_LEVEL_2, enhanced_residuals, symbols[plane][LOQ_LEVEL_2],
					                 Temporal_SWM::SWM_Disabled, EncodingMode::ENCODE_ALL, Surface(), Surface(),
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
				}

				const Surface residual_reconstruction =
				    decode_residuals(plane, LOQ_LEVEL_2, symbols[plane][LOQ_LEVEL_2], Temporal_SWM::SWM_Disabled, Surface());
				residual_reconstruction.dump(format("enc_full_resi_reco_P%1d", plane));

				if (configuration_.global_configuration.temporal_enabled) {
					// Save residuals in temporal buffer only if temporal prediction is enabled
					previous_residuals_[plane] = residual_reconstruction;
					previous_residuals_[plane].dump(format("enc_full_temp_buff_P%1d", plane));
				}
				full_reco[plane] = Add().process(enhanced_prediction, residual_reconstruction);
			} else {
				//// Temporal
				//
				configuration_.picture_configuration.temporal_refresh = false;
				configuration_.picture_configuration.temporal_signalling_present = true;

				if (frame_type == BaseFrame_IDR || frame_type == BaseFrame_Intra)
					last_idr_frame_num = gop_frame_num;
				if ((frame_type == BaseFrame_IDR || frame_type == BaseFrame_Intra) &&
				    configuration_.global_configuration.temporal_enabled) {
					configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
					    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
					              encoder_configuration_.temporal_cq_sw_multiplier / 1000,
					          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
				} else {
					configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
					    (configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2]);
				}

#if defined DELTA_SW
				if (!(frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR)) {
					if ((gop_frame_num - last_idr_frame_num) % 8 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop08 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else if ((gop_frame_num - last_idr_frame_num) % 4 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop04 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else if ((gop_frame_num - last_idr_frame_num) % 2 == 0) {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop02 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					} else {
						configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] =
						    clamp(configuration_.picture_configuration.step_width_loq_orig[LOQ_LEVEL_2] *
						              encoder_configuration_.delta_sw_mult_gop01 / 1000,
						          (unsigned)MIN_STEP_WIDTH, (unsigned)MAX_STEP_WIDTH);
					}
				}
#endif // defined DELTA_SW

				Surface residuals_input;
				Surface residuals_recon;

				temporal_mask = Surface::build_from<uint8_t>()
				                    .fill(TEMPORAL_INTR, enhanced_prediction.width() / transform_block_size(),
				                          enhanced_prediction.height() / transform_block_size())
				                    .finish();
				const bool horizontal_only =
				    (configuration_.global_configuration.scaling_mode[LOQ_LEVEL_2] == ScalingMode_1D ? true : false);

				// do we need to apply chromasw also for temporal decision?
				int16_t calculated_step_width = configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2];
				if (plane > 0) {
					calculated_step_width =
					    clamp((int16_t)((calculated_step_width * configuration_.global_configuration.chroma_step_width_multiplier) >> 6),
					          (int16_t)MIN_STEP_WIDTH, (int16_t)MAX_STEP_WIDTH);
				}

				int32_t dirq_step_width =
				    find_dirq_step_width(/* configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] */ calculated_step_width,
				                         quant_matrix_coeffs_[plane][LOQ_LEVEL_2][0]);

				int32_t invq_offset = find_invq_offset(
				    configuration_.picture_configuration,
				    /* configuration_.picture_configuration.step_width_loq[LOQ_LEVEL_2] */ calculated_step_width, dirq_step_width);

				int32_t lambda = find_invq_step_width(configuration_.picture_configuration, dirq_step_width, invq_offset);

				if (!previous_residuals_[plane].empty()) {
					// Encode, reconstruct & cost intra tiles
					//
					Surface intra_symbols[MAX_NUM_LAYERS];
					encode_residuals(plane, LOQ_LEVEL_2, enhanced_residuals, intra_symbols, Temporal_SWM::SWM_Active,
					                 EncodingMode::ENCODE_ALL, Surface(), Surface(), encoder_configuration_.priority_type_sl_2);
					const Surface intra_residuals_recon =
					    decode_residuals(plane, LOQ_LEVEL_2, intra_symbols, Temporal_SWM::SWM_Active, Surface());
					const Surface intra_recon = Add().process(enhanced_prediction, intra_residuals_recon);
					Surface intra_cost;
					if (transform_block_size() == 2)
						intra_cost =
						    TemporalCost_2x2().process(src, intra_recon, intra_symbols, transform_block_size(), lambda, true);
					else
						intra_cost =
						    TemporalCost_4x4().process(src, intra_recon, intra_symbols, transform_block_size(), lambda, true);

					intra_cost.dump(format("enc_intr_cost_P%1d", plane));

					// Encode, reconstruct & cost inter tiles
					//
					Surface inter_symbols[MAX_NUM_LAYERS];
					Surface inter_residuals = Subtract().process(enhanced_residuals, previous_residuals_[plane]);
					encode_residuals(plane, LOQ_LEVEL_2, inter_residuals, inter_symbols, Temporal_SWM::SWM_Active,
					                 EncodingMode::ENCODE_ALL, Surface(), Surface(), encoder_configuration_.priority_type_sl_2);
					const Surface inter_residuals_recon =
					    decode_residuals(plane, LOQ_LEVEL_2, inter_symbols, Temporal_SWM::SWM_Active, Surface());
					const Surface inter_residuals_accumulated = Add().process(previous_residuals_[plane], inter_residuals_recon);
					const Surface inter_recon = Add().process(enhanced_prediction, inter_residuals_accumulated);
					Surface inter_cost;
					if (transform_block_size() == 2)
						inter_cost =
						    TemporalCost_2x2().process(src, inter_recon, inter_symbols, transform_block_size(), lambda, false);
					else
						inter_cost =
						    TemporalCost_4x4().process(src, inter_recon, inter_symbols, transform_block_size(), lambda, false);

					inter_cost.dump(format("enc_pred_cost_P%1d", plane));

					// Figure out intra/pred mask per transform

					temporal_mask_per_transform = CompareLE().process(intra_cost, inter_cost);

					if (configuration_.global_configuration.temporal_tile_intra_signalling_enabled) {
						// Calculate tile mask
						temporal_tile_map = TemporalTileMap().process(intra_symbols, inter_symbols, transform_block_size());

						// Filter the decision mask applying reduced signaling.
						temporal_mask = TemporalTileIntraSignal().process(temporal_tile_map, temporal_mask_per_transform,
						                                                  transform_block_size());
					} else {
						temporal_mask = temporal_mask_per_transform;
					}

					previous_residuals_[plane] =
					    ApplyTemporalMap().process(previous_residuals_[plane], temporal_mask, transform_block_size());
				} else {
					// Force intra prediction (no temporal buffer available)
					previous_residuals_[plane] =
					    Surface::build_from<int16_t>().fill(0, enhanced_residuals.width(), enhanced_residuals.height()).finish();
					temporal_mask_per_transform =
					    Surface::build_from<uint8_t>()
					        .fill(TemporalType::TEMPORAL_INTR, enhanced_prediction.width() / transform_block_size(),
					              enhanced_prediction.height() / transform_block_size())
					        .finish();
					if (configuration_.global_configuration.temporal_tile_intra_signalling_enabled) {
						const unsigned tile_size = 32;
						temporal_tile_map =
						    Surface::build_from<uint8_t>()
						        .fill(TemporalType::TEMPORAL_INTR, (enhanced_prediction.width() + tile_size - 1) / tile_size,
						              (enhanced_prediction.height() + tile_size - 1) / tile_size)
						        .finish();
						temporal_mask = TemporalTileIntraSignal().process(temporal_tile_map, temporal_mask_per_transform,
						                                                  transform_block_size());
					} else {
						temporal_mask = temporal_mask_per_transform;
					}
				}
				temporal_mask_per_transform.dump(format("enc_full_temp_mask_bloc_P%1d", plane));
				if (configuration_.global_configuration.temporal_tile_intra_signalling_enabled)
					temporal_tile_map.dump(format("enc_full_temp_mask_tile_P%1d", plane));
				temporal_mask.dump(format("enc_full_temp_mask_P%1d", plane));

				residuals_input = Subtract().process(enhanced_residuals, previous_residuals_[plane]);

				residuals_input.dump(format("enc_full_resi_inpu_P%1d", plane));

				//// Temporal
				//
				// Apply residual map
				if (encoder_configuration_.temporal_use_priority_map_sl_2 &&
				    (encoder_configuration_.encoding_mode != EncodingMode::ENCODE_NONE) && (plane == 0)) {
#if defined PRIORITY_TEST
					encode_residuals(plane, LOQ_LEVEL_2, residuals_input, symbols[plane][LOQ_LEVEL_2], Temporal_SWM::SWM_Dependent,
					                 EncodingMode::ENCODE_ALL, temporal_mask, Surface(), encoder_configuration_.priority_type_sl_2,
					                 true, pixel_sad);
#endif // defined PRIORITY_TEST
#if defined PRIORITY_RESI_SL2 || defined PRIORITY_COEF
					// source vs upsampled
					Surface priority_mean = PriorityMap().ComputeMeanValue(src);
					Surface priority_type = PriorityMap().ComputeContrastTexture(src, priority_mean);
					// Surface priority_mean = PriorityMap().ComputeMeanValue(enhanced_prediction);
					// Surface priority_type = PriorityMap().ComputeContrastTexture(enhanced_prediction, priority_mean);
					priority_type.dump(format("priority_map_layer2"));
#endif // defined PRIORITY_RESI_SL2 || defined PRIORITY_COEF

#if defined PRIORITY_RESI_SL2 // NEED REVISITATION FOR SL-2 TO INCLUDE ADDITIONAL PM TYPE AND MODE
					Surface priority_residuals = PriorityMap().ApplyPriorityResiduals(
					    residuals_input, priority_type, configuration_.picture_configuration.step_width_loq[0]);
					encode_residuals(plane, LOQ_LEVEL_2, priority_residuals, symbols[plane][LOQ_LEVEL_2],
					                 Temporal_SWM::SWM_Dependent, EncodingMode::ENCODE_ALL, temporal_mask, Surface(),
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
#endif // defined PRIORITY_RESI
#if defined PRIORITY_COEF
					encode_residuals(plane, LOQ_LEVEL_2, residuals_input, symbols[plane][LOQ_LEVEL_2], Temporal_SWM::SWM_Dependent,
					                 EncodingMode::ENCODE_ALL, temporal_mask, priority_type,
					                 encoder_configuration_.priority_type_sl_2, true, pixel_sad);
#endif            // defined PRIORITY_COEF
				} // if plane == 0
				else {
					encode_residuals(plane, LOQ_LEVEL_2, residuals_input, symbols[plane][LOQ_LEVEL_2], Temporal_SWM::SWM_Dependent,
					                 EncodingMode::ENCODE_ALL, temporal_mask, Surface(), encoder_configuration_.priority_type_sl_2,
					                 true, pixel_sad);
				}

				residuals_recon =
				    decode_residuals(plane, LOQ_LEVEL_2, symbols[plane][LOQ_LEVEL_2], Temporal_SWM::SWM_Dependent, temporal_mask);
				residuals_recon.dump(format("enc_full_resi_reco_P%1d", plane));

				previous_residuals_[plane] = Add().process(previous_residuals_[plane], residuals_recon);
				previous_residuals_[plane].dump(format("enc_full_temp_buff_P%1d", plane));

				full_reco[plane] = Add().process(enhanced_prediction, previous_residuals_[plane]);

				// Optional temporal plane - goes in to 'extra' layer following residual layers
				if (!configuration_.picture_configuration.temporal_refresh)
					symbols[plane][LOQ_LEVEL_2][num_residual_layers()] = temporal_mask;
			}
		} else if (plane < configuration_.global_configuration.num_processed_planes) {
			configuration_.picture_configuration.temporal_signalling_present = encoder_configuration_.no_enhancement_temporal_layer;
			if (!configuration_.global_configuration.temporal_enabled || previous_residuals_[plane].empty() ||
			    frame_type == BaseFrame_Intra || frame_type == BaseFrame_IDR) {
				// No Temporal & no enhancement, just pass prediction through
				configuration_.picture_configuration.temporal_refresh = true;
				configuration_.picture_configuration.temporal_signalling_present = false;
				previous_residuals_[plane] = Surface();
				full_reco[plane] = enhanced_prediction;
			} else if (configuration_.picture_configuration.temporal_signalling_present) {
				// No Enhancement but temporal can still be added
				configuration_.picture_configuration.temporal_refresh = false;
				Surface intra_cost = TemporalCost_SAD().process(src, enhanced_prediction, transform_block_size());
				const Surface inter_recon = Add().process(enhanced_prediction, previous_residuals_[plane]);
				Surface inter_cost = TemporalCost_SAD().process(src, inter_recon, transform_block_size());

				// Figure out intra/pred mask per transform
				Surface temporal_mask_per_transform = CompareLE().process(intra_cost, inter_cost);

				if (configuration_.global_configuration.temporal_tile_intra_signalling_enabled) {
					unsigned transforms_per_tile = 32 / transform_block_size();
					Surface temporal_tile_map = Surface::build_from<uint8_t>()
					                                .generate(temporal_mask_per_transform.width() / transforms_per_tile,
					                                          temporal_mask_per_transform.height() / transforms_per_tile,
					                                          [&](unsigned x, unsigned y) -> uint8_t { return TEMPORAL_PRED; })
					                                .finish();
					temporal_tile_map.dump(format("enc_full_temp_mask_tile_P%1d", plane));
					// Insert the reduced signalling part
					temporal_mask =
					    TemporalTileIntraSignal().process(temporal_tile_map, temporal_mask_per_transform, transform_block_size());
				} else {
					temporal_mask = temporal_mask_per_transform;
				}

				temporal_mask_per_transform.dump(format("enc_full_temp_mask_bloc_P%1d", plane));
				temporal_mask.dump(format("enc_full_temp_mask_P%1d", plane));

				previous_residuals_[plane] =
				    ApplyTemporalMap().process(previous_residuals_[plane], temporal_mask, transform_block_size());
				full_reco[plane] = Add().process(enhanced_prediction, previous_residuals_[plane]);
				// Temporal plane - goes in to 'extra' layer
				symbols[plane][LOQ_LEVEL_2][num_residual_layers()] = temporal_mask;
			} else {
				configuration_.picture_configuration.temporal_refresh = false;
				full_reco[plane] = Add().process(enhanced_prediction, previous_residuals_[plane]);
			}
		} else {
			full_reco[plane] = enhanced_prediction;
		}
		// INFO("dither flag %4d type %4d stre %4d", configuration_.picture_configuration.dithering_control,
		// configuration_.picture_configuration.dithering_type, configuration_.picture_configuration.dithering_strength);
		if ((configuration_.picture_configuration.dithering_control) && (plane == 0)) {
			outp_reco[plane] = dithering_.process(full_reco[plane], transform_block_size());
			if (configuration_.picture_configuration.dithering_type == Dithering_UniformFixed) {
				// PSNR calculation after dithering if using fixed seed
				full_reco[plane] = outp_reco[plane];
			}
		} else
			outp_reco[plane] = full_reco[plane];
	}

	if (Surface::get_dump_surfaces())
		base_reco[0].dump_yuv("enc_base_reco", base_reco, base_image_description_, true);

	if (output_file != "") {
		Surface output[MAX_NUM_PLANES];
		if (configuration_.sequence_configuration.conformance_window) {
			// Apply conformance windowing
			for (unsigned p = 0; p < src_image[0]->description().num_planes(); ++p) {
				const unsigned cw = dimensions_.crop_unit_width(p), ch = dimensions_.crop_unit_height(p);
				output[p] = Conform().process(outp_reco[p], configuration_.sequence_configuration.conf_win_left_offset * cw,
				                              configuration_.sequence_configuration.conf_win_top_offset * ch,
				                              configuration_.sequence_configuration.conf_win_right_offset * cw,
				                              configuration_.sequence_configuration.conf_win_bottom_offset * ch);
			}
		} else {
			for (unsigned p = 0; p < src_image[0]->description().num_planes(); ++p)
				output[p] = outp_reco[p];
		}

		output[0].dump_yuv(output_file.c_str(), output, src_image_description_);
	}

	std::vector<SurfaceView<int16_t>> out;
	for (unsigned plane = 0; plane < src_image[0]->description().num_planes(); plane++) {
		const Surface src = ConvertToInternal().process(src_image[0]->plane(plane), src_image[0]->description().bit_depth());
		const auto in = src.view_as<int16_t>();
		out.push_back(full_reco[plane].view_as<int16_t>());
		PicturePsnr15bpp((int16_t *)in.data(), (int16_t *)out[plane].data(), plane, src_image[0]->description().width(plane),
		                 src_image[0]->description().height(plane), goPsnr);
	}

	LCEVC_IMGB oImageBuffer;
	oImageBuffer.np = src_image[0]->description().num_planes();
	oImageBuffer.y[0] = oImageBuffer.y[1] = oImageBuffer.y[2] = 0;
	oImageBuffer.x[0] = oImageBuffer.x[1] = oImageBuffer.x[2] = 0;
	for (unsigned plane = 0; plane < src_image[0]->description().num_planes(); plane++) {
		oImageBuffer.h[plane] = src_image[0]->description().height(plane);
		oImageBuffer.w[plane] = src_image[0]->description().width(plane);
		oImageBuffer.s[plane] = src_image[0]->description().width(plane) * 2;
		oImageBuffer.a[plane] = (void *)out[plane].data();
	}

	lcevc_md5_imgb(&oImageBuffer, gaucMd5Digest);

	//// Serialize enhancenemt data
	//
	const bool encoded_data_present = configuration_.picture_configuration.enhancement_enabled ||
	                                  configuration_.picture_configuration.temporal_signalling_present;

	return Serializer().emit(configuration_,
	                         syntax_blocks_mask(frame_type, encoded_data_present,
	                                            configuration_.global_configuration.tile_dimensions_type == TileDimensions_None,
	                                            configuration_.global_configuration.additional_info_present),
	                         symbols);
}

} // namespace lctm
