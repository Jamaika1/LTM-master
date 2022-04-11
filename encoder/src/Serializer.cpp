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
// Serializer.cpp
//

#include "Serializer.hpp"

#include "BitstreamPacker.hpp"
#include "Config.hpp"
#include "Crop.hpp"
#include "Dimensions.hpp"
#include "Diagnostics.hpp"
#include "EntropyEncoder.hpp"
#include "Misc.hpp"

#include <algorithm>
#include <climits>

namespace lctm {
// Write a multibyte encoded uint64_t
//
static void write_multibyte_bytes(BitstreamPacker &b, uint64_t value, bool more) {
	if (value > 0x7f)
		write_multibyte_bytes(b, value >> 7, true);

	b.u(1, more ? 1 : 0, "mb-more");
	b.u(7, value & 0x7f, "mb-bits");
}

static void write_multibyte(BitstreamPacker &b, uint64_t value, const std::string &label) {
	BitstreamPacker::ScopedContextLabel l(b, label);
	write_multibyte_bytes(b, value, false);
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

static_assert(ARRAY_SIZE(resolution_table) == 51, "Resolution table is broken");

//
//
Packet Serializer::emit(const SignaledConfiguration &configuration, unsigned block_mask,
                        const Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {
	BitstreamPacker bitstream;

	for (unsigned b = SyntaxBlock_Sequence; b <= SyntaxBlock_Filler; b <<= 1) {
		if (block_mask & b) {
			Packet p = emit_block(configuration, b, symbols);
			bitstream.bytes(p);
		}
	}

	return bitstream.finish();
}

Packet Serializer::emit_block(const SignaledConfiguration &configuration, unsigned block,
                              const Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {

	const unsigned num_residual_layers = configuration.global_configuration.num_residual_layers;

	// Emit block into a packet
	//
	BitstreamPacker contents_bitstream;

	unsigned payload_type{};

	switch (block) {
	case SyntaxBlock_Sequence:
		emit_sequence_configuration(configuration.sequence_configuration, contents_bitstream);
		payload_type = 0;
		break;

	case SyntaxBlock_Global:
		emit_global_configuration(configuration.global_configuration, contents_bitstream);
		payload_type = 1;
		break;

	case SyntaxBlock_Picture:
		emit_picture_configuration(configuration.picture_configuration, contents_bitstream, num_residual_layers);
		payload_type = 2;
		break;

	case SyntaxBlock_EncodedData:
		emit_encoded_data(configuration, contents_bitstream, symbols, num_residual_layers);
		payload_type = 3;
		break;

	case SyntaxBlock_EncodedData_Tiled:
		emit_encoded_data_tiled(configuration, contents_bitstream, symbols, num_residual_layers);
		payload_type = 4;
		break;

	case SyntaxBlock_Additional_Info:
		INFO("emit_additional_info");
		emit_additional_info(configuration.additional_info, contents_bitstream);
		payload_type = 5;
		break;

		/***
	case SyntaxBlock_Additional_Info:
		emit_additional_info(contents_bitstream);
		payload_type = 6;
		break;
		***/

	case SyntaxBlock_Filler:
		emit_filler(contents_bitstream, 0); // XXXX pass filler size
		payload_type = 6;
		break;

	default:
		CHECK(0);
	}

	Packet contents = contents_bitstream.finish();

	BitstreamPacker block_bitstream;

	// Block header
	unsigned payload_size_type = 0;
	switch (contents.size()) {
	case 0:
		payload_size_type = 0;
		break;
	case 1:
		payload_size_type = 1;
		break;
	case 2:
		payload_size_type = 2;
		break;
	case 3:
		payload_size_type = 3;
		break;
	case 4:
		payload_size_type = 4;
		break;
	case 5:
		payload_size_type = 5;
		break;
	default:
		payload_size_type = 7;
		break;
	}

	block_bitstream.u(3, payload_size_type, "payload_size_type");
	block_bitstream.u(5, payload_type, "payload_type");

	if (payload_size_type == 7)
		write_multibyte(block_bitstream, contents.size(), "payload_byte_size");

	// Block contents
	block_bitstream.bytes(PacketView(contents));

	return block_bitstream.finish();
}

// Write each configuration
//
void Serializer::emit_sequence_configuration(const SequenceConfiguration &sequence_configuration, BitstreamPacker &b) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ sequence_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamPacker::ScopedContextLabel label(b, "sequence_config");

	switch (sequence_configuration.profile_idc) {
	case Profile_Main:
		b.u(4, 0, "profile_idc");
		break;
	case Profile_Main444:
		b.u(4, 1, "profile_idc");
		break;
	default:
		CHECK(0);
	}
	b.u(4, sequence_configuration.level_idc, "level_idc");
	b.u(2, sequence_configuration.sublevel_idc, "sublevel_idc");
	b.u(1, sequence_configuration.conformance_window, "conformance_window");
	b.u(5, 0, "reserved");
	if (sequence_configuration.profile_idc == 15 || sequence_configuration.level_idc == 15) {
		b.u(3, sequence_configuration.extended_profile_idc, "extended_profile_idc");
		b.u(4, sequence_configuration.extended_level_idc, "extended_level_idc");
		b.u(1, 0, "reserved");
	}
	if (sequence_configuration.conformance_window) {
		write_multibyte(b, sequence_configuration.conf_win_left_offset, "conf_win_left_offset");
		write_multibyte(b, sequence_configuration.conf_win_right_offset, "conf_win_right_offset");
		write_multibyte(b, sequence_configuration.conf_win_top_offset, "conf_win_top_offset");
		write_multibyte(b, sequence_configuration.conf_win_bottom_offset, "conf_win_bottom_offset");
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ sequence_config <<<< \n");
	fflush(goBits);
#endif
}

void Serializer::emit_global_configuration(const GlobalConfiguration &global_configuration, BitstreamPacker &b) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ global_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamPacker::ScopedContextLabel label(b, "global_config");

	switch (global_configuration.num_processed_planes) {
	case 1:
		b.u(1, 0, "processed_planes_type");
		break;
	case 3:
		b.u(1, 1, "processed_planes_type");
		break;
	default:
		CHECK(0);
	}

	unsigned resolution_type = 0;
	for (resolution_type = 0; resolution_type < ARRAY_SIZE(resolution_table); ++resolution_type)
		if (global_configuration.resolution_width == resolution_table[resolution_type][0] &&
		    global_configuration.resolution_height == resolution_table[resolution_type][1])
			break;
	if (resolution_type < ARRAY_SIZE(resolution_table)) {
		b.u(6, resolution_type, "resolution_type");
	} else {
		resolution_type = 63;
		b.u(6, 63, "resolution_type");
	}

	switch (global_configuration.transform_block_size) {
	case 2:
		b.u(1, 0, "transform_type");
		break;
	case 4:
		b.u(1, 1, "transform_type");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.colourspace) {
	case Colourspace_Y:
		b.u(2, 0, "chroma_sampling_type");
		break;
	case Colourspace_YUV420:
		b.u(2, 1, "chroma_sampling_type");
		break;
	case Colourspace_YUV422:
		b.u(2, 2, "chroma_sampling_type");
		break;
	case Colourspace_YUV444:
		b.u(2, 3, "chroma_sampling_type");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.base_depth) {
	case 8:
		b.u(2, 0, "base_depth_type");
		break;
	case 10:
		b.u(2, 1, "base_depth_type");
		break;
	case 12:
		b.u(2, 2, "base_depth_type");
		break;
	case 14:
		b.u(2, 3, "base_depth_type");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.enhancement_depth) {
	case 8:
		b.u(2, 0, "enhancement_depth_type");
		break;
	case 10:
		b.u(2, 1, "enhancement_depth_type");
		break;
	case 12:
		b.u(2, 2, "enhancement_depth_type");
		break;
	case 14:
		b.u(2, 3, "enhancement_depth_type");
		break;
	default:
		CHECK(0);
	}

	if (global_configuration.temporal_step_width_modifier == 48)
		b.u(1, 0, "temporal_step_width_modifier_signalled");
	else
		b.u(1, 1, "temporal_step_width_modifier_signalled");

	b.u(1, global_configuration.predicted_residual_enabled, "predicted_residual_mode");
	b.u(1, global_configuration.temporal_tile_intra_signalling_enabled, "temporal_tile_intra_signalling_enabled");
	b.u(1, global_configuration.temporal_enabled, "temporal_enabled");

	switch (global_configuration.upsample) {
	case Upsample_Nearest:
		b.u(3, 0, "upsample_type");
		break;
	case Upsample_Linear:
		b.u(3, 1, "upsample_type");
		break;
	case Upsample_Cubic:
		b.u(3, 2, "upsample_type");
		break;
	case Upsample_ModifiedCubic:
		b.u(3, 3, "upsample_type");
		break;
	case Upsample_AdaptiveCubic:
		b.u(3, 4, "upsample_type");
		break;
	default:
		CHECK(0);
	}

	if (global_configuration.level_1_filtering_first_coefficient == 0 &&
	    global_configuration.level_1_filtering_second_coefficient == 0)
		b.u(1, 0, "level_1_filtering_signalled");
	else
		b.u(1, 1, "level_1_filtering_signalled");

	switch (global_configuration.scaling_mode[LOQ_LEVEL_1]) {
	case ScalingMode_None:
		b.u(2, 0, "scaling_mode_level1");
		break;
	case ScalingMode_1D:
		b.u(2, 1, "scaling_mode_level1");
		break;
	case ScalingMode_2D:
		b.u(2, 2, "scaling_mode_level1");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.scaling_mode[LOQ_LEVEL_2]) {
	case ScalingMode_None:
		b.u(2, 0, "scaling_mode_level2");
		break;
	case ScalingMode_1D:
		b.u(2, 1, "scaling_mode_level2");
		break;
	case ScalingMode_2D:
		b.u(2, 2, "scaling_mode_level2");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.tile_dimensions_type) {
	case TileDimensions_None:
		b.u(2, 0, "tile_dimensions_type");
		break;
	case TileDimensions_512x256:
		b.u(2, 1, "tile_dimensions_type");
		break;
	case TileDimensions_1024x512:
		b.u(2, 2, "tile_dimensions_type");
		break;
	case TileDimensions_Custom:
		b.u(2, 3, "tile_dimensions_type");
		break;
	default:
		CHECK(0);
	}

	switch (global_configuration.user_data_enabled) {
	case UserData_None:
		b.u(2, 0, "user_data_enabled");
		break;
	case UserData_2bits:
		b.u(2, 1, "user_data_enabled");
		break;
	case UserData_6bits:
		b.u(2, 2, "user_data_enabled");
		break;
	default:
		CHECK(0);
	}

	b.u(1, global_configuration.level1_depth_flag, "level1_depth_flags");

	if (global_configuration.chroma_step_width_multiplier != 64)
		b.u(1, 1, "chroma_step_width_flag");
	else
		b.u(1, 0, "chroma_step_width_flag");

	if (global_configuration.num_processed_planes != 1) {
		CHECK(global_configuration.num_processed_planes == 3);
		b.u(4, 1, "planes_type");
		b.u(4, 0, "reserved");
	}

	if (global_configuration.temporal_step_width_modifier != 48)
		b.u(8, global_configuration.temporal_step_width_modifier, "temporal_step_width_modifier");

	if (global_configuration.upsample == Upsample_AdaptiveCubic) {
		b.u(16, global_configuration.upsampling_coefficients[0], "upsampling_coefficients[0]");
		b.u(16, global_configuration.upsampling_coefficients[1], "upsampling_coefficients[1]");
		b.u(16, global_configuration.upsampling_coefficients[2], "upsampling_coefficients[2]");
		b.u(16, global_configuration.upsampling_coefficients[3], "upsampling_coefficients[3]");
	}

	if (global_configuration.level_1_filtering_first_coefficient != 0 ||
	    global_configuration.level_1_filtering_second_coefficient != 0) {
		b.u(4, global_configuration.level_1_filtering_first_coefficient, "level_1_filtering_first_coefficient");
		b.u(4, global_configuration.level_1_filtering_second_coefficient, "level_1_filtering_second_coefficient");
	}

	if (global_configuration.tile_dimensions_type != TileDimensions_None) {
		switch (global_configuration.tile_dimensions_type) {
		case TileDimensions_512x256:
			CHECK(global_configuration.tile_width == 512);
			CHECK(global_configuration.tile_height == 256);
			break;
		case TileDimensions_1024x512:
			CHECK(global_configuration.tile_width == 1024);
			CHECK(global_configuration.tile_height == 512);
			break;
		case TileDimensions_Custom:
			b.u(16, global_configuration.tile_width, "custom_tile_width");
			b.u(16, global_configuration.tile_height, "custom_tile_height");
			break;
		default:
			CHECK(0);
		}
		b.u(5, 0, "reserved");
		b.u(1, global_configuration.compression_type_entropy_enabled_per_tile, "compression_type_entropy_enabled_per_tile");
		switch (global_configuration.compression_type_size_per_tile) {
		case CompressionType_None:
			b.u(2, 0, "compression_type_size_per_tile");
			break;
		case CompressionType_Prefix:
			b.u(2, 1, "compression_type_size_per_tile");
			break;
		case CompressionType_Prefix_OnDiff:
			b.u(2, 2, "compression_type_size_per_tile");
			break;
		default:
			CHECK(0);
		}
	}

	if (resolution_type == 63) {
		b.u(16, global_configuration.resolution_width, "resolution_width");
		b.u(16, global_configuration.resolution_height, "resolution_height");
	}

	if (global_configuration.chroma_step_width_multiplier != 64)
		b.u(8, global_configuration.chroma_step_width_multiplier, "chroma_step_width_multiplier");

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ global_config <<<< \n");
	fflush(goBits);
#endif
}

void Serializer::emit_picture_configuration(const PictureConfiguration &picture_configuration, BitstreamPacker &b,
                                            unsigned num_layers) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ picture_config >>>> \n");
	fflush(goBits);
#endif

	BitstreamPacker::ScopedContextLabel label(b, "picture_config");

	b.u(1, !picture_configuration.enhancement_enabled, "no_enhancement_bit");

	if (picture_configuration.enhancement_enabled) {
		switch (picture_configuration.quant_matrix_mode) {
		case QuantMatrix_BothPrevious:
			b.u(3, 0, "quant_matrix_mode");
			break;
		case QuantMatrix_BothDefault:
			b.u(3, 1, "quant_matrix_mode");
			break;
		case QuantMatrix_SameAndCustom:
			b.u(3, 2, "quant_matrix_mode");
			break;
		case QuantMatrix_Level2CustomLevel1Default:
			b.u(3, 3, "quant_matrix_mode");
			break;
		case QuantMatrix_Level2DefaultLevel1Custom:
			b.u(3, 4, "quant_matrix_mode");
			break;
		case QuantMatrix_DifferentAndCustom:
			b.u(3, 5, "quant_matrix_mode");
			break;
		default:
			CHECK(0);
		}
		b.u(1, picture_configuration.dequant_offset_signalled, "dequant_offset_signalled");
		switch (picture_configuration.picture_type) {
		case PictureType_Frame:
			b.u(1, 0, "picture_type");
			break;
		case PictureType_Field:
			b.u(1, 1, "picture_type");
			break;
		default:
			CHECK(0);
		}
		b.u(1, picture_configuration.temporal_refresh, "temporal_refresh");
		if (picture_configuration.step_width_loq[LOQ_LEVEL_1] == MAX_STEP_WIDTH)
			b.u(1, 0, "step_width_level1_enabled");
		else
			b.u(1, 1, "step_width_level1_enabled");
		b.u(15, picture_configuration.step_width_loq[LOQ_LEVEL_2], "step_width_level2");
		b.u(1, picture_configuration.dithering_control, "dithering_control");
	} else {
		b.u(4, 0, "reserved");
		switch (picture_configuration.picture_type) {
		case PictureType_Frame:
			b.u(1, 0, "picture_type");
			break;
		case PictureType_Field:
			b.u(1, 1, "picture_type");
			break;
		default:
			CHECK(0);
		}
		b.u(1, picture_configuration.temporal_refresh, "temporal_refresh");
		b.u(1, picture_configuration.temporal_signalling_present, "temporal_signalling_present");
	}

	if (picture_configuration.picture_type == PictureType_Field) {
		switch (picture_configuration.field_type) {
		case FieldType_Top:
			b.u(1, 0, "field_type");
			break;
		case FieldType_Bottom:
			b.u(1, 1, "field_type");
			break;
		default:
			CHECK(0);
		}
		b.u(7, 0, "reserved");
	}

	if (picture_configuration.step_width_loq[LOQ_LEVEL_1] != MAX_STEP_WIDTH) {
		b.u(15, picture_configuration.step_width_loq[LOQ_LEVEL_1], "step_width_level1");
		b.u(1, picture_configuration.level_1_filtering_enabled, "level_1_filtering_enabled");
	}

	if (picture_configuration.quant_matrix_mode == QuantMatrix_SameAndCustom ||
	    picture_configuration.quant_matrix_mode == QuantMatrix_Level2CustomLevel1Default ||
	    picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
		for (unsigned layerIndex = 0; layerIndex < num_layers; layerIndex++)
			b.u(8, picture_configuration.qm_coefficient_2[layerIndex], "qm_coefficient_0");
	}
	if (picture_configuration.quant_matrix_mode == QuantMatrix_Level2DefaultLevel1Custom ||
	    picture_configuration.quant_matrix_mode == QuantMatrix_DifferentAndCustom) {
		for (unsigned layerIndex = 0; layerIndex < num_layers; layerIndex++)
			b.u(8, picture_configuration.qm_coefficient_1[layerIndex], "qm_coefficient_1");
	}

	if (picture_configuration.dequant_offset_signalled) {
		switch (picture_configuration.dequant_offset_mode) {
		case DequantOffset_Default:
			b.u(1, 0, "dequant_offset_mode");
			break;
		case DequantOffset_ConstOffset:
			b.u(1, 1, "dequant_offset_mode");
			break;
		default:
			CHECK(0);
		}
		b.u(7, picture_configuration.dequant_offset, "dequant_offset");
	}

	if (picture_configuration.dithering_control) {
		switch (picture_configuration.dithering_type) {
		case Dithering_None:
			b.u(2, 0, "dithering_type");
			break;
		case Dithering_Uniform:
		case Dithering_UniformFixed:
			b.u(2, 1, "dithering_type");
			break;
		default:
			CHECK(0);
		}
		b.u(1, 0, "reserved");
		if (picture_configuration.dithering_type != Dithering_None)
			b.u(5, picture_configuration.dithering_strength, "dithering_strength");
		else
			b.u(5, 0, "reserved");
	}
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ picture_config <<<< \n");
	fflush(goBits);
#endif
}

// Return whether temporal layer is enabled
//
static unsigned temporal_layer_(const SignaledConfiguration &signaled_configuration, unsigned loq) {
	if (loq == LOQ_LEVEL_2 && signaled_configuration.picture_configuration.temporal_signalling_present)
		return 1;
	else
		return 0;
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
		return configuration.global_configuration.num_residual_layers;
}

static bool is_temporal_layer(const SignaledConfiguration &configuration, unsigned plane, unsigned loq, unsigned layer) {
	return layer == configuration.global_configuration.num_residual_layers;
}

void Serializer::emit_encoded_data(const SignaledConfiguration &signaled_configuration, BitstreamPacker &b,
                                   const Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], unsigned num_layers) {
	CHECK(signaled_configuration.global_configuration.tile_dimensions_type == TileDimensions_None);

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data >>>> \n");
	fflush(goBits);
#endif
	// Entropy code the layers
	bool entropy_enabled[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {false};
	bool rle_only[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {false};
	Packet data[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];

	for (unsigned plane = 0; plane < signaled_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
			for (unsigned layer = first_layer(signaled_configuration); layer < total_layers(signaled_configuration, plane, loq);
			     ++layer) {
				// Entropy code symbols
				EncodedChunk e;
				if (!is_temporal_layer(signaled_configuration, plane, loq, layer)) {
					const bool use_tiled_encoding_order = signaled_configuration.global_configuration.temporal_enabled ||
					                                      signaled_configuration.global_configuration.tile_dimensions_type > 0;
					if (use_tiled_encoding_order) {
						e = EntropyEncoderResidualsTiled().process(
						    symbols[plane][loq][layer], signaled_configuration.global_configuration.transform_block_size);
					} else {
						e = EntropyEncoderResiduals().process(symbols[plane][loq][layer]);
					}

				} else {
					e = EntropyEncoderTemporal().process(
					    symbols[plane][loq][layer], signaled_configuration.global_configuration.transform_block_size,
					    signaled_configuration.global_configuration.temporal_tile_intra_signalling_enabled);
				}
				if (!e.empty()) {
					entropy_enabled[plane][loq][layer] = true;

					// Choose smaller of prefix coded vs. raw
					if (e.prefix.size() > e.raw.size()) {
						rle_only[plane][loq][layer] = true;
						data[plane][loq][layer] = e.raw;
					} else {
						rle_only[plane][loq][layer] = false;
						data[plane][loq][layer] = e.prefix;
					}
				}
			}
		}
	}

	// Emit flags
	BitstreamPacker::ScopedContextLabel label(b, "encoded_data");

	for (unsigned plane = 0; plane < signaled_configuration.global_configuration.num_processed_planes; ++plane) {
		if (signaled_configuration.picture_configuration.enhancement_enabled) {
			for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
				for (unsigned layer = 0; layer < num_layers; ++layer) {
					b.u(1, entropy_enabled[plane][loq][layer], "entropy_enabled");
					b.u(1, rle_only[plane][loq][layer], "rle_only");
				}
			}
		}
		if (temporal_layer_(signaled_configuration, LOQ_LEVEL_2)) {
			b.u(1, entropy_enabled[plane][LOQ_LEVEL_2][num_layers], "entropy_enabled");
			b.u(1, rle_only[plane][LOQ_LEVEL_2][num_layers], "rle_only");
		}
	}

	// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		b.u(1, 0, "alignment");

	// Emit surfaces
	for (unsigned plane = 0; plane < signaled_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {

			const unsigned n = num_layers + temporal_layer_(signaled_configuration, loq);

			for (unsigned layer = 0; layer < n; ++layer) {
				BitstreamPacker::ScopedContextLabel label(b, format("surface[%d][%d][%d]", plane, loq, layer));
				if (entropy_enabled[plane][loq][layer]) {
					write_multibyte(b, data[plane][loq][layer].size(), "data_size");
					b.bytes(data[plane][loq][layer]);
				}
			}
		}
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data <<<< \n");
	fflush(goBits);
#endif
}

void Serializer::emit_encoded_data_tiled(const SignaledConfiguration &signaled_configuration, BitstreamPacker &b,
                                         const Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], unsigned num_layers) {

	CHECK(signaled_configuration.global_configuration.tile_dimensions_type != TileDimensions_None);

	Dimensions dimensions;
	dimensions.set(signaled_configuration,
				   signaled_configuration.global_configuration.resolution_width,
	               signaled_configuration.global_configuration.resolution_height);

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data_tiled >>>> \n");
	fflush(goBits);
#endif

	// Accumulated flags and data
	std::vector<bool> rle_only;        // Per layer
	std::vector<bool> entropy_enabled; // Per tile
	std::vector<Packet> chunks;        // Per tile

	unsigned num_tiles[MAX_NUM_PLANES][MAX_NUM_LOQS] = {0};

	// Entropy code the tiles in each layer
	for (unsigned plane = 0; plane < signaled_configuration.global_configuration.num_processed_planes; ++plane) {
		for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {

			// All tile dimensions are in terms of transform blocks
			const unsigned tile_width = dimensions.tile_width(plane, loq);
			const unsigned tile_height = dimensions.tile_height(plane, loq);

			// Figure out tile breakdown for this level
			const unsigned width = symbols[plane][loq][0].width();
			const unsigned height = symbols[plane][loq][0].height();

			const unsigned tiles_x = (width + tile_width - 1) / tile_width;
			const unsigned tiles_y = (height + tile_height - 1) / tile_height;

			num_tiles[plane][loq] = tiles_x * tiles_y;

			for (unsigned layer = first_layer(signaled_configuration); layer < total_layers(signaled_configuration, plane, loq);
			     ++layer) {

				std::vector<EncodedChunk> encoded_chunks;
				unsigned layer_raw_sizes = 0;
				unsigned layer_prefix_sizes = 0;

				// For each tile ...
				for (unsigned ty = 0; ty < tiles_y; ++ty) {
					for (unsigned tx = 0; tx < tiles_x; ++tx) {
						// Extract tile
						const unsigned tx0 = tx * tile_width;
						const unsigned ty0 = ty * tile_height;
						const unsigned tx1 = std::min((tx + 1) * tile_width, width);
						const unsigned ty1 = std::min((ty + 1) * tile_height, height);

						// Entropy code (raw & prefix)
						if (!is_temporal_layer(signaled_configuration, plane, loq, layer)) {
							Surface tile_symbols = CropResiduals().process(symbols[plane][loq][layer], tx0, ty0, tx1, ty1);
							encoded_chunks.push_back(EntropyEncoderResidualsTiled().process(
							    tile_symbols, signaled_configuration.global_configuration.transform_block_size));
						} else {
							Surface tile_symbols = CropTemporal().process(symbols[plane][loq][layer], tx0, ty0, tx1, ty1);
							encoded_chunks.push_back(EntropyEncoderTemporal().process(
							    tile_symbols, signaled_configuration.global_configuration.transform_block_size,
							    signaled_configuration.global_configuration.temporal_tile_intra_signalling_enabled));
						}
						// Accumalate raw/prefix sizes for each layer
						layer_raw_sizes += encoded_chunks.back().raw.size();
						layer_prefix_sizes += encoded_chunks.back().prefix.size();

						// Accumalate entropy_enabled for each tile
						CHECK((encoded_chunks.back().raw.size() == 0) == (encoded_chunks.back().prefix.size() == 0));
						entropy_enabled.push_back(encoded_chunks.back().raw.size() > 0);
					}
				}
				// Decide rle_only for layer based on accumulated sizes
				const bool r = layer_raw_sizes < layer_prefix_sizes;
				rle_only.push_back(r);
				// Accumulate chosen packets
				for (auto ec : encoded_chunks)
					chunks.push_back(r ? ec.raw : ec.prefix);
			}
		}
	}

	CHECK(chunks.size() == entropy_enabled.size());

	BitstreamPacker::ScopedContextLabel label(b, "encoded_data_tiled");

	for (auto r : rle_only)
		b.u(1, r, "rle_only");

		// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		b.u(1, 0, "alignment");

	// Emit per tile entropy_enabled
	if (!signaled_configuration.global_configuration.compression_type_entropy_enabled_per_tile) {
		// Don't compress entropy_enabled flags
		for (auto r : entropy_enabled)
			b.u(1, r, "entropy_enabled");
	} else {
		// Prefix code the entropy_enabled flags
		const Surface ee = Surface::build_from<uint8_t>()
		                       .generate(static_cast<unsigned>(entropy_enabled.size()), 1,
		                                 [&](unsigned x, unsigned y) -> uint8_t { return entropy_enabled[x]; })
		                       .finish();
		b.bytes(EntropyEncoderFlags().process(ee).raw);
	}

	// Byte alignment
#if defined __OPT_MODULO__
	while ((b.bit_offset() & 0x07))
#else
	while ((b.bit_offset() % 8 != 0))
#endif
		b.u(1, 0, "alignment");

	if (signaled_configuration.global_configuration.compression_type_size_per_tile == CompressionType_None) {
		// Don't compress tile sizes
		for (unsigned c = 0; c < chunks.size(); ++c) {
			if (entropy_enabled[c]) {
				CHECK(chunks[c].size() > 0);
				write_multibyte(b, chunks[c].size(), "data_size");
				b.bytes(chunks[c]);
			}
		}
	} else {
		// Prefix code each layer's tile sizes
		unsigned idx = 0;
		for (unsigned plane = 0; plane < signaled_configuration.global_configuration.num_processed_planes; ++plane) {
			for (unsigned loq = 0; loq < MAX_NUM_LOQS; ++loq) {
				for (unsigned layer = first_layer(signaled_configuration); layer < total_layers(signaled_configuration, plane, loq);
				     ++layer) {

					// Does any tile contain data?
					bool any_tile_enabled = false;
					for (unsigned x = 0; x < num_tiles[plane][loq]; x++) {
						if (entropy_enabled[idx + x]) {
							any_tile_enabled = true;
							break;
						}
					}

					if (any_tile_enabled) {
						// Collect tile sizes for this layer - optionally converting to deltas
						auto sb = Surface::build_from<uint16_t>();
						sb.generate(num_tiles[plane][loq], 1,
						            [&](unsigned x, unsigned y) -> int16_t { return chunks[idx + x].size(); });
						const Surface sz = sb.finish();

						// Prefix code and emit them
						b.bytes(EntropyEncoderSizes()
						            .process(sz, entropy_enabled, idx,
						                     signaled_configuration.global_configuration.compression_type_size_per_tile)
						            .prefix);
						// Emit tiles
						for (unsigned t = 0; t < num_tiles[plane][loq]; ++t)
							if (chunks[idx + t].size() > 0)
								b.bytes(chunks[idx + t]);
					}

					idx += num_tiles[plane][loq];
				}
			}
		}
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ encoded_data_tiled >>>> \n");
	fflush(goBits);
#endif
}

void Serializer::emit_additional_info(const AdditionalInfo &additional_info, BitstreamPacker &b) {
#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ additional_info >>>> \n");
	fflush(goBits);
#endif

	BitstreamPacker::ScopedContextLabel label(b, "additional_info");

	b.u(8, additional_info.additional_info_type, "additional_info_type");
	if (additional_info.additional_info_type == 0) {
		b.u(8, additional_info.payload_type, "payload_type");
		INFO("SeiPayload");
	} else if (additional_info.additional_info_type == 1) {
		INFO("VuiParameters");
	} else {
		// (additional_info.additional_info_type >= 2)
		INFO("additional_info_type MUST be 0 or 1 - type = %4d", additional_info.additional_info_type);
		CHECK(0);
	}

#if BITSTREAM_DEBUG
	fprintf(goBits, "@@@@ @@@@ additional_info <<<< \n");
	fflush(goBits);
#endif
}

/***
void Serializer::emit_additional_info(BitstreamPacker &b) {
	BitstreamPacker::ScopedContextLabel label(b, "additional_info");

	// TODO
	return;
}
***/

void Serializer::emit_filler(BitstreamPacker &b, unsigned size) {
	BitstreamPacker::ScopedContextLabel label(b, "filler");

	while (size--)
		b.u(8, 0xaa, "filler");
}
} // namespace lctm
