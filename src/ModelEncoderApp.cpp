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
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//
// ModelEncoderApp.cpp
//
//
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <string>

#include <cxxopts.hpp>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "FileEncoder.hpp"

#include "Config.hpp"
#include "Diagnostics.hpp"
#include "Image.hpp"
#include "Misc.hpp"
#include "YUVReader.hpp"

#if defined(__linux__)
#include "git_version.h"
#endif

#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

#include "SignaledConfiguration.hpp"

#include <queue>
#include <vector>

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
BitstreamStatistic goStat;
FILE *goBits = NULL;
PsnrStatistic goPsnr;
uint8_t gaucMd5Digest[lctm::MAX_NUM_PLANES][16];
ReportStructure goReportStructure;
std::priority_queue<ReportStructure, std::vector<ReportStructure, std::allocator<ReportStructure>>, ReportStructureComp>
    goReportQueue;

using namespace lctm;

int main(int argc, char *argv[]) {

	auto pb = Parameters::build();

	try {
		//// Command line
		//
		cxxopts::Options options_description(argv[0], "LCEVC Encoder " GIT_VERSION);

		// clang-format off
		options_description.add_options()
			("i,input_file", "Input filename for raw YUV video frames", cxxopts::value<string>()->default_value("source.yuv"))
			("o,output_file", "Output filename for elementary stream", cxxopts::value<string>()->default_value("output.lvc"))
			("w,width", "Image width", cxxopts::value<unsigned>()->default_value("1920"))
			("h,height", "Image height", cxxopts::value<unsigned>()->default_value("1080"))
			("r,fps", "Frame rate", cxxopts::value<unsigned>()->default_value("50"))
			("l,limit", "Limit number of frames to encode", cxxopts::value<unsigned>())
			("f,format", "Picture format (yuv420p, yuv420p10, yuv420p12, yuv420p14, yuv422p, yuv422p10, yuv422p12, yuv422p14, yuv444p, yuv444p10, yuv444p12, yuv444p14, y, y10, y12, or y14)", cxxopts::value<string>()->default_value("yuv420p"))
			("p,parameters", "JSON parameters for encoder (path to .json file)", cxxopts::value<string>()->default_value("{}"))
			("parameter_config", "Configuration with default parameter values to use (default or conformance)", cxxopts::value<string>()->default_value("default"))
			("dump_surfaces", "Dump intermediate surfaces to yuv files", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("downsample_only", "Downsample input and write to output.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("upsample_only", "Upsample input and write to output.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("output_recon", "Output filename for encoder yuv reconstruction (must be specified for output)", cxxopts::value<string>())
			("encapsulation", "Code enhancement as SEI or NAL", cxxopts::value<string>()->default_value("nal"))

			("additional_info_present", "Additional Info present.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("additional_info_type", "Additional Info type.", cxxopts::value<unsigned>()->default_value("0")->implicit_value("0"))
			("sei_message_present", "SEI Message present.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("vui_message_present", "VUI Message present.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))

			("version", "Show version")
			("help", "Show this help");

		options_description.add_options("Base Encoder Configuration")
			("b,base_encoder", "Base encoder plugin to use (avc, hevc, vvc, or evc)", cxxopts::value<string>()->default_value("avc"))
			("qp", "QP value to be used by the specified base encoder", cxxopts::value<unsigned>()->default_value("28"))
			("base", "Encoded base bitstream if base encoder shall be skipped", cxxopts::value<string>())
			("base_recon", "Decoded YUV for base bitstream", cxxopts::value<string>())
			("keep_base", "Keep the encoded base bitstream and reconstruction", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("intra_period", "Intra Period for base encoding (default: derived from framerate)", cxxopts::value<unsigned>())
			("base_depth", "Bit depth of base encoder", cxxopts::value<unsigned>());

		options_description.add_options("Sequence Configuration")
			("profile_idc", "Profile (see Annex A.3) (auto, main, or main444)", cxxopts::value<string>()->default_value("auto"))
			("level_idc", "Level (see Annex A.4) (0: choose automatically)", cxxopts::value<unsigned>()->default_value("0"))
			("sublevel_idc", "Sublevel (see Annex A.4)", cxxopts::value<unsigned>()->default_value("1"))
			("conformance_window", "Turn signalling of conformance cropping window offset parameters on", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("extended_profile_idc", "Extended profile (see Annex A)", cxxopts::value<unsigned>()->default_value("0"))
			("extended_level_idc", "Extended level (see Annex A)", cxxopts::value<unsigned>()->default_value("0"))
			("conf_win_left_offset", "Left offset of the conformance window", cxxopts::value<unsigned>()->default_value("0"))
			("conf_win_right_offset", "Right offset of the conformance window", cxxopts::value<unsigned>()->default_value("0"))
			("conf_win_top_offset", "Top offset of the conformance window", cxxopts::value<unsigned>()->default_value("0"))
			("conf_win_bottom_offset", "Bottom offset of the conformance window", cxxopts::value<unsigned>()->default_value("0"));

		options_description.add_options("Global Configuration")
			("num_image_planes", "Number of planes in input sequence", cxxopts::value<unsigned>()->default_value("3"))
			("num_processed_planes", "Number of planes for which enhancement shall be added (1 or 3)", cxxopts::value<unsigned>()->default_value("1"))
			("predicted_residual", "Predicted average after the upscaling", cxxopts::value<bool>()->default_value("true"))
			("encoding_transform_type", "Transform type (dd or dds)", cxxopts::value<string>()->default_value("dds"))
			("temporal_enabled", "Temporal prediction for enhancement sub-layer 2", cxxopts::value<bool>()->default_value("true"))
			("temporal_use_reduced_signalling", "Reduced signalling (tile based) for temporal", cxxopts::value<bool>()->default_value("true"))
			("encoding_upsample", "Upsample filter", cxxopts::value<string>()->default_value("modifiedcubic"))
			("temporal_step_width_modifier", "Temporal step width modifier", cxxopts::value<unsigned>()->default_value("48"))
			("chroma_step_width_multiplier", "Chroma step width multiplier", cxxopts::value<unsigned>()->default_value("64"))
			("level_1_filtering_first_coefficient", "L-1 filter 1st coefficient", cxxopts::value<unsigned>()->default_value("0"))
			("level_1_filtering_second_coefficient", "L-1 filter 2nd coefficient", cxxopts::value<unsigned>()->default_value("0"))
			("scaling_mode_level1", "Scaling mode between encoded base and preliminary intermediate picture (none, 1d or 2d)", cxxopts::value<string>()->default_value("none"))
			("scaling_mode_level2", "Scaling mode between combined intermediate picture and preliminary output picture (none, 1d or 2d)", cxxopts::value<string>()->default_value("2d"))
			("user_data_enabled", "User data in enhancement sub-layer 1 (none, 2bits or 6bits)", cxxopts::value<string>()->default_value("none"))
			("level1_depth_flag", "Flag for bit depth of enhancement sub-layer 1", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("tile_width", "Width of a tile (disable tiling with 0)", cxxopts::value<unsigned>()->default_value("0"))
			("tile_height", "Height of a tile (disable tiling with 0)", cxxopts::value<unsigned>()->default_value("0"))
			("compression_type_entropy_enabled_per_tile", "Use compression to signal entropy_enabled when tiling is used", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("compression_type_size_per_tile", "Compression type per tile (none, prefix or prefix_diff)", cxxopts::value<string>()->default_value("none"))
			("upsampling_coefficients", "Custom upsampling coefficents", cxxopts::value<string>());

		options_description.add_options("Picture Configuration")
			("cq_step_width_loq_1", "Step width for enhancement sub-layer 1 (range: [1, 32767])", cxxopts::value<unsigned>()->default_value("32767"))
			("cq_step_width_loq_0", "Step width for enhancement sub-layer 2 (range: [1, 32767])", cxxopts::value<unsigned>()->default_value("32767"))
			("temporal_signalling_present", "Signal temporal layer although no_enhacement is enabled", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("picture_type", "Picture type (frame or field)", cxxopts::value<string>()->default_value("frame"))
			("dithering_control", "Dithering", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("dithering_type", "Dithering type (none, uniform or uniform_fixed)", cxxopts::value<string>()->default_value("none"))
			("dithering_strength", "Strength of the dithering", cxxopts::value<unsigned>()->default_value("0"))
			("dequant_offset_mode", "Dequantization offset mode (default or const_offset)", cxxopts::value<string>()->default_value("default"))
			("dequant_offset", "Offset of the dequantization", cxxopts::value<unsigned>()->default_value("0"))
			("quant_matrix_mode", "Quantization mode (previous, default, custom, custom_default, default_custom or custom_custom)", cxxopts::value<string>()->default_value("previous"))
			("qm_coefficient_1", "Custom quantization coefficents for sub-layer 1", cxxopts::value<string>())
			("qm_coefficient_2", "Custom quantization coefficents for sub-layer 2", cxxopts::value<string>())
			("level_1_filtering_enabled", "Enable L-1 filter (deblocking filter)", cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

		options_description.add_options("Encoder only Configuration")
			("encoding_downsample_luma", "Downsample filter for y plane", cxxopts::value<string>()->default_value("lanczos3"))
			("encoding_downsample_chroma", "Downsample filter for u and v planes", cxxopts::value<string>()->default_value("lanczos3"))
			("temporal_cq_sw_multiplier", "Multiplier to reduce the enhancement sub-layer 2 stepwidth of an IDR (range: [200, 1000])", cxxopts::value<unsigned>()->default_value("1000"))
			("delta_sw_mult_gop08", "Multiplier to increase the enhancement sub-layer 2 stepwidth of a picture at position 8 (range: [1000, 2000])", cxxopts::value<unsigned>()->default_value("1000"))
			("delta_sw_mult_gop04", "Multiplier to increase the enhancement sub-layer 2 stepwidth of a picture at position 4 (range: [1000, 2000])", cxxopts::value<unsigned>()->default_value("1000"))
			("delta_sw_mult_gop02", "Multiplier to increase the enhancement sub-layer 2 stepwidth of a picture at position 2 (range: [1000, 2000])", cxxopts::value<unsigned>()->default_value("1000"))
			("delta_sw_mult_gop01", "Multiplier to increase the enhancement sub-layer 2 stepwidth of a picture at position 1 (range: [1000, 2000])", cxxopts::value<unsigned>()->default_value("1000"))
			("priority_mode", "Priority map running mode", cxxopts::value<string>()->default_value("mode_3_1"))
			("priority_type_sl_1", "Priority block type SL-1", cxxopts::value<string>()->default_value("type_4"))
			("priority_type_sl_2", "Priority block type SL-2", cxxopts::value<string>()->default_value("type_5"))
			("sad_threshold", "Threshold of SAD decision for removing non-static residuals (off: 0)", cxxopts::value<unsigned>()->default_value("0"))
			("sad_coeff_threshold", "Threshold of coefficients for removing non-static residuals (off: 0)", cxxopts::value<unsigned>()->default_value("0"))
			("quant_reduced_deadzone", "Multiplier to reduce the quantization deadzone (range: [1, 5]) (off: 5)", cxxopts::value<unsigned>()->default_value("5"))
			("user_data_method", "Type of user data to be inserted (zeros, ones, random or fixed_random)", cxxopts::value<string>()->default_value("zeros"))
			("dump_configuration", "Output JSON encoded contents of config blocks that are written enhancement stream.", cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

		// clang-format on

		const bool show_help = argc == 1;

		auto options = options_description.parse(argc, argv);

		// Help
		if (show_help || options.count("help")) {
			std::cout << options_description.help({"", "Base Encoder Configuration", "Sequence Configuration",
			                                       "Global Configuration", "Picture Configuration", "Encoder only Configuration"})
			          << std::endl;
			exit(0);
		}

		// Version
		if (options.count("version")) {
			INFO(GIT_VERSION);
			exit(0);
		}

		// Print version
		INFO("LTM [%s] " GIT_VERSION, get_program().c_str());

		//// Parameters

		// Start with encoder parameters from command line or file
		const string s = options["parameters"].as<string>();
		if (!s.empty() && s[0] != '{') {
			// String is filename - read and parse
			const string f = lctm::read_file(s);
			if (f.empty())
				ERR("Cannot read parameters from \"%s\"\n", s.c_str());
			pb.set_json(f);
		} else {
			// String is json
			pb.set_json(s);
		}

		// Override any settings from other arguments
		//
		if (options.count("input_file"))
			pb.set("input_file", options["input_file"].as<std::string>());
		if (options.count("output_file"))
			pb.set("output_file", options["output_file"].as<std::string>());
		if (options.count("width"))
			pb.set("width", options["width"].as<unsigned>());
		if (options.count("height"))
			pb.set("height", options["height"].as<unsigned>());
		if (options.count("fps"))
			pb.set("fps", options["fps"].as<unsigned>());
		if (options.count("limit"))
			pb.set("limit", options["limit"].as<unsigned>());
		if (options.count("format"))
			pb.set("format", options["format"].as<std::string>());
		if (options.count("parameter_config"))
			pb.set("parameter_config", options["parameter_config"].as<string>());
		if (options.count("dump_surfaces"))
			pb.set("dump_surfaces", options["dump_surfaces"].as<bool>());
		if (options.count("output_recon"))
			pb.set("output_recon", options["output_recon"].as<std::string>());
		if (options.count("encapsulation"))
			pb.set("encapsulation", options["encapsulation"].as<string>());
		if (options.count("downsample_only"))
			pb.set("downsample_only", options["downsample_only"].as<bool>());
		if (options.count("upsample_only"))
			pb.set("upsample_only", options["upsample_only"].as<bool>());

		// Base Encoder Configuration
		if (options.count("base_encoder"))
			pb.set("base_encoder", options["base_encoder"].as<std::string>());
		if (options.count("qp"))
			pb.set("qp", options["qp"].as<unsigned>());
		if (options.count("base"))
			pb.set("base", options["base"].as<std::string>());
		if (options.count("base_recon"))
			pb.set("base_recon", options["base_recon"].as<std::string>());
		if (options.count("keep_base"))
			pb.set("keep_base", options["keep_base"].as<bool>());
		if (options.count("intra_period"))
			pb.set("intra_period", options["intra_period"].as<unsigned>());
		if (options.count("base_depth"))
			pb.set("base_depth", options["base_depth"].as<unsigned>());

		// Sequence Configuration
		if (options.count("profile_idc"))
			pb.set("profile_idc", options["profile_idc"].as<string>());
		if (options.count("level_idc"))
			pb.set("level_idc", options["level_idc"].as<unsigned>());
		if (options.count("sublevel_idc"))
			pb.set("sublevel_idc", options["sublevel_idc"].as<unsigned>());
		if (options.count("conformance_window"))
			pb.set("conformance_window", options["conformance_window"].as<bool>());
		if (options.count("extended_profile_idc"))
			pb.set("extended_profile_idc", options["extended_profile_idc"].as<unsigned>());
		if (options.count("extended_level_idc"))
			pb.set("extended_level_idc", options["extended_level_idc"].as<unsigned>());
		if (options.count("conf_win_left_offset"))
			pb.set("conf_win_left_offset", options["conf_win_left_offset"].as<unsigned>());
		if (options.count("conf_win_right_offset"))
			pb.set("conf_win_right_offset", options["conf_win_right_offset"].as<unsigned>());
		if (options.count("conf_win_top_offset"))
			pb.set("conf_win_top_offset", options["conf_win_top_offset"].as<unsigned>());
		if (options.count("conf_win_bottom_offset"))
			pb.set("conf_win_bottom_offset", options["conf_win_bottom_offset"].as<unsigned>());

		// Global Config
		if (options.count("num_image_planes"))
			pb.set("num_image_planes", options["num_image_planes"].as<unsigned>());
		if (options.count("num_processed_planes"))
			pb.set("num_processed_planes", options["num_processed_planes"].as<unsigned>());
		if (options.count("predicted_residual"))
			pb.set("predicted_residual", options["predicted_residual"].as<bool>());
		if (options.count("encoding_transform_type"))
			pb.set("encoding_transform_type", options["encoding_transform_type"].as<string>());
		if (options.count("temporal_enabled"))
			pb.set("temporal_enabled", options["temporal_enabled"].as<bool>());
		if (options.count("temporal_use_reduced_signalling"))
			pb.set("temporal_use_reduced_signalling", options["temporal_use_reduced_signalling"].as<bool>());
		if (options.count("encoding_upsample"))
			pb.set("encoding_upsample", options["encoding_upsample"].as<string>());
		if (options.count("temporal_step_width_modifier"))
			pb.set("temporal_step_width_modifier", options["temporal_step_width_modifier"].as<unsigned>());
		if (options.count("level_1_filtering_first_coefficient"))
			pb.set("level_1_filtering_first_coefficient", options["level_1_filtering_first_coefficient"].as<unsigned>());
		if (options.count("level_1_filtering_second_coefficient"))
			pb.set("level_1_filtering_second_coefficient", options["level_1_filtering_second_coefficient"].as<unsigned>());
		if (options.count("scaling_mode_level1"))
			pb.set("scaling_mode_level1", options["scaling_mode_level1"].as<string>());
		if (options.count("scaling_mode_level2"))
			pb.set("scaling_mode_level2", options["scaling_mode_level2"].as<string>());
		if (options.count("user_data_enabled"))
			pb.set("user_data_enabled", options["user_data_enabled"].as<string>());
		if (options.count("level1_depth_flag"))
			pb.set("level1_depth_flag", options["level1_depth_flag"].as<bool>());
		if (options.count("tile_width"))
			pb.set("tile_width", options["tile_width"].as<unsigned>());
		if (options.count("tile_height"))
			pb.set("tile_height", options["tile_height"].as<unsigned>());
		if (options.count("compression_type_entropy_enabled_per_tile"))
			pb.set("compression_type_entropy_enabled_per_tile", options["compression_type_entropy_enabled_per_tile"].as<bool>());
		if (options.count("compression_type_size_per_tile"))
			pb.set("compression_type_size_per_tile", options["compression_type_size_per_tile"].as<string>());
		if (options.count("upsampling_coefficients"))
			pb.set("upsampling_coefficients", options["upsampling_coefficients"].as<string>());
		if (options.count("chroma_step_width_modifier"))
			pb.set("chroma_step_width_modifier", options["chroma_step_width_modifier"].as<unsigned>());

		// Picture Configuration
		if (options.count("cq_step_width_loq_1"))
			pb.set("cq_step_width_loq_1", options["cq_step_width_loq_1"].as<unsigned>());
		if (options.count("cq_step_width_loq_0"))
			pb.set("cq_step_width_loq_0", options["cq_step_width_loq_0"].as<unsigned>());
		if (options.count("temporal_signalling_present"))
			pb.set("temporal_signalling_present", options["temporal_signalling_present"].as<bool>());
		if (options.count("picture_type"))
			pb.set("picture_type", options["picture_type"].as<string>());
		if (options.count("dithering_control"))
			pb.set("dithering_control", options["dithering_control"].as<bool>());
		if (options.count("dithering_type"))
			pb.set("dithering_type", options["dithering_type"].as<string>());
		if (options.count("dithering_strength"))
			pb.set("dithering_strength", options["dithering_strength"].as<unsigned>());
		if (options.count("dequant_offset_mode"))
			pb.set("dequant_offset_mode", options["dequant_offset_mode"].as<string>());
		if (options.count("dequant_offset"))
			pb.set("dequant_offset", options["dequant_offset"].as<unsigned>());
		if (options.count("quant_matrix_mode"))
			pb.set("quant_matrix_mode", options["quant_matrix_mode"].as<string>());
		if (options.count("qm_coefficient_1"))
			pb.set("qm_coefficient_1", options["qm_coefficient_1"].as<string>());
		if (options.count("qm_coefficient_2"))
			pb.set("qm_coefficient_2", options["qm_coefficient_2"].as<string>());
		if (options.count("level_1_filtering_enabled"))
			pb.set("level_1_filtering_enabled", options["level_1_filtering_enabled"].as<bool>());

		// Encoder only Configuration
		if (options.count("encoding_downsample_luma"))
			pb.set("encoding_downsample_luma", options["encoding_downsample_luma"].as<string>());
		if (options.count("encoding_downsample_chroma"))
			pb.set("encoding_downsample_chroma", options["encoding_downsample_chroma"].as<string>());
		if (options.count("temporal_cq_sw_multiplier"))
			pb.set("temporal_cq_sw_multiplier", options["temporal_cq_sw_multiplier"].as<unsigned>());
		if (options.count("delta_sw_mult_gop08"))
			pb.set("delta_sw_mult_gop08", options["delta_sw_mult_gop08"].as<unsigned>());
		if (options.count("delta_sw_mult_gop04"))
			pb.set("delta_sw_mult_gop04", options["delta_sw_mult_gop04"].as<unsigned>());
		if (options.count("delta_sw_mult_gop02"))
			pb.set("delta_sw_mult_gop02", options["delta_sw_mult_gop02"].as<unsigned>());
		if (options.count("delta_sw_mult_gop01"))
			pb.set("delta_sw_mult_gop01", options["delta_sw_mult_gop01"].as<unsigned>());
		if (options.count("user_data_method"))
			pb.set("user_data_method", options["user_data_method"].as<string>());
		if (options.count("priority_mode"))
			pb.set("priority_mode", options["priority_mode"].as<string>());
		if (options.count("priority_type_sl_1"))
			pb.set("priority_type_sl_1", options["priority_type_sl_1"].as<string>());
		if (options.count("priority_type_sl_2"))
			pb.set("priority_type_sl_2", options["priority_type_sl_2"].as<string>());
		if (options.count("sad_threshold"))
			pb.set("sad_threshold", options["sad_threshold"].as<unsigned>());
		if (options.count("sad_coeff_threshold"))
			pb.set("sad_coeff_threshold", options["sad_coeff_threshold"].as<unsigned>());
		if (options.count("quant_reduced_deadzone"))
			pb.set("quant_reduced_deadzone", options["quant_reduced_deadzone"].as<unsigned>());

	} catch (const cxxopts::OptionException &e) {
		std::cout << "error parsing options: " << e.what() << std::endl;
		exit(1);
	}
	auto parameters = pb.finish();

	// After parsing JSON
	Surface::set_dump_surfaces(parameters["dump_surfaces"].get<bool>(false));

	// Figure source image description
	//
	const ImageFormat format = parameters["format"].get_enum<ImageFormat>(IMAGE_FORMAT_YUV420P8);

	const unsigned width = parameters["width"].get<unsigned>(1920);
	const unsigned height = parameters["height"].get<unsigned>(1080);

	const ImageDescription image_description(format, width, height);

	const unsigned fps = parameters["fps"].get<unsigned>(50);
	const unsigned limit = parameters["limit"].get<unsigned>(INT_MAX);

	const std::string input_file = parameters["input_file"].get<string>("source.yuv");
	const std::string output_recon = parameters["output_recon"].get<string>();
	const std::string output_file = parameters["output_file"].get<string>("output.lvc");

	// Are we just scaling?
	//

	// Base encoder type
	BaseCoding base_encoder_type = parameters["base_encoder"].get_enum<BaseCoding>(BaseCoding_AVC);

	const std::string base_file = parameters["base"].get<string>();
	const std::string base_recon_file = parameters["base_recon"].get<string>();

#if BITSTREAM_DEBUG
	goStat.Reset();
	goBits = fopen("BitstreamEnc.txt", "w");
#endif
	auto file_encoder = CreateFileEncoder(base_encoder_type, image_description, fps, parameters);

	clock_t EnhaClock0;
	EnhaClock0 = clock();
	INFO("**** Enh. start %16d", EnhaClock0);

	if (parameters["downsample_only"].get<bool>())
		// Downsample only
		file_encoder->downsample_file(input_file, output_file, limit);
	else if (parameters["upsample_only"].get<bool>())
		// Upsample only
		file_encoder->upsample_file(input_file, output_file, limit);
	else if (!base_file.empty() && !base_recon_file.empty())
		// Encode with prepared base and recon
		file_encoder->encode_file_with_base(input_file, base_file, base_recon_file, output_file, output_recon, limit);
	else if (!base_file.empty() && base_recon_file.empty())
		// Encode with prepared base
		file_encoder->encode_file_with_decoder(input_file, base_file, output_file, output_recon, limit);
	else
		// Encode
		file_encoder->encode_file(input_file, output_file, output_recon, limit);

	clock_t EnhaClock1;
	EnhaClock1 = clock();
	INFO("**** Enh. stop. %16d", EnhaClock1);
	INFO("@@@@ Enh. delta %16d", EnhaClock1 - EnhaClock0);

#if BITSTREAM_DEBUG
	goStat.Dump();
	fflush(goBits);
	fclose(goBits);
#endif
}
