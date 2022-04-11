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
//
// Model Decoder
//

#include <cstdio>

#include <cxxopts.hpp>

#include "BaseVideoDecoder.hpp"
#include "Config.hpp"
#include "Decoder.hpp"
#include "Diagnostics.hpp"
#include "Expand.hpp"
#include "Surface.hpp"
#include "YUVReader.hpp"
#include "YUVWriter.hpp"

#include "uESFile.h"

#include <time.h>

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
using namespace vnova::utility;

class DecoderApp : public BaseVideoDecoder::Output {
public:
	DecoderApp(YUVWriter &writer, YUVReader &reader);

	// implement BaseVideoDecoder::Output
	void push_base_enhancement(const BaseVideoDecoder::BasePicture *base_picture, const uint8_t *enhancement_data,
	                           size_t enhancement_data_size, uint64_t pts, bool is_lcevc_idr) override;

	void push_base_enhancement(const uint8_t *base_data, size_t base_data_size,
	                           Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], uint64_t pts,
	                           bool is_lcevc_idr) override;

	void deserialize_enhancement(const uint8_t *enhancement_data, size_t enhancement_data_size,
	                             Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) override;

	Dimensions get_dimensions() override;
	Colourspace get_colourspace() override;
	unsigned get_base_bitdepth() override;

	YUVWriter &writer_;
	YUVReader &reader_;

	Decoder decoder_;

	int count_ = 0;

	bool report_ = false;
	bool dithering_switch_ = false;
	bool dithering_fixed_ = false;
	bool apply_enhancement_ = true;
};

int main(int argc, char *argv[]) {
	BaseCoding base_video_type;
	bool base_external;
	bool keep_base;
	bool apply_enhancement;
	Encapsulation encapsulation;
	std::string input_es, output_yuv, base_yuv, input_yuv;
	BaseDecoder::Codec file_base;
	bool report;
	bool dithering_switch;
	bool dithering_fixed;
	unsigned limit = 1000000;

	try {
		cxxopts::Options options_description(argv[0], "LCEVC Decoder " GIT_VERSION);

		// clang-format off
		options_description.add_options()
			("i,input_file", "Input elementary stream filename", cxxopts::value<string>()->default_value("input.lvc"))
			("o,output_file", "Output filename for decoded YUV data", cxxopts::value<string>()->default_value("output.yuv"))
			("b,base", "Base codec (avc, hevc, evc, vvc, or yuv)", cxxopts::value<string>()->default_value("avc"))
			("base_encoder", "Base codec (same as --base)", cxxopts::value<string>()->default_value("avc"))
			("base_external", "Use an external base codec executable (select for decoding of monochrome output)", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("y,base_yuv", "Prepared YUV data for base decode", cxxopts::value<string>()->default_value(""))
			("input_yuv", "Original YUV data for PSNR computation", cxxopts::value<string>()->default_value(""))
			("l,limit", "Number of frames to decode", cxxopts::value<unsigned>()->default_value("1000000"))
			("dump_surfaces", "Dump intermediate surfaces to yuv files", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("encapsulation", "Wrap enhancement as SEI or NAL", cxxopts::value<string>()->default_value("nal"))
			("dithering_switch", "Disable decoder dithering independent of configuration in bitstream", cxxopts::value<bool>()->default_value("true"))
			("dithering_fixed", "Use a fixed seed for dithering", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("report", "Calculate PSNR and checksums", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("keep_base", "Keep the base + enhancement bitstreams and base decoded yuv file", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			("apply_enhancement", "Apply LCEVC enhancement data (residuals) on output YUV", cxxopts::value<bool>()->default_value("true"))

			// Retain additional arguments for backwards compatibility (to be removed in future release)
			("w,width", "Placeholder (to be removed in future release)", cxxopts::value<unsigned>()->default_value("1920"))
			("h,height", "Placeholder (to be removed in future release)", cxxopts::value<unsigned>()->default_value("1080"))
			("f,format", "Placeholder (to be removed in future release)", cxxopts::value<string>()->default_value("yuv420p"))
			("base_depth", "Placeholder (to be removed in future release)", cxxopts::value<unsigned>()->default_value("0"))

			("version", "Show version")
			("help", "Show help");
		// clang-format on

		const bool show_help = argc == 1;

		// Parse command line
		auto options = options_description.parse(argc, argv);

		// Help
		if (show_help || options.count("help")) {
			cout << options_description.help({""}) << std::endl;
			exit(0);
		}

		// Version
		if (options.count("version")) {
			INFO(GIT_VERSION);
			exit(0);
		}

		// Debugging
		Surface::set_dump_surfaces(options["dump_surfaces"].as<bool>());

		// Base codec
		//
		base_video_type = extract<BaseCoding>(options["base"].as<string>());
		if (options["base_encoder"].count())
			base_video_type = extract<BaseCoding>(options["base_encoder"].as<string>());

		switch (base_video_type) {
		case BaseCoding_AVC:
			file_base = BaseDecoder::AVC;
			break;
		case BaseCoding_HEVC:
			file_base = BaseDecoder::HEVC;
			break;
		case BaseCoding_VVC:
			file_base = BaseDecoder::VVC;
			break;
		case BaseCoding_EVC:
			file_base = BaseDecoder::EVC;
			break;
		case BaseCoding_YUV:
			file_base = BaseDecoder::None;
			break;
		default:
			CHECK(0);
		}

		base_external = options["base_external"].as<bool>();
		keep_base = options["keep_base"].as<bool>();
		apply_enhancement = options["apply_enhancement"].as<bool>();

		// Encapsulation
		encapsulation = extract<Encapsulation>(options["encapsulation"].as<string>());

		input_es = options["input_file"].as<string>();
		output_yuv = options["output_file"].as<string>();
		base_yuv = options["base_yuv"].as<string>();
		input_yuv = options["input_yuv"].as<string>();

		report = options["report"].as<bool>();
		dithering_switch = options["dithering_switch"].as<bool>();
		dithering_fixed = options["dithering_fixed"].as<bool>();
		limit = options["limit"].as<unsigned>();

		if (base_video_type == BaseCoding_YUV && base_yuv.empty())
			ERR("No base codec selected and no base yuv file provided.");

	} catch (const cxxopts::OptionException &e) {
		std::cout << "error parsing options: " << e.what() << std::endl;
		exit(1);
	}

	// Input
	//
	ESFile es_file;
	if (!es_file.Open(input_es, file_base)) {
		ERR("Cannot open file: %s\n", input_es.c_str());
	}

	// Dummy Output
	//
	unique_ptr<YUVWriter> yuv_writer(CreateYUVWriter(output_yuv));
	// additional Input for PSNR
	unique_ptr<YUVReader> yuv_reader(CreateYUVReader(input_yuv, 60));

	// Create Base Video Decpder
	DecoderApp app(*yuv_writer, *yuv_reader);
	std::unique_ptr<BaseVideoDecoder> base_video_decoder(
	    CreateBaseVideoDecoder(app, base_video_type, encapsulation, base_external, base_yuv, keep_base));

	app.report_ = report;
	app.dithering_switch_ = dithering_switch;
	app.dithering_fixed_ = dithering_fixed;
	app.apply_enhancement_ = apply_enhancement;

	const float start = (float)(system_timestamp() / 1000000.0);
	INFO("-- Starting: %.3f", start);

	base_video_decoder->start();

#if BITSTREAM_DEBUG
	goStat.Reset();
	goBits = fopen("BitstreamDec.txt", "w");
#endif

	ESFile::AccessUnit au;

	clock_t EnhaClock0;
	EnhaClock0 = clock();
	INFO("**** Enh. start %16d", EnhaClock0);

	unsigned count = 0;
	while (count < limit) {
		ESFile::Result rc = ESFile::EndOfFile;

		try {
			rc = es_file.NextAccessUnit(au);
		} catch (const runtime_error &e) {
			ERR("End of file: %s\n", e.what());
			rc = ESFile::NalParsingError;
		}

		if (rc != ESFile::Success)
			break;

		// Reassemble AU from NALUs
		vector<uint8_t> bytes;
		for (const auto &n : au.m_nalUnits)
			bytes.insert(bytes.end(), n.m_data.begin(), n.m_data.end());

		// Use derived Picture Order Count to construct a plausible PTS
		const uint64_t pts = au.m_poc + 1000;
		const bool is_base_idr = au.m_pictureType == BaseDecPictType::IDR;

		// Push AU into base decoder
		// INFO("AU: size %8d - pts. %8d", bytes.size(), pts);
		// REPORT("BASE AU -- pts. %8d size %8d", pts, bytes.size());
		base_video_decoder->push_au(bytes.data(), bytes.size(), pts, is_base_idr, au.m_pictureType);

		++count;
	}
	INFO("-- Flushing: %.3f", system_timestamp() / 1000000.0);

	base_video_decoder->push_au(0, 0, 0, false, 0);

	clock_t EnhaClock1;
	EnhaClock1 = clock();
	INFO("**** Enh. stop. %16d", EnhaClock1);
	INFO("@@@@ Enh. delta %16d", EnhaClock1 - EnhaClock0);

	INFO("-- Flushed: %.3f", system_timestamp() / 1000000.0);

	const float finish = (float)(system_timestamp() / 1000000.0);
	INFO("-- Finished: %.3f", finish);
	INFO("-- FPS: %.3f", (float)count / (finish - start));

	base_video_decoder->StatisticsComputation();

#if BITSTREAM_DEBUG
	goStat.Dump();
	fflush(goBits);
	fclose(goBits);
#endif
}

DecoderApp::DecoderApp(YUVWriter &writer, YUVReader &reader) : writer_(writer), reader_(reader) {}

void DecoderApp::push_base_enhancement(const BaseVideoDecoder::BasePicture *base_picture, const uint8_t *enhancement_data,
                                       size_t enhancement_data_size, uint64_t pts, bool is_lcevc_idr) {
	if (count_ == 0)
		INFO("-- Decoding: %.3f", system_timestamp() / 1000000.0);

	// Deserilize LCEVC data
	Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];
	decoder_.initialize_decode(Packet::build().contents(enhancement_data, (unsigned)enhancement_data_size).finish(), symbols);
	const SignaledConfiguration configuration = decoder_.get_configuration();
	decoder_.set_idr(is_lcevc_idr);

	if (count_ == 0) {
		// Set-up Output before decoding first frame
		unsigned width = configuration.global_configuration.resolution_width;
		unsigned height = configuration.global_configuration.resolution_height;
		if (configuration.sequence_configuration.conformance_window) {
			// Figure out output size after conformance window
			Dimensions dimensions = decoder_.get_dimensions();
			const unsigned cw = dimensions.crop_unit_width(0), ch = dimensions.crop_unit_height(0);
			width -= (configuration.sequence_configuration.conf_win_left_offset * cw +
			          configuration.sequence_configuration.conf_win_right_offset * cw);
			height -= (configuration.sequence_configuration.conf_win_top_offset * ch +
			           configuration.sequence_configuration.conf_win_bottom_offset * ch);
		}
		const ImageDescription image_description(configuration.global_configuration.image_format, width, height);
		writer_.update_data(image_description);
		// additional Input for PSNR
		if (&reader_)
			reader_.update_data(image_description);
		if (image_description.colourspace() == Colourspace_Y)
			ERR("Please use argument '--base_external=true' for decoding with a monochrome output");
	}

	// Base image
	//
	std::vector<Surface> base_planes;
	const ImageDescription base_desc = writer_.image_description()
	                                       .with_depth(configuration.global_configuration.base_depth)
	                                       .with_size(base_picture->width_y, base_picture->height_y);
	if (configuration.global_configuration.base_depth == 8) {
		base_planes.push_back(Surface::build_from<int8_t>()
		                          .contents((int8_t *)base_picture->data_y, base_picture->width_y, base_picture->height_y)
		                          .finish());
		base_planes.push_back(Surface::build_from<int8_t>()
		                          .contents((int8_t *)base_picture->data_u, base_picture->width_uv, base_picture->height_uv)
		                          .finish());
		base_planes.push_back(Surface::build_from<int8_t>()
		                          .contents((int8_t *)base_picture->data_v, base_picture->width_uv, base_picture->height_uv)
		                          .finish());
	} else {
		base_planes.push_back(Surface::build_from<int16_t>()
		                          .contents((int16_t *)base_picture->data_y, base_picture->width_y, base_picture->height_y)
		                          .finish());
		base_planes.push_back(Surface::build_from<int16_t>()
		                          .contents((int16_t *)base_picture->data_u, base_picture->width_uv, base_picture->height_uv)
		                          .finish());
		base_planes.push_back(Surface::build_from<int16_t>()
		                          .contents((int16_t *)base_picture->data_v, base_picture->width_uv, base_picture->height_uv)
		                          .finish());
	}

	// INFO("PTS: %" PRIu64 " PSS: %zu", pts, enhancement_data_size);
	// REPORT("ENHA AU -- pts. %8d size %8d ", pts, enhancement_data_size);

	if (&reader_) {
		auto reference_image = ExpandImage(reader_.read(count_), reader_.description());
		auto full_image = decoder_.decode(Image("base", base_desc, pts, base_planes), symbols, reference_image,
		                                  report_, dithering_switch_, dithering_fixed_, apply_enhancement_);
		writer_.write(full_image);
	} else {
		Image reference_image;
		auto full_image = decoder_.decode(Image("base", base_desc, pts, base_planes), symbols, reference_image,
		                                  report_, dithering_switch_, dithering_fixed_, apply_enhancement_);
		writer_.write(full_image);
	}

	count_++;
}

void DecoderApp::push_base_enhancement(const uint8_t *base_data, size_t base_data_size,
                                       Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], uint64_t pts,
                                       bool is_lcevc_idr) {

	Dimensions dimensions = decoder_.get_dimensions();
	const SignaledConfiguration configuration = decoder_.get_configuration();
	decoder_.set_idr(is_lcevc_idr);

	if (count_ == 0) {
		INFO("-- Decoding: %.3f", system_timestamp() / 1000000.0);

		// Set-up Output before decoding first frame
		unsigned width = configuration.global_configuration.resolution_width;
		unsigned height = configuration.global_configuration.resolution_height;
		if (configuration.sequence_configuration.conformance_window) {
			// Figure out output size after conformance window
			const unsigned cw = dimensions.crop_unit_width(0), ch = dimensions.crop_unit_height(0);
			width -= (configuration.sequence_configuration.conf_win_left_offset * cw +
			          configuration.sequence_configuration.conf_win_right_offset * cw);
			height -= (configuration.sequence_configuration.conf_win_top_offset * ch +
			           configuration.sequence_configuration.conf_win_bottom_offset * ch);
		}
		const ImageDescription image_description(configuration.global_configuration.image_format, width, height);
		writer_.update_data(image_description);
		// additional Input for PSNR
		if (&reader_)
			reader_.update_data(image_description);
	}

	// Base image
	//
	std::vector<Surface> base_planes;
	const ImageDescription base_desc = writer_.image_description()
	                                       .with_depth(configuration.global_configuration.base_depth)
	                                       .with_size(dimensions.base_width(), dimensions.base_height());

	if (configuration.global_configuration.base_depth == 8) {
		int offset = 0;
		for (unsigned plane = 0; plane < base_desc.num_planes(); plane++) {
			base_planes.push_back(Surface::build_from<int8_t>()
			                          .contents((int8_t *)(base_data + offset), base_desc.width(plane), base_desc.height(plane))
			                          .finish());
			offset += base_desc.width(plane) * base_desc.height(plane);
		}
	} else {
		int offset = 0;
		for (unsigned plane = 0; plane < base_desc.num_planes(); plane++) {
			base_planes.push_back(Surface::build_from<int16_t>()
			                          .contents((int16_t *)(base_data + offset), base_desc.width(plane), base_desc.height(plane))
			                          .finish());
			offset += base_desc.width(plane) * base_desc.height(plane) * sizeof(int16_t);
		}
	}

	// INFO("PTS: %" PRIu64 " PSS: %zu", pts, enhancement_data_size);
	// REPORT("ENHA AU -- pts. %8d size %8d ", pts, enhancement_data_size);

	Image base("base", base_desc, pts, base_planes);

	if (&reader_) {
		auto reference_image = ExpandImage(reader_.read(count_), reader_.description());
		auto full_image = decoder_.decode(base, symbols, reference_image, report_, dithering_switch_, dithering_fixed_,
		                                  apply_enhancement_);
		writer_.write(full_image);
	} else {
		Image reference_image;
		auto full_image = decoder_.decode(base, symbols, reference_image, report_, dithering_switch_, dithering_fixed_,
		                                  apply_enhancement_);
		writer_.write(full_image);
	}

	count_++;
}

// Deserilize LCEVC data
void DecoderApp::deserialize_enhancement(const uint8_t *enhancement_data, size_t enhancement_data_size,
                                         Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) {
	decoder_.initialize_decode(Packet::build().contents(enhancement_data, (unsigned)enhancement_data_size).finish(), symbols);
}

Dimensions DecoderApp::get_dimensions() { return decoder_.get_dimensions(); }

unsigned DecoderApp::get_base_bitdepth() { return decoder_.get_configuration().global_configuration.base_depth; }

Colourspace DecoderApp::get_colourspace() { return decoder_.get_configuration().global_configuration.colourspace; }
