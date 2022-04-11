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
// Deserializer.cpp
//

#include "Deserializer.hpp"

#include "BitstreamUnpacker.hpp"
#include "Config.hpp"
#include "Diagnostics.hpp"
#include "Dimensions.hpp"
#include "EntropyDecoder.hpp"

#include <climits>

namespace lctm {

namespace {

// Read a multibyte encoded uint64_t
//
uint64_t read_multibyte(BitstreamUnpacker &b, const std::string &l) {
	BitstreamUnpacker::ScopedContextLabel label(b, l);

	uint64_t result = 0;
	bool more = false;
	do {
		more = b.u(1, "mb-more") != 0;
		uint64_t bits = b.u(7, "mb-bits");

		result = (result << 7) | bits;
	} while (more);

	return result;
}

// Coded resolution
//
const unsigned resolution_table[51][2] = {
    {0, 0},       {360, 200},   {400, 240},   {480, 320},   {640, 360},   {640, 480},   {768, 480},   {800, 600},   {852, 480},
    {854, 480},   {856, 480},   {960, 540},   {960, 640},   {1024, 576},  {1024, 600},  {1024, 768},  {1152, 864},  {1280, 720},
    {1280, 800},  {1280, 1024}, {1360, 768},  {1366, 768},  {1440, 1050}, {1440, 900},  {1600, 1200}, {1680, 1050}, {1920, 1080},
    {1920, 1200}, {2048, 1080}, {2048, 1152}, {2048, 1536}, {2160, 1440}, {2560, 1440}, {2560, 1600}, {2560, 2048}, {3200, 1800},
    {3200, 2048}, {3200, 2400}, {3440, 1440}, {3840, 1600}, {3840, 2160}, {3840, 3072}, {4096, 2160}, {4096, 3072}, {5120, 2880},
    {5120, 3200}, {5120, 4096}, {6400, 4096}, {6400, 4800}, {7680, 4320}, {7680, 4800}};

static_assert((sizeof(resolution_table) / sizeof(resolution_table[0])) == 51, "Resolution table is broken");

} // namespace

Deserializer::Deserializer(const Packet &packet, SignaledConfiguration &dst_configuration,
                           Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS])
    : Component("Deserializer"), view_(packet), b_(view_), dst_configuration_(dst_configuration), symbols_(symbols) {}

// Top level of enhancement layer parsing
//
bool Deserializer::has_more() const { return !b_.empty(); };

//
//
unsigned Deserializer::parse_block() {

	const unsigned payload_size_type = b_.u(3, "payload_size_type");
	const unsigned payload_type = b_.u(5, "payload_type");

	unsigned payload_byte_size = 0;
	switch (payload_size_type) {
	case 0:
		payload_byte_size = 0;
		break;
	case 1:
		payload_byte_size = 1;
		break;
	case 2:
		payload_byte_size = 2;
		break;
	case 3:
		payload_byte_size = 3;
		break;
	case 4:
		payload_byte_size = 4;
		break;
	case 5:
		payload_byte_size = 5;
		break;
	case 7:
		payload_byte_size = (unsigned)read_multibyte(b_, "payload_byte_size");
		break;
	default:
		CHECK(0);
		break;
	}

	// Read all payload into packet, then parse from that
	const Packet packet = b_.bytes(payload_byte_size);
	const PacketView view(packet);
	BitstreamUnpacker payload_bitstream(view);

	switch (payload_type) {
	case 0:
		parse_sequence_config(dst_configuration_.sequence_configuration, payload_bitstream);
		return SyntaxBlock_Sequence;
	case 1:
		parse_global_config(dst_configuration_.global_configuration, payload_bitstream);
		return SyntaxBlock_Global;
	case 2:
		parse_picture_config(dst_configuration_.picture_configuration, payload_bitstream,
		                     dst_configuration_.global_configuration.num_residual_layers,
		                     dst_configuration_.global_configuration.temporal_enabled);
		return SyntaxBlock_Picture;
	case 3:
		parse_encoded_data(dst_configuration_, payload_bitstream, dst_configuration_.global_configuration.num_processed_planes,
		                   symbols_);
		return SyntaxBlock_EncodedData;
	case 4:
		parse_encoded_data_tiled(dst_configuration_, payload_bitstream,
		                         dst_configuration_.global_configuration.num_processed_planes, symbols_);
		return SyntaxBlock_EncodedData_Tiled;
	case 5:
		INFO("parse_additional_info");
		parse_additional_info(dst_configuration_.additional_info, payload_bitstream);
		return SyntaxBlock_Additional_Info;
	case 6:
		parse_filler(payload_bitstream);
		return SyntaxBlock_Filler;
	default:
		CHECK(0);
	}

	return 0;
}

// Read each configuration
//
void Deserializer::parse_sequence_config(SequenceConfiguration &sequence_configuration, BitstreamUnpacker &b) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ sequence_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamUnpacker::ScopedContextLabel label(b, "sequence_config");

	const unsigned profile_idc = b.u(4, "profile_idc");
	sequence_configuration.level_idc = b.u(4, "level_idc");
	sequence_configuration.sublevel_idc = b.u(2, "sublevel_idc");
	sequence_configuration.conformance_window = b.u(1, "conformance_window_flag");
	b.u(5, "reserved");
	if (profile_idc == 15 || sequence_configuration.level_idc == 15) {
		const unsigned extended_profile_idc = b.u(3, "extended_profile_idc");
		const unsigned extended_level_idc = b.u(3, "extended_level_idc");
		b.u(1, "reserved");
	}
	if (sequence_configuration.conformance_window) {
		sequence_configuration.conf_win_left_offset = (unsigned)read_multibyte(b, "conf_win_left_offset");
		sequence_configuration.conf_win_right_offset = (unsigned)read_multibyte(b, "conf_win_right_offset");
		sequence_configuration.conf_win_top_offset = (unsigned)read_multibyte(b, "conf_win_top_offset");
		sequence_configuration.conf_win_bottom_offset = (unsigned)read_multibyte(b, "conf_win_bottom_offset");
	}

	switch (profile_idc) {
	case 0:
		sequence_configuration.profile_idc = Profile_Main;
		break;
	case 1:
		sequence_configuration.profile_idc = Profile_Main444;
		break;
	default:
		CHECK(0);
		break;
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ sequence_config <<<< \n");
	fflush(goBits);
#endif
}

void Deserializer::parse_global_config(GlobalConfiguration &global_configuration, BitstreamUnpacker &b) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ global_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamUnpacker::ScopedContextLabel label(b, "global_config");

	const unsigned processed_planes_type = b.u(1, "processed_planes_type");
	const unsigned resolution_type = b.u(6, "resolution_type");
	const unsigned transform_type = b.u(1, "transform_type");
	const unsigned chroma_sampling_type = b.u(2, "chroma_sampling_type");
	const unsigned base_depth_type = b.u(2, "base_depth_type");
	const unsigned enhancement_depth_type = b.u(2, "enhancement_depth_type");
	const unsigned temporal_step_width_modifier_signalled = b.u(1, "temporal_step_width_modifier_signalled");
	global_configuration.predicted_residual_enabled = b.u(1, "predicted_residual_mode");
	global_configuration.temporal_tile_intra_signalling_enabled = b.u(1, "temporal_tile_intra_signalling_enabled");
	global_configuration.temporal_enabled = b.u(1, "temporal_enabled");
	const unsigned upsample_type = b.u(3, "upsample_type");
	const unsigned level_1_filtering_signalled = b.u(1, "level_1_filtering_signalled");
	const unsigned scaling_mode_level1 = b.u(2, "scaling_mode_level1");
	const unsigned scaling_mode_level2 = b.u(2, "scaling_mode_level2");
	const unsigned tile_dimensions_type = b.u(2, "tile_dimensions_type");
	const unsigned user_data_enabled = b.u(2, "user_data_enabled");
	switch (user_data_enabled) {
	case 0:
		global_configuration.user_data_enabled = UserData_None;
		break;
	case 1:
		global_configuration.user_data_enabled = UserData_2bits;
		break;
	case 2:
		global_configuration.user_data_enabled = UserData_6bits;
		break;
	default:
		CHECK(0);
		break;
	}
	global_configuration.level1_depth_flag = b.u(1, "level1_depth_flag");

	const unsigned chroma_step_width_flag = b.u(1, "chroma_step_width_flag");

	if (processed_planes_type == 0) {
		global_configuration.num_processed_planes = 1; // Y
	} else {
		const unsigned planes_type = b.u(4, "planes_type");
		b.u(4, "reserved");
		switch (planes_type) {
		case 1:
			global_configuration.num_processed_planes = 3; // YUV
			break;
		default:
			CHECK(0);
		}
	}

	if (temporal_step_width_modifier_signalled)
		global_configuration.temporal_step_width_modifier = b.u(8, "temporal_step_width_modifier");
	else
		global_configuration.temporal_step_width_modifier = 48;

	if (upsample_type == 4) {
		global_configuration.upsampling_coefficients[0] = b.u(16, "upsampling_coefficient[0]");
		global_configuration.upsampling_coefficients[1] = b.u(16, "upsampling_coefficient[1]");
		global_configuration.upsampling_coefficients[2] = b.u(16, "upsampling_coefficient[2]");
		global_configuration.upsampling_coefficients[3] = b.u(16, "upsampling_coefficient[3]");
	}

	if (level_1_filtering_signalled) {
		global_configuration.level_1_filtering_first_coefficient = b.u(4, "level_1_filtering_first_coefficient");
		global_configuration.level_1_filtering_second_coefficient = b.u(4, "level_1_filtering_second_coefficient");
	} else {
		global_configuration.level_1_filtering_first_coefficient = 0;
		global_configuration.level_1_filtering_second_coefficient = 0;
	}

	switch (tile_dimensions_type) {
	case 0:
		global_configuration.tile_dimensions_type = TileDimensions_None;
		global_configuration.tile_width = 0;
		global_configuration.tile_height = 0;
		break;
	case 1:
		global_configuration.tile_dimensions_type = TileDimensions_512x256;
		global_configuration.tile_width = 512;
		global_configuration.tile_height = 256;
		break;
	case 2:
		global_configuration.tile_dimensions_type = TileDimensions_1024x512;
		global_configuration.tile_width = 1024;
		global_configuration.tile_height = 512;
		break;
	case 3:
		global_configuration.tile_dimensions_type = TileDimensions_Custom;
		global_configuration.tile_width = b.u(16, "custom_tile_width");
		global_configuration.tile_height = b.u(16, "custom_tile_height");
		break;
	}

	if (tile_dimensions_type > 0) {
		b.u(5, "reserved");
		global_configuration.compression_type_entropy_enabled_per_tile = b.u(1, "compression_type_entropy_enabled_per_tile");
		const unsigned compression_type_size_per_tile = b.u(2, "compression_type_size_per_tile");
		switch (compression_type_size_per_tile) {
		case 0:
			global_configuration.compression_type_size_per_tile = CompressionType_None;
			break;
		case 1:
			global_configuration.compression_type_size_per_tile = CompressionType_Prefix;
			break;
		case 2:
			global_configuration.compression_type_size_per_tile = CompressionType_Prefix_OnDiff;
			break;
		default:
			CHECK(0);
			break;
		}
	}

	if (resolution_type > 0 && resolution_type < 51) {
		global_configuration.resolution_width = resolution_table[resolution_type][0];
		global_configuration.resolution_height = resolution_table[resolution_type][1];
	} else if (resolution_type == 63) {
		global_configuration.resolution_width = b.u(16, "resolution_width");
		global_configuration.resolution_height = b.u(16, "resolution_height");
	} else {
		CHECK(0);
	}

	if (chroma_step_width_flag == 1) {
		global_configuration.chroma_step_width_multiplier = b.u(8, "chroma_step_width_multiplier");
	} else {
		global_configuration.chroma_step_width_multiplier = 64;
	}

	switch (chroma_sampling_type) {
	case 0:
		global_configuration.colourspace = Colourspace_Y;
		global_configuration.num_image_planes = 1;
		CHECK(global_configuration.num_image_planes >= global_configuration.num_processed_planes);
		break;
	case 1:
		global_configuration.colourspace = Colourspace_YUV420;
		global_configuration.num_image_planes = 3;
		break;
	case 2:
		global_configuration.colourspace = Colourspace_YUV422;
		global_configuration.num_image_planes = 3;
		break;
	case 3:
		global_configuration.colourspace = Colourspace_YUV444;
		global_configuration.num_image_planes = 3;
		break;
	default:
		CHECK(0);
		break;
	}

	switch (transform_type) {
	case 0:
		global_configuration.transform_block_size = 2;
		global_configuration.num_residual_layers = 4;
		break;
	case 1:
		global_configuration.transform_block_size = 4;
		global_configuration.num_residual_layers = 16;
		break;
	default:
		CHECK(0);
		break;
	}

	switch (base_depth_type) {
	case 0:
		global_configuration.base_depth = 8;
		break;
	case 1:
		global_configuration.base_depth = 10;
		break;
	case 2:
		global_configuration.base_depth = 12;
		break;
	case 3:
		global_configuration.base_depth = 14;
		break;
	default:
		CHECK(0);
		break;
	}

	switch (enhancement_depth_type) {
	case 0:
		global_configuration.enhancement_depth = 8;
		switch (global_configuration.colourspace) {
		case Colourspace_Y:
			global_configuration.image_format = IMAGE_FORMAT_Y8;
			break;
		case Colourspace_YUV420:
			global_configuration.image_format = IMAGE_FORMAT_YUV420P8;
			break;
		case Colourspace_YUV422:
			global_configuration.image_format = IMAGE_FORMAT_YUV422P8;
			break;
		case Colourspace_YUV444:
			global_configuration.image_format = IMAGE_FORMAT_YUV444P8;
			break;
		default:
			CHECK(0);
			break;
		}
		break;
	case 1:
		global_configuration.enhancement_depth = 10;
		switch (global_configuration.colourspace) {
		case Colourspace_Y:
			global_configuration.image_format = IMAGE_FORMAT_Y10;
			break;
		case Colourspace_YUV420:
			global_configuration.image_format = IMAGE_FORMAT_YUV420P10;
			break;
		case Colourspace_YUV422:
			global_configuration.image_format = IMAGE_FORMAT_YUV422P10;
			break;
		case Colourspace_YUV444:
			global_configuration.image_format = IMAGE_FORMAT_YUV444P10;
			break;
		default:
			CHECK(0);
			break;
		}
		break;
	case 2:
		global_configuration.enhancement_depth = 12;
		switch (global_configuration.colourspace) {
		case Colourspace_Y:
			global_configuration.image_format = IMAGE_FORMAT_Y12;
			break;
		case Colourspace_YUV420:
			global_configuration.image_format = IMAGE_FORMAT_YUV420P12;
			break;
		case Colourspace_YUV422:
			global_configuration.image_format = IMAGE_FORMAT_YUV422P12;
			break;
		case Colourspace_YUV444:
			global_configuration.image_format = IMAGE_FORMAT_YUV444P12;
			break;
		default:
			CHECK(0);
			break;
		}
		break;
	case 3:
		global_configuration.enhancement_depth = 14;
		switch (global_configuration.colourspace) {
		case Colourspace_Y:
			global_configuration.image_format = IMAGE_FORMAT_Y14;
			break;
		case Colourspace_YUV420:
			global_configuration.image_format = IMAGE_FORMAT_YUV420P14;
			break;
		case Colourspace_YUV422:
			global_configuration.image_format = IMAGE_FORMAT_YUV422P14;
			break;
		case Colourspace_YUV444:
			global_configuration.image_format = IMAGE_FORMAT_YUV444P14;
			break;
		default:
			CHECK(0);
			break;
		}
		break;
	default:
		CHECK(0);
		break;
	}

	switch (upsample_type) {
	case 0:
		global_configuration.upsample = Upsample_Nearest;
		break;
	case 1:
		global_configuration.upsample = Upsample_Linear;
		break;
	case 2:
		global_configuration.upsample = Upsample_Cubic;
		break;
	case 3:
		global_configuration.upsample = Upsample_ModifiedCubic;
		break;
	case 4:
		global_configuration.upsample = Upsample_AdaptiveCubic;
		break;
	default:
		CHECK(0);
		break;
	}

	switch (scaling_mode_level1) {
	case 0:
		global_configuration.scaling_mode[LOQ_LEVEL_1] = ScalingMode_None;
		break;
	case 1:
		global_configuration.scaling_mode[LOQ_LEVEL_1] = ScalingMode_1D;
		break;
	case 2:
		global_configuration.scaling_mode[LOQ_LEVEL_1] = ScalingMode_2D;
		break;
	default:
		CHECK(0);
		break;
	}

	switch (scaling_mode_level2) {
	case 0:
		global_configuration.scaling_mode[LOQ_LEVEL_2] = ScalingMode_None;
		break;
	case 1:
		global_configuration.scaling_mode[LOQ_LEVEL_2] = ScalingMode_1D;
		break;
	case 2:
		global_configuration.scaling_mode[LOQ_LEVEL_2] = ScalingMode_2D;
		break;
	default:
		CHECK(0);
		break;
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ global_config <<<< \n");
	fflush(goBits);
#endif
}

void Deserializer::parse_picture_config(PictureConfiguration &picture_configuration, BitstreamUnpacker &b,
                                        unsigned num_residual_layers, bool temporal_enabled) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ picture_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamUnpacker::ScopedContextLabel label(b, "picture_config");
	unsigned picture_type;

	picture_configuration.enhancement_enabled = !b.u(1, "no_enhancement_bit");
	if (picture_configuration.enhancement_enabled) {
		const unsigned quant_matrix_mode = b.u(3, "quant_matrix_mode");
		picture_configuration.dequant_offset_signalled = b.u(1, "dequant_offset_signalled");
		picture_type = b.u(1, "picture_type");
		picture_configuration.temporal_refresh = b.u(1, "temporal_refresh");
		if (temporal_enabled && !picture_configuration.temporal_refresh)
			picture_configuration.temporal_signalling_present = true;
		else
			picture_configuration.temporal_signalling_present = false;
		const unsigned step_width_level1_enabled = b.u(1, "step_width_level1_enabled");
		picture_configuration.step_width_loq[LOQ_LEVEL_2] = b.u(15, "step_width_level2");
		CHECK(picture_configuration.step_width_loq[LOQ_LEVEL_2] > 0);
		picture_configuration.dithering_control = b.u(1, "dithering_control");

		switch (quant_matrix_mode) {
		case 0:
			picture_configuration.quant_matrix_mode = QuantMatrix_BothPrevious;
			break;
		case 1:
			picture_configuration.quant_matrix_mode = QuantMatrix_BothDefault;
			break;
		case 2:
			picture_configuration.quant_matrix_mode = QuantMatrix_SameAndCustom;
			break;
		case 3:
			picture_configuration.quant_matrix_mode = QuantMatrix_Level2CustomLevel1Default;
			break;
		case 4:
			picture_configuration.quant_matrix_mode = QuantMatrix_Level2DefaultLevel1Custom;
			break;
		case 5:
			picture_configuration.quant_matrix_mode = QuantMatrix_DifferentAndCustom;
			break;
		default:
			CHECK(0);
			break;
		}

		if (picture_type) {
			const unsigned field_type = b.u(1, "field_type");
			switch (field_type) {
			case 0:
				picture_configuration.field_type = FieldType_Top;
				break;
			case 1:
				picture_configuration.field_type = FieldType_Bottom;
				break;
			default:
				CHECK(0);
				break;
			}
			b.u(7, "reserved");
		}

		if (step_width_level1_enabled) {
			picture_configuration.step_width_loq[LOQ_LEVEL_1] = b.u(15, "step_width_level1");
			CHECK(picture_configuration.step_width_loq[LOQ_LEVEL_1] > 0);
			picture_configuration.level_1_filtering_enabled = b.u(1, "level_1_filtering_enabled");
		} else
			picture_configuration.step_width_loq[LOQ_LEVEL_1] = MAX_STEP_WIDTH;

		if (picture_configuration.quant_matrix_mode == QuantMatrix_SameAndCustom ||
		    picture_configuration.quant_matrix_mode == QuantMatrix_Level2CustomLevel1Default ||
		    picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
			for (unsigned layerIndex = 0; layerIndex < num_residual_layers; layerIndex++)
				picture_configuration.qm_coefficient_2[layerIndex] = b.u(8, "qm_coefficient_0");
		}
		if (picture_configuration.quant_matrix_mode == QuantMatrix_Level2DefaultLevel1Custom ||
		    picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
			for (unsigned layerIndex = 0; layerIndex < num_residual_layers; layerIndex++)
				picture_configuration.qm_coefficient_1[layerIndex] = b.u(8, "qm_coefficient_1");
		}

		if (picture_configuration.dequant_offset_signalled) {
			const unsigned dequant_offset_mode = b.u(1, "dequant_offset_mode");
			switch (dequant_offset_mode) {
			case 0:
				picture_configuration.dequant_offset_mode = DequantOffset_Default;
				break;
			case 1:
				picture_configuration.dequant_offset_mode = DequantOffset_ConstOffset;
				break;
			default:
				CHECK(0);
				break;
			}
			picture_configuration.dequant_offset = b.u(7, "dequant_offset");
		}

		if (picture_configuration.dithering_control) {
			const unsigned dithering_type = b.u(2, "dithering_type");
			switch (dithering_type) {
			case 0:
				picture_configuration.dithering_type = Dithering_None;
				break;
			case 1:
				picture_configuration.dithering_type = Dithering_Uniform;
				break;
			default:
				CHECK(0);
				break;
			}
			b.u(1, "reserved");
			if (picture_configuration.dithering_type != Dithering_None)
				picture_configuration.dithering_strength = b.u(5, "dithering_strength");
			else
				b.u(5, "reserved");
		}
	} else {
		b.u(4, "reserved");
		picture_type = b.u(1, "picture_type");
		picture_configuration.temporal_refresh = b.u(1, "temporal_refresh");
		picture_configuration.temporal_signalling_present = b.u(1, "temporal_signalling_present");
	}

	switch (picture_type) {
	case 0:
		picture_configuration.picture_type = PictureType_Frame;
		break;
	case 1:
		picture_configuration.picture_type = PictureType_Field;
		break;
	default:
		CHECK(0);
		break;
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ picture_config <<<< \n");
	fflush(goBits);
#endif
}

// Number of encoded layers in bitstream - residual + temporal
static unsigned total_layers(const SignaledConfiguration &configuration, unsigned plane, unsigned loq) {
	return configuration.global_configuration.num_residual_layers +
	       ((loq == LOQ_LEVEL_2 && configuration.picture_configuration.temporal_signalling_present) ? 1 : 0);
}

static unsigned first_layer(const SignaledConfiguration &configuration) {
	if (configuration.picture_configuration.enhancement_enabled)
		return 0;
	else
		// First layer is temporal
		return configuration.global_configuration.num_residual_layers;
}

static bool is_temporal_layer(const SignaledConfiguration &configuration, unsigned plane, unsigned loq, unsigned layer) {
	return layer == configuration.global_configuration.num_residual_layers;
}

void Deserializer::parse_encoded_data(SignaledConfiguration &dst_configuration, BitstreamUnpacker &b, unsigned num_planes,
                                      Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data >>>> \n");
	fflush(goBits);
#endif
	bool entropy_enabled[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {false};
	bool rle_only[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {false};

	for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
			for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq); ++layer) {
				BitstreamUnpacker::ScopedContextLabel label(b, "encoded_data");
				entropy_enabled[plane][loq][layer] = b.u(1, "entropy_enabled");
				rle_only[plane][loq][layer] = b.u(1, "rle_only");
			}
		}
	}

	// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		CHECK(b.u(1, "alignment") == 0);

	for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
		if (dst_configuration.picture_configuration.enhancement_enabled ||
		    dst_configuration.picture_configuration.temporal_signalling_present) {
			for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
				for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq);
				     ++layer) {
					const SurfaceConfiguration &surface_configuration = dst_configuration.surface_configuration[plane][loq][layer];
					Packet data;

					if (entropy_enabled[plane][loq][layer]) {
						BitstreamUnpacker::ScopedContextLabel label(b, format("surface[%d][%d][%d]", plane, loq, layer));
						const uint64_t data_size = read_multibyte(b, "data_size");
						CHECK(data_size < INT_MAX);
						data = b.bytes((unsigned)data_size);
					}

					PacketView view(data);
					BitstreamUnpacker pb(view);
					if (!is_temporal_layer(dst_configuration, plane, loq, layer)) {
						const bool use_tiled_encoding_order = dst_configuration.global_configuration.temporal_enabled ||
						                                      dst_configuration.global_configuration.tile_dimensions_type > 0;
						if (use_tiled_encoding_order) {
							symbols[plane][loq][layer] = EntropyDecoderResidualsTiled().process(
							    surface_configuration.width, surface_configuration.height, entropy_enabled[plane][loq][layer],
							    rle_only[plane][loq][layer], pb, dst_configuration.global_configuration.transform_block_size);
						} else {
							symbols[plane][loq][layer] = EntropyDecoderResiduals().process(
							    surface_configuration.width, surface_configuration.height, entropy_enabled[plane][loq][layer],
							    rle_only[plane][loq][layer], pb);
						}
					} else {
						symbols[plane][loq][layer] = EntropyDecoderTemporal().process(
						    surface_configuration.width, surface_configuration.height, entropy_enabled[plane][loq][layer],
						    rle_only[plane][loq][layer], pb, dst_configuration.global_configuration.transform_block_size,
						    dst_configuration.global_configuration.temporal_tile_intra_signalling_enabled);
					}
				}
			}
		}
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data <<<< \n");
	fflush(goBits);
#endif
}

static Surface decode_layer(const SignaledConfiguration &dst_configuration, unsigned plane, unsigned loq, unsigned layer,
                            unsigned width, unsigned height, bool entropy_enabled, bool rle_only, BitstreamUnpacker &b) {

	if (!is_temporal_layer(dst_configuration, plane, loq, layer)) {
		const bool use_tiled_encoding_order = dst_configuration.global_configuration.temporal_enabled ||
		                                      dst_configuration.global_configuration.tile_dimensions_type > 0;
		if (use_tiled_encoding_order) {
			return EntropyDecoderResidualsTiled().process(width, height, entropy_enabled, rle_only, b,
			                                              dst_configuration.global_configuration.transform_block_size);
		} else {
			return EntropyDecoderResiduals().process(width, height, entropy_enabled, rle_only, b);
		}

	} else {
		return EntropyDecoderTemporal().process(width, height, entropy_enabled, rle_only, b,
		                                        dst_configuration.global_configuration.transform_block_size,
		                                        dst_configuration.global_configuration.temporal_tile_intra_signalling_enabled);
	}
}

static Surface assemble_layer(SignaledConfiguration &dst_configuration, unsigned plane, unsigned loq, unsigned layer,
                              unsigned width, unsigned height, unsigned tiles_x, unsigned tiles_y, unsigned tile_width,
                              unsigned tile_height, const std::vector<Surface> &tiles) {

	if (is_temporal_layer(dst_configuration, plane, layer, loq)) {
		std::vector<SurfaceView<uint8_t>> src;
		for (const auto &t : tiles)
			src.push_back(SurfaceView<uint8_t>(t));

		return Surface::build_from<uint8_t>()
		    .generate(width, height,
		              [&](unsigned x, unsigned y) -> uint8_t {
			              const unsigned tx = x / tile_width;
			              const unsigned ty = y / tile_height;
			              CHECK(tx < tiles_x);
			              CHECK(ty < tiles_y);
			              return src[ty * tiles_x + tx].read(x % tile_width, y % tile_height);
		              })
		    .finish();
	} else {
		std::vector<SurfaceView<int16_t>> src;
		for (const auto &t : tiles)
			src.push_back(SurfaceView<int16_t>(t));

		return Surface::build_from<int16_t>()
		    .generate(width, height,
		              [&](unsigned x, unsigned y) -> int16_t {
			              const unsigned tx = x / tile_width;
			              const unsigned ty = y / tile_height;
			              CHECK(tx < tiles_x);
			              CHECK(ty < tiles_y);
			              return src[ty * tiles_x + tx].read(x % tile_width, y % tile_height);
		              })
		    .finish();
	}
}

void Deserializer::parse_encoded_data_tiled(SignaledConfiguration &dst_configuration, BitstreamUnpacker &b, unsigned num_planes,
                                            Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {

	CHECK(dst_configuration.global_configuration.tile_dimensions_type != TileDimensions_None);

	const unsigned transform_block_size = dst_configuration.global_configuration.transform_block_size;
	Dimensions dimensions;
	dimensions.set(dst_configuration, dst_configuration.global_configuration.resolution_width,
	               dst_configuration.global_configuration.resolution_height);

	// Per layer sizes
	struct {
		unsigned width;
		unsigned height;
		unsigned tile_width;
		unsigned tile_height;
		unsigned tiles_x;
		unsigned tiles_y;
		unsigned num_tiles;
	} sizes[MAX_NUM_PLANES][MAX_NUM_LOQS] = {0};

	unsigned total_tiles = 0;

	// Fill in layer sizes
	for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
			const unsigned num_layers = total_layers(dst_configuration, plane, loq) - first_layer(dst_configuration);

			const unsigned tile_width = dimensions.tile_width(plane, loq);
			const unsigned tile_height = dimensions.tile_height(plane, loq);

			// Figure out tile breakdown for this level
			sizes[plane][loq].width = dimensions.layer_width(plane, loq);
			sizes[plane][loq].height = dimensions.layer_height(plane, loq);
			sizes[plane][loq].tile_width = tile_width;
			sizes[plane][loq].tile_height = tile_height;
			sizes[plane][loq].tiles_x = (sizes[plane][loq].width + tile_width - 1) / tile_width;
			sizes[plane][loq].tiles_y = (sizes[plane][loq].height + tile_height - 1) / tile_height;
			sizes[plane][loq].num_tiles = sizes[plane][loq].tiles_x * sizes[plane][loq].tiles_y;

			total_tiles += sizes[plane][loq].num_tiles * num_layers;
		}
	}

	// Parse per-layer rle_only flags
	bool rle_only[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {false};

	for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
			if (dst_configuration.picture_configuration.enhancement_enabled) {
				for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq);
				     ++layer) {
					rle_only[plane][loq][layer] = b.u(1, "rle_only");
				}
			}
		}
	}

	// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		CHECK(b.u(1, "alignment") == 0);

	std::vector<bool> entropy_enabled(total_tiles, false);

	// Parse per tile entropy_enabled
	if (!dst_configuration.global_configuration.compression_type_entropy_enabled_per_tile) {

		int idx = 0;
		for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
			if (dst_configuration.picture_configuration.enhancement_enabled) {
				for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
					for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq);
					     ++layer) {
						for (unsigned tile = 0; tile < sizes[plane][loq].num_tiles; ++tile) {
							entropy_enabled[idx] = b.u(1, "entropy_enabled");
							idx++;
						}
					}
				}
			}
		}

	} else {
		if (dst_configuration.picture_configuration.enhancement_enabled) {
			Surface ee = EntropyDecoderFlags().process(static_cast<unsigned>(entropy_enabled.size()), 1, b);

			const auto eeview = ee.view_as<uint8_t>();
			for (unsigned i = 0; i < entropy_enabled.size(); ++i)
				entropy_enabled[i] = eeview.read(i, 0);
		}
	}

	// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		CHECK(b.u(1, "alignment") == 0);

	if (dst_configuration.global_configuration.compression_type_size_per_tile == CompressionType::CompressionType_None) {
		// Uncompressed tile sizes and data
		int idx = 0;
		for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
			for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
				for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq);
				     ++layer) {
					std::vector<Surface> tiles;
					for (unsigned ty = 0; ty < sizes[plane][loq].tiles_y; ++ty) {
						for (unsigned tx = 0; tx < sizes[plane][loq].tiles_x; ++tx) {
							// Where is tile going to go?
							const unsigned tx0 = tx * sizes[plane][loq].tile_width;
							const unsigned ty0 = ty * sizes[plane][loq].tile_height;
							// const unsigned tx1 = std::min((tx + 1) * tile_width, sizes[plane][loq].width);
							const unsigned tx1 = LCEVC_MIN((tx + 1) * sizes[plane][loq].tile_width, sizes[plane][loq].width);
							// const unsigned ty1 = std::min((ty + 1) * tile_height, sizes[plane][loq].height);
							const unsigned ty1 = LCEVC_MIN((ty + 1) * sizes[plane][loq].tile_height, sizes[plane][loq].height);

							Packet data;
							if (entropy_enabled[idx]) {
								const uint64_t data_size = read_multibyte(b, "data_size");
								CHECK(data_size < INT_MAX);
								data = b.bytes((unsigned)data_size);
							}
							PacketView view(data);
							BitstreamUnpacker pb(view);
							tiles.push_back(decode_layer(dst_configuration, plane, loq, layer, tx1 - tx0, ty1 - ty0,
							                             entropy_enabled[idx], rle_only[plane][loq][layer], pb));
							idx++;
						}
					}
					symbols[plane][loq][layer] =
					    assemble_layer(dst_configuration, plane, layer, loq, sizes[plane][loq].width, sizes[plane][loq].height,
					                   sizes[plane][loq].tiles_x, sizes[plane][loq].tiles_y, dimensions.tile_width(plane, loq),
					                   dimensions.tile_height(plane, loq), tiles);
				}
			}
		}
	} else {
		int idx = 0;
		for (unsigned plane = 0; plane < dst_configuration.global_configuration.num_processed_planes; ++plane) {
			for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
				for (unsigned layer = first_layer(dst_configuration); layer < total_layers(dst_configuration, plane, loq);
				     ++layer) {

					// Does any tile contain data?
					bool any_tile_enabled = false;
					for (unsigned x = 0; x < sizes[plane][loq].num_tiles; x++) {
						if (entropy_enabled[idx + x]) {
							any_tile_enabled = true;
							break;
						}
					}

					Surface sz;
					if (any_tile_enabled) {
						// Decode sizes for this layer
						sz = EntropyDecoderSizes().process(sizes[plane][loq].num_tiles, 1, b, entropy_enabled, idx,
						                                   dst_configuration.global_configuration.compression_type_size_per_tile);
					} else {
						sz = Surface::build_from<uint16_t>().fill(0, sizes[plane][loq].num_tiles, 1).finish();
					}
					// Byte alignment
#if defined __OPT_MODULO__
					while ((b.bit_offset() & 0x07))
#else
					while ((b.bit_offset() % 8 != 0))
#endif
						CHECK(b.u(1, "alignment") == 0);

					const auto data_sizes = sz.view_as<uint16_t>();

					std::vector<Surface> tiles;

					for (unsigned ty = 0; ty < sizes[plane][loq].tiles_y; ++ty) {
						for (unsigned tx = 0; tx < sizes[plane][loq].tiles_x; ++tx) {
							// Where is tile going to go?
							const unsigned tx0 = tx * sizes[plane][loq].tile_width;
							const unsigned ty0 = ty * sizes[plane][loq].tile_height;
							const unsigned tx1 = LCEVC_MIN((tx + 1) * sizes[plane][loq].tile_width, sizes[plane][loq].width);
							const unsigned ty1 = LCEVC_MIN((ty + 1) * sizes[plane][loq].tile_height, sizes[plane][loq].height);

							Packet data;
							if (entropy_enabled[idx]) {
								const uint64_t data_size = data_sizes.read(ty * sizes[plane][loq].tiles_x + tx, 0);
								CHECK(data_size < INT_MAX && data_size > 0);
								data = b.bytes((unsigned)data_size);
							}
							PacketView view(data);
							BitstreamUnpacker pb(view);
							tiles.push_back(decode_layer(dst_configuration, plane, loq, layer, tx1 - tx0, ty1 - ty0,
							                             entropy_enabled[idx], rle_only[plane][loq][layer], pb));
							idx++;
						}
					}

					symbols[plane][loq][layer] =
					    assemble_layer(dst_configuration, plane, layer, loq, sizes[plane][loq].width, sizes[plane][loq].height,
					                   sizes[plane][loq].tiles_x, sizes[plane][loq].tiles_y, dimensions.tile_width(plane, loq),
					                   dimensions.tile_height(plane, loq), tiles);
				}
			}
		}
	}
}

void Deserializer::parse_additional_info(AdditionalInfo &additional_info, BitstreamUnpacker &b) {
	additional_info.additional_info_type = b.u(8, "additional_info_type");
	if (additional_info.additional_info_type == 0) {
		additional_info.payload_type = b.u(8, "payload_type");
		INFO("SeiPayload");
	} else if (additional_info.additional_info_type == 1) {
		INFO("VuiParameters");
	} else {
		// (additional_info.additional_info_type >= 2)
		CHECK(0);
	}
	// TODO
}

void Deserializer::parse_filler(BitstreamUnpacker &b) {
	BitstreamUnpacker::ScopedContextLabel label(b, "filler");
	// Check filler bytes are correct value
	while (!b.empty())
		CHECK(b.u(8, "filler") == 0xaa);
}

} // namespace lctm
