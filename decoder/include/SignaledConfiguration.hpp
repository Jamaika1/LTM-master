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
//
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//
// SignaledConfiguration.hpp
//
#pragma once

#include "Config.hpp"
#include "Image.hpp"
#include "Packet.hpp"
#include "Types.hpp"

namespace lctm {

enum { MAX_NUM_PLANES = 3 };
enum { MAX_NUM_LOQS = 2 };
enum { MAX_NUM_LAYERS = 17 };
enum { MIN_STEP_WIDTH = 1 };
enum { MAX_STEP_WIDTH = 32767 };
enum { MIN_BASE_QP = 0 };
enum { MAX_BASE_QP = 50 };

enum : unsigned long {
	MAX_OUTPUT_RATE_LEVEL_1 = 29410000,
	MAX_OUTPUT_RATE_LEVEL_2 = 124560000,
	MAX_OUTPUT_RATE_LEVEL_3 = 527650000,
	MAX_OUTPUT_RATE_LEVEL_4 = 2235160000,
};

enum {
	LOQ_LEVEL_1 = 0, // Enhancement sub-layer 1
	LOQ_LEVEL_2 = 1, // Enhancement sub-layer 2
};

// The constant per sequence decoder configuration
//
struct SequenceConfiguration {
	Profile profile_idc;
	unsigned level_idc;
	unsigned sublevel_idc;
	bool conformance_window;
	unsigned extended_profile_idc;
	unsigned extended_level_idc;
	unsigned conf_win_left_offset;
	unsigned conf_win_right_offset;
	unsigned conf_win_top_offset;
	unsigned conf_win_bottom_offset;
};

// The constant per stream decoder configuration
//
struct GlobalConfiguration {
	unsigned base_depth;
	unsigned enhancement_depth;
	Colourspace colourspace;
	ImageFormat image_format;
	unsigned num_image_planes;
	unsigned num_processed_planes;
	unsigned num_residual_layers;
	unsigned transform_block_size;
	bool predicted_residual_enabled;
	unsigned resolution_height;
	unsigned resolution_width;
	unsigned temporal_enabled;
	bool temporal_tile_intra_signalling_enabled;
	unsigned temporal_step_width_modifier;
	Upsample upsample;
	unsigned level_1_filtering_first_coefficient;
	unsigned level_1_filtering_second_coefficient;
	ScalingMode scaling_mode[MAX_NUM_LOQS];
	TileDimensions tile_dimensions_type;
	UserDataMode user_data_enabled;
	bool level1_depth_flag;
	unsigned chroma_step_width_multiplier;
	unsigned tile_width;
	unsigned tile_height;
	bool compression_type_entropy_enabled_per_tile;
	CompressionType compression_type_size_per_tile;
	unsigned upsampling_coefficients[4];

	bool additional_info_present;
	unsigned additional_info_type;
	bool sei_message_present;
	bool vui_message_present;
};

// The per-picture decoder configuration
//
struct PictureConfiguration {
	bool enhancement_enabled;
	bool temporal_refresh;
	bool temporal_signalling_present;
	PictureType picture_type;
	FieldType field_type;
	CodingType coding_type;
	unsigned step_width_loq[MAX_NUM_LOQS];
	unsigned step_width_loq_orig[MAX_NUM_LOQS];
	bool dithering_control;
	DitheringType dithering_type;
	unsigned dithering_strength;
	bool dequant_offset_signalled;
	DequantOffset dequant_offset_mode;
	unsigned dequant_offset;
	bool level_1_filtering_enabled;
	QuantMatrix quant_matrix_mode;
	unsigned qm_coefficient_2[MAX_NUM_LAYERS];
	unsigned qm_coefficient_1[MAX_NUM_LAYERS];
	unsigned qm_coefficient_2_mem[MAX_NUM_LAYERS];
	unsigned qm_coefficient_1_mem[MAX_NUM_LAYERS];
	unsigned qm_coefficient_2_par[MAX_NUM_LAYERS];
	unsigned qm_coefficient_1_par[MAX_NUM_LAYERS];
};

// The additional info configurationn
//
struct AdditionalInfo {
	unsigned additional_info_type;
	unsigned payload_type;
};

struct SeiMasteringDisplayColourVolume {
	unsigned display_primaries_x[MAX_NUM_PLANES];
	unsigned display_primaries_y[MAX_NUM_PLANES];
	unsigned white_point_x;
	unsigned white_point_y;
	unsigned max_display_mastering_luminance;
	unsigned min_display_mastering_luminance;
};

struct SeiContentLightLevelInfo {
	unsigned max_content_light_level;
	unsigned max_pic_average_light_level;
};

struct SeiUserDataRegisteredItutT35 {
	unsigned itu_t_t35_country_code;
	unsigned itu_t_t35_country_code_extension_byte;
};

struct SeiUserDataUnrgistered {
	uint8_t uuid_iso_iec_11578[16];
};

struct VuiMessage {
	unsigned aspect_ratio_info_present_flag;
	unsigned aspect_ratio_idc;
	unsigned sar_width;
	unsigned sar_height;
	unsigned overscan_info_present_flag;
	unsigned overscan_appropriate_flag;
	unsigned video_signal_type_present_flag;
	unsigned video_format;
	unsigned video_full_range_flag;
	unsigned colour_description_present_flag;
	unsigned colour_primaries;
	unsigned transfer_characteristics;
	unsigned matrix_coefficients;
	unsigned chroma_loc_info_present_flag;
	unsigned chroma_sample_loc_type_top_field;
	unsigned chroma_sample_loc_type_bottom_field;
	unsigned timing_info_present_flag;
	unsigned num_units_in_tick;
	unsigned time_scale;
	unsigned fixed_pic_rate_flag;
	unsigned bitstream_restriction_flag;
	unsigned motion_vectors_over_pic_boundaries_flag;
	unsigned max_bytes_per_pic_denom;
	unsigned max_bits_per_mb_denom;
	unsigned log2_max_mv_length_horizontal;
	unsigned log2_max_mv_length_vertical;
	unsigned num_reorder_pics;
	unsigned max_dec_pic_buffering;
};

// The per-surface decoder configuration
//
struct SurfaceConfiguration {
	// Size of this surface
	unsigned width;
	unsigned height;
};

// Configuration that gets sent to decoder
//
struct SignaledConfiguration {
	SequenceConfiguration sequence_configuration;
	GlobalConfiguration global_configuration;
	PictureConfiguration picture_configuration;
	AdditionalInfo additional_info;
	SurfaceConfiguration surface_configuration[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];
};

} // namespace lctm
