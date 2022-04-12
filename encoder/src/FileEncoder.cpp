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
//
// FileEncoder.cpp
//
// Reads input file(s), manages enahancement & base codec, merges data, and writes to output
//

#include "FileEncoder.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

#include <cassert>

#include "Config.hpp"
#include "Convert.hpp"
#include "Downsampling.hpp"
#include "Encoder.hpp"
#include "Expand.hpp"
#include "Types.hpp"
#include "Upsampling.hpp"

#include "BitstreamPacker.hpp"
#include "Diagnostics.hpp"
#include "Image.hpp"
#include "Misc.hpp"
#include "Packet.hpp"
#include "Probe.hpp"
#include "TemporalDecode.hpp"
#include "YUVReader.hpp"
#include "YUVWriter.hpp"

#include "uESFile.h"

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern priority_queue<ReportStructure, vector<ReportStructure, allocator<ReportStructure>>, ReportStructureComp> goReportQueue;

using namespace vnova::utility;

#include "nlohmann/json.hpp"
using json = nlohmann::json;

namespace lctm {

// Implementation is specialised for base codec
//
class FileEncoderImpl : public FileEncoder {
public:
	//
	FileEncoderImpl(const ImageDescription &image_description, unsigned fps, const Parameters &parameters);

	~FileEncoderImpl() override;

	// Encode from 'src' YUV file, up to 'limit' frames (if limit > 0)
	//
	void encode_file(const string &src_filename, const string &dst_filename, const string &dst_filename_yuv,
	                 unsigned limit) override;

	// Encode from 'src' base file which will run the base deocoder first
	//
	void encode_file_with_decoder(const string &src_filename, const string base_filename, const string &dst_filename,
	                              const string &dst_filename_yuv, unsigned limit) override;

	// Encode from 'src' YUV file, base, base_recon & preprocessed info
	//
	void encode_file_with_base(const string &src_filename, const string base_filename, const string base_recon_filename,
	                           const string &dst_filename, const string &dst_filename_yuv, unsigned limit) override;

	void downsample_file(const string &src_filename, const string &dst_filename, unsigned limit) override;
	void upsample_file(const string &src_filename, const string &dst_filename, unsigned limit) override;

	virtual BaseDecoder::Codec es_file_type() const = 0;
	virtual BaseCoding base_coding() const = 0;

protected:
	// Body of encode loop
	void encode(const YUVReader &src_file, const YUVReader &recon_file, FILE *output_file, ESFile &es_file,
	            const string &dst_filename_yuv, unsigned limit);

	bool read_au(ESFile &es_file, ESFile::AccessUnit &au);
	Packet rbsp_encapsulate(const Packet &src) const;

	void initialize_encoder(const YUVReader &src_file, unsigned limit);
	struct RegisteredSEI {
		static Packet sei_payload(const Packet &enhancement_data);
	};
	struct UnregisteredSEI {
		static Packet sei_payload(const Packet &enhancement_data);
	};
	Packet nal_payload(const Packet &enhancement_data) const;

	virtual int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) = 0;

	ESFile::NalUnit build_nalu(bool is_idr, const Packet &payload) const;

	unsigned intra_period() const;

	virtual bool run_base_encoder(const string &input, const string &stream_output, const string &recon_output,
	                              unsigned frame_count) const = 0;
	virtual bool run_base_decoder(const string &input, const string &output) = 0;

	// Size, format & depth - base, intermediate & full resolution
	//
	const unsigned fps_;
	const Parameters parameters_;

	const ScalingMode scaling_mode_[MAX_NUM_LOQS];
	const Downsample downsample_luma_;
	const Downsample downsample_chroma_;
	const Upsample upsample_;
	unsigned upsampling_coefficients_[4];

	const Encapsulation encapsulation_;

	Encoder encoder_;
};

FileEncoderImpl::FileEncoderImpl(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
    : fps_(fps), parameters_(parameters), scaling_mode_{parameters_["scaling_mode_level1"].get_enum<ScalingMode>(ScalingMode_None),
                                                        parameters_["scaling_mode_level2"].get_enum<ScalingMode>(ScalingMode_2D)},
      downsample_luma_(parameters_["encoding_downsample_luma"].get_enum<Downsample>(Downsample_Lanczos3)),
      downsample_chroma_(parameters_["encoding_downsample_chroma"].get_enum<Downsample>(Downsample_Lanczos3)),
      upsample_(parameters_["encoding_upsample"].get_enum<Upsample>(Upsample_ModifiedCubic)),
      encapsulation_(parameters_["encapsulation"].get_enum<Encapsulation>(Encapsulation_None)),
      encoder_(image_description, parameters) {

	parameters_["encoding_upsample"].get_vector<unsigned>(upsampling_coefficients_, 4);
}

FileEncoderImpl::~FileEncoderImpl() {}

//
//
void FileEncoderImpl::encode_file(const string &src_filename, const string &dst_filename, const string &dst_filename_yuv,
                                  unsigned limit) {
	// Temp. input file to encode
	//
	string base_yuv_filename = make_temporary_filename("_base.yuv");

	// Output files
	//
	string base_bin_filename;
	string base_recon_filename;

	if (!parameters_["keep_base"].get<bool>(false)) {
		base_bin_filename = make_temporary_filename("_base.bin");
		base_recon_filename = make_temporary_filename("_base_recon.yuv");
	} else {
		base_bin_filename = "base.bin";
		base_recon_filename = "base_recon.yuv";
	}

	INFO("Using temporary file for base YUV: %s", base_yuv_filename.c_str());
	INFO("Using temporary file for base stream: %s", base_bin_filename.c_str());
	INFO("Using temporary file for base recon: %s", base_recon_filename.c_str());

	// Read source and downsample into base
	//
	unique_ptr<YUVReader> src_file(CHECK(CreateYUVReader(src_filename, encoder_.src_image_description(), fps_)));
	initialize_encoder(*src_file, limit);
	auto base_yuv = CHECK(CreateYUVWriter(base_yuv_filename, encoder_.base_image_description(), true));

	int64_t pts = 0;
	int64_t duration = 90000L / fps_;

	const unsigned frame_count = min(src_file->length(), limit);
	INFO("input limit %8d - local count %8d", limit, frame_count);
	const bool level1_depth_flag = encoder_.level1_depth_flag();

	for (unsigned f = 0; f < frame_count; ++f, pts += duration) {
		INFO("Writing base %d", f);
		const Image src = ExpandImage(src_file->read(f, pts), encoder_.enhancement_image_description());
		// Downsample and bit shifting depending base and enhancement bit depths
		const Image img = DownsampleImage(DownsampleImage(src, downsample_luma_, downsample_chroma_, scaling_mode_[LOQ_LEVEL_2],
		                                                  level1_depth_flag ? 0 : encoder_.base_image_description().bit_depth()),
		                                  downsample_luma_, downsample_chroma_, scaling_mode_[LOQ_LEVEL_1],
		                                  level1_depth_flag ? encoder_.base_image_description().bit_depth()	: 0);
		base_yuv->write(img);
	}

	// Run the base encoder yuv-file-> es-file and recon
	run_base_encoder(base_yuv->filename(), base_bin_filename, base_recon_filename, frame_count);

	// Open file
	unique_ptr<YUVReader> recon_file(CHECK(CreateYUVReader(base_recon_filename, encoder_.base_image_description(), fps_)));

	UniquePtrFile output_file(CHECK(fopen(format("%s", dst_filename.c_str()).c_str(), "wb")));

	ESFile es_file;
	CHECK(es_file.Open(base_bin_filename, es_file_type()));

	// Encode enhancement given src, base and base_recon
	clock_t EnhaClock1 = clock();

	encode(*src_file, *recon_file, output_file.get(), es_file, dst_filename_yuv, limit);

	clock_t EnhaClock2 = clock();

	INFO("**** Encode enhancement. delta. %16d secs", (EnhaClock2 - EnhaClock1) / CLOCKS_PER_SEC);
	// Clear up
	if (!parameters_["keep_base"].get<bool>(false)) {
		::remove(base_yuv_filename.c_str());
		::remove(base_bin_filename.c_str());
		::remove(base_recon_filename.c_str());
	}
}

void FileEncoderImpl::encode_file_with_decoder(const string &src_filename, const string base_filename, const string &dst_filename,
                                               const string &dst_filename_yuv, unsigned limit) {

	// Initialize encoder to derive final dimensions
	unique_ptr<YUVReader> src_file(CHECK(CreateYUVReader(src_filename, encoder_.src_image_description(), fps_)));
	initialize_encoder(*src_file, limit);

	// Verify dimensions of base input
	unsigned base_width = 0, base_height = 0, base_depth = 0, chroma_format_idc;
	if (probe_es_file(base_filename, base_coding(), base_width, base_height, base_depth, chroma_format_idc)) {
		CHECK(base_width == encoder_.base_image_description().width());
		CHECK(base_height == encoder_.base_image_description().height());
		CHECK(base_depth == encoder_.base_image_description().bit_depth());
		CHECK(chroma_format_idc == (unsigned)encoder_.base_image_description().colourspace());
	} else {
		WARN("Cannot find dimension of base.");
	}

	// Generate base recon.
	string base_recon_filename = make_temporary_filename(
	    format("_base_recon_%dx%d.yuv", encoder_.base_image_description().width(), encoder_.base_image_description().height()));
	INFO("Using temporary file for base recon: %s", base_recon_filename.c_str());

	INFO("-- Starting decoding process: %.3f", system_timestamp() / 1000000.0);
	run_base_decoder(base_filename, base_recon_filename);

	// Open file
	unique_ptr<YUVReader> recon_file(CHECK(CreateYUVReader(base_recon_filename, encoder_.base_image_description(), fps_)));

	UniquePtrFile output_file(CHECK(fopen(format("%s", dst_filename.c_str()).c_str(), "wb")));

	ESFile es_file;
	CHECK(es_file.Open(base_filename, es_file_type()));

	encode(*src_file, *recon_file, output_file.get(), es_file, dst_filename_yuv, limit);

	// Clear up
	if (!parameters_["keep_base"].get<bool>(false)) {
		::remove(base_recon_filename.c_str());
	}
}

// Fetch next AU from an ES file - return true if not EOF
//
bool FileEncoderImpl::read_au(ESFile &es_file, ESFile::AccessUnit &au) {
	ESFile::Result rc = ESFile::EndOfFile;

	try {
		rc = es_file.NextAccessUnit(au);
	} catch (const runtime_error &) {
		rc = ESFile::NalParsingError;
	}

	return rc == ESFile::Success;
}

static BaseFrameType frame_type(BaseDecPictType::Enum pt) {
	switch (pt) {
	case BaseDecPictType::IDR:
		return BaseFrame_IDR;
	case BaseDecPictType::I:
		return BaseFrame_Intra;
	case BaseDecPictType::P:
		// return BaseFrame_Inter;
		return BaseFrame_Pred;
	case BaseDecPictType::B:
		// return BaseFrame_Inter;
		return BaseFrame_Bidi;
	case BaseDecPictType::BR:
		// return BaseFrame_Inter;
		return BaseFrame_Bidi;
	case BaseDecPictType::Unknown:
		return BaseFrame_Inter;
	default:
		CHECK(0);
		return BaseFrame_IDR;
	}
}

void FileEncoderImpl::encode_file_with_base(const string &src_filename, const string base_filename,
                                            const string base_recon_filename, const string &dst_filename,
                                            const string &dst_filename_yuv, unsigned limit) {
	// Open files
	//
	unique_ptr<YUVReader> src_file(CHECK(CreateYUVReader(src_filename, encoder_.src_image_description(), fps_)));
	unique_ptr<YUVReader> recon_file(CHECK(CreateYUVReader(base_recon_filename, encoder_.base_image_description(), fps_)));

	initialize_encoder(*src_file, limit);

	// Verify dimensions of base input
	unsigned base_width = 0, base_height = 0, base_depth = 0, chroma_format_idc;
	if (probe_es_file(base_filename, base_coding(), base_width, base_height, base_depth, chroma_format_idc)) {
		CHECK(base_width == encoder_.base_image_description().width());
		CHECK(base_height == encoder_.base_image_description().height());
		CHECK(base_depth == encoder_.base_image_description().bit_depth());
		CHECK(chroma_format_idc == (unsigned)encoder_.base_image_description().colourspace());
	} else {
		WARN("Cannot find dimension of base.");
	}

	UniquePtrFile output_file(CHECK(fopen(format("%s", dst_filename.c_str()).c_str(), "wb")));

	ESFile es_file;
	CHECK(es_file.Open(base_filename, es_file_type()));

	encode(*src_file, *recon_file, output_file.get(), es_file, dst_filename_yuv, limit);
}

template <typename T, typename... Args> std::unique_ptr<T> make_unique(Args &&...args) {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

void FileEncoderImpl::encode(const YUVReader &src_file, const YUVReader &recon_file, FILE *output_file, ESFile &es_file,
                             const string &dst_filename_yuv, unsigned limit) {

	// Queue of pending AUs, so that enhancment can be encoded in display order
	deque<ESFile::AccessUnit> pending;

	unsigned display_frame = 0;
	unsigned written_count = 0;
	bool end_of_es = false;
	encoder_.set_last_idr_frame_num(0);
	std::vector<std::unique_ptr<Image>> src;

	const unsigned num_frames = std::min(src_file.length(), limit);
	if (num_frames == 0)
		ERR("Frames cannot be read from source.");

	// Fill with initial frame
	src.push_back(make_unique<Image>(ExpandImage(src_file.read(0, 0), encoder_.enhancement_image_description())));

	while (written_count < limit && !end_of_es) {

		// Get next AU from base stream
		ESFile::AccessUnit au;
		if (read_au(es_file, au))
			pending.push_front(au);
		else
			end_of_es = true;

		// Advance 'enhancement_poc' past AUs that are present in queue, adding enhancement as needed
		bool keep_looking = false;
		do {
			keep_looking = false;
			for (auto &a : pending) {

				// AVC POCs are increased by 2 XXX move to virtual fn
				const unsigned enhance_poc = (es_file_type() == BaseDecoder::AVC) ? 2 * display_frame : display_frame;

				if (a.m_poc == enhance_poc) {
					if (display_frame != 0)
						src.erase(src.begin());
					if (src.size() < 2 && (display_frame + 1) < src_file.length()) {
						src.push_back(make_unique<Image>(ExpandImage(src_file.read(display_frame + 1, display_frame + 1),
						                                             encoder_.enhancement_image_description())));
					}

					const Image recon = recon_file.read(display_frame, display_frame);

					const Image src_intermediate =
					    DownsampleImage(*src[0], downsample_luma_, downsample_chroma_, scaling_mode_[LOQ_LEVEL_2],
					                    encoder_.intermediate_image_description().bit_depth());

					// Enhancement encoding
					const bool is_idr = a.m_pictureType == BaseDecPictType::IDR;
					Packet pss = encoder_.encode(src, src_intermediate, recon, frame_type(a.m_pictureType), is_idr, display_frame,
					                             dst_filename_yuv);

					goReportStructure.miTimeStamp = (int)a.m_poc;
					goReportStructure.miPictureType = (int)a.m_pictureType;
					goReportStructure.miBaseSize = (int)a.m_size;
					if (!pss.empty())
						goReportStructure.miEnhancementSize = add_enhancement(a, is_idr, pss);
					goReportQueue.push(goReportStructure);
					goPsnr.miBaseBytes += goReportStructure.miBaseSize;
					goPsnr.miEnhancementBytes += goReportStructure.miEnhancementSize;

					// clang-format off
					fprintf(stdout, "ENC. [pts. %4d] [type %4d] [base %8d] [enha %8d] ",
						goReportStructure.miTimeStamp, goReportStructure.miPictureType, goReportStructure.miBaseSize, goReportStructure.miEnhancementSize);
					fflush(stdout);
					fprintf(stdout, "[psnrY %8.4f] ", goPsnr.mfCurPsnr[0]);
					if (encoder_.src_image_description().num_planes() > 1)
						fprintf(stdout, "[psnrU %8.4f] [psnrV %8.4f] \n", (goPsnr.mfCurPsnr[1]), (goPsnr.mfCurPsnr[2]));
					else
						fprintf(stdout, "\n");
					fflush(stdout);
					fprintf(stdout, "[MD5Y %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] ", 
						gaucMd5Digest[0][0],  gaucMd5Digest[0][1],  gaucMd5Digest[0][2],  gaucMd5Digest[0][3],  
						gaucMd5Digest[0][4],  gaucMd5Digest[0][5],  gaucMd5Digest[0][6],  gaucMd5Digest[0][7],  
						gaucMd5Digest[0][8],  gaucMd5Digest[0][9],  gaucMd5Digest[0][10], gaucMd5Digest[0][11], 
						gaucMd5Digest[0][12], gaucMd5Digest[0][13], gaucMd5Digest[0][14], gaucMd5Digest[0][15]);
					fflush(stdout);
					if (encoder_.src_image_description().num_planes() > 1) {
						fprintf(stdout, "[MD5U %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] ", 
							gaucMd5Digest[1][0],  gaucMd5Digest[1][1],  gaucMd5Digest[1][2],  gaucMd5Digest[1][3],  
							gaucMd5Digest[1][4],  gaucMd5Digest[1][5],  gaucMd5Digest[1][6],  gaucMd5Digest[1][7],  
							gaucMd5Digest[1][8],  gaucMd5Digest[1][9],  gaucMd5Digest[1][10], gaucMd5Digest[1][11], 
							gaucMd5Digest[1][12], gaucMd5Digest[1][13], gaucMd5Digest[1][14], gaucMd5Digest[1][15]);
						fflush(stdout);
						fprintf(stdout, "[MD5V %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X] \n", 
							gaucMd5Digest[2][0],  gaucMd5Digest[2][1],  gaucMd5Digest[2][2],  gaucMd5Digest[2][3],  
							gaucMd5Digest[2][4],  gaucMd5Digest[2][5],  gaucMd5Digest[2][6],  gaucMd5Digest[2][7],  
							gaucMd5Digest[2][8],  gaucMd5Digest[2][9],  gaucMd5Digest[2][10], gaucMd5Digest[2][11], 
							gaucMd5Digest[2][12], gaucMd5Digest[2][13], gaucMd5Digest[2][14], gaucMd5Digest[2][15]);
						fflush(stdout);
					} else {
						fprintf(stdout, "\n");
						fflush(stdout);
					}
					// clang-format on

					display_frame++;
					keep_looking = true;

					// INFO("base -- %4u %8u %4u", display_frame, pss.size(), a.m_pictureType);
					break;
				}
			}
		} while (keep_looking);

		// Consume AUs that have been enhanced from the back of queue and write to output stream
		const unsigned write_poc = (es_file_type() == BaseDecoder::AVC) ? 2 * display_frame : display_frame;
		while (!pending.empty() && (pending.back().m_poc < write_poc)) {
			for (const auto &n : pending.back().m_nalUnits) {
				size_t sz = n.m_data.size();
				// INFO("lvc  -- %4u %8u", written_count, sz);
				CHECK(fwrite(n.m_data.data(), 1, sz, output_file) == sz);
			}
			pending.pop_back();
			++written_count;
		}
	}

	if (written_count > 0) {
		int iFrames = written_count;

		float fAccMse[MAX_NUM_PLANES];
		float fPsnr[MAX_NUM_PLANES];
		for (unsigned plane = 0; plane < encoder_.src_image_description().num_planes(); plane++) {
			fAccMse[plane] = goPsnr.mfAccMse[plane] / iFrames;
			fPsnr[plane] = (float)(10.0f * log10((32767.0f * 32767.0f) / fAccMse[plane]));
		}

		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
		if (encoder_.src_image_description().num_planes() > 1)
			REPORT("PSNR -- YUV %8.4f -- Y %8.4f U %8.4f V %8.4f", (6 * fPsnr[0] + fPsnr[1] + fPsnr[2]) / 8, fPsnr[0], fPsnr[1],
			       fPsnr[2]);
		else
			REPORT("PSNR -- Y %8.4f", fPsnr[0]);
		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
		REPORT("BITS -- base %8d bps -- enha %8d bps ", (goPsnr.miBaseBytes * 8 * this->fps_) / iFrames,
		       (goPsnr.miEnhancementBytes * 8 * this->fps_) / iFrames);
		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
	}
	
}

// RBSP encapsualtion (0b00000000 0b00000000 0b000000xx -> 0b00000000 0b00000000 0b00000011 0b000000xx)
//
Packet FileEncoderImpl::rbsp_encapsulate(const Packet &src) const {
	PacketView in(src);

	vector<uint8_t> out;

	unsigned zeros = 0;
	for (unsigned int i = 0; i < in.size(); ++i) {
		uint8_t b = in.data()[i];

		if (zeros == 2 && (b & ~3) == 0) {
			out.push_back(0x03);
			zeros = 0;
		}

		if (b == 0)
			++zeros;
		else
			zeros = 0;

		out.push_back(b);
	}

	out.push_back(0x80);

	return Packet::build().contents(out).finish();
}

// Make the SEI encapsulated payload data given enhancement data
//
Packet FileEncoderImpl::RegisteredSEI::sei_payload(const Packet &enhancement_data) {
	BitstreamPacker sei;

	// SEI Type: Registered
	sei.u(8, 0x04);

	// SEI Length
	unsigned l = enhancement_data.size() + 4;
	while (l >= 255) {
		sei.u(8, 255);
		l -= 255;
	}
	sei.u(8, l);

	// SEI code
	static const uint8_t sei_code[4] = {0xb4, 0x00, 0x50, 0x00};
	sei.bytes(sei_code, 4);

	// Enhancement part of payload
	sei.bytes(enhancement_data);

	return sei.finish();
}

Packet FileEncoderImpl::UnregisteredSEI::sei_payload(const Packet &enhancement_data) {
	BitstreamPacker sei;

	// SEI Type: Unregistered
	sei.u(8, 0x05);

	// SEI Length
	unsigned l = enhancement_data.size() + 16;
	while (l >= 255) {
		sei.u(8, 255);
		l -= 255;
	}
	sei.u(8, l);

	// UUID
	static const uint8_t uuid[16] = {0xa7, 0xc4, 0x6d, 0xed, 0x49, 0xd8, 0x38, 0xeb,
	                                 0x9a, 0xad, 0x6d, 0xa6, 0x84, 0x97, 0xa7, 0x54};
	sei.bytes(uuid, 16);

	// Enhancement part of payload
	sei.bytes(enhancement_data);

	return sei.finish();
}

// Make the NAL encapsulated payload data given enhancement data
//
Packet FileEncoderImpl::nal_payload(const Packet &enhancement_data) const {
	BitstreamPacker p;

	// Enhancement part of payload
	p.bytes(enhancement_data);

	return p.finish();
}

// Build  a standard LCEV NALU
//
ESFile::NalUnit FileEncoderImpl::build_nalu(const bool is_idr, const Packet &payload) const {
	// nal_marker, forbidden_zero_bit, forbidden_one_bit, nal_unit_type=28/29, reserved_flag
	const NalUnitType nal_type = is_idr ? NalUnitType::LCEVC_IDR : NalUnitType::LCEVC_NON_IDR;
	const uint8_t nalu_header_lcevc[5] = {0x00, 0x00, 0x01, (uint8_t)(is_idr ? 0x7b : 0x79), 0xff};
	ESFile::NalUnit nal_unit = {(uint32_t)nal_type, DataBuffer(nalu_header_lcevc, nalu_header_lcevc + sizeof(nalu_header_lcevc))};

	const Packet rbsp = rbsp_encapsulate(nal_payload(payload));
	const PacketView v(rbsp);
	nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());

	return nal_unit;
}

// Figure out IntraPeriod for base layer
//
unsigned FileEncoderImpl::intra_period() const {
	// Check for explicit set
	if (!parameters_["intra_period"].empty())
		return parameters_["intra_period"].get<unsigned>();

	// .. otherwise derive from frame rate
	switch (fps_) {
	case 30:
		return 32;
	case 50:
		return 48;
	case 60:
		return 64;

	default:
		INFO("Unrecognised frame rate: %u", fps_);
		return 64;
	}
}

// Initialize Encoder configuration to figure out final dimensions (after possible conformance window)
//
void FileEncoderImpl::initialize_encoder(const YUVReader &src_file, unsigned limit) {
	std::vector<std::unique_ptr<Image>> src;
	const unsigned num_frames = std::min(src_file.length(), limit);
	if (num_frames == 0)
		ERR("Frames cannot be read from source.");

	// Fill vector with first 5 frames
	for (unsigned n = 0; n < std::min(5u, num_frames); n++)
		src.push_back(make_unique<Image>(ExpandImage(src_file.read(n, n), encoder_.enhancement_image_description())));
	encoder_.initialise_config(parameters_, src);
}

//// AVC Based Codec
//
class FileEncoderImplAVC : public FileEncoderImpl {
public:
	FileEncoderImplAVC(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
	    : FileEncoderImpl(image_description, fps, parameters) {}

	BaseDecoder::Codec es_file_type() const override { return BaseDecoder::AVC; };
	BaseCoding base_coding() const override { return BaseCoding_AVC; }

	bool run_base_encoder(const string &yuv_file, const string &es_file, const string &recon_file,
	                      unsigned frame_count) const override {
		const string prog = get_program_directory("external_codecs/JM/lencod");
		const string default_cfg = get_program_directory("external_codecs/JM/encoder.cfg");
		const string cfg = get_program_directory("external_codecs/JM/encoder_randomaccess.cfg");

		const unsigned qp_islice = encoder_.base_qp();
		const unsigned qp_pslice = qp_islice + 1;
		const unsigned qp_bslice = qp_islice + 1;

		string cmd_line =
		    format("%s -d %s -f %s -p InputFile=%s -p OutputFile=%s -p ReconFile=%s "
		           "-p SourceWidth=%u -p SourceHeight=%u "
		           "-p SourceBitDepthLuma=%u -p SourceBitDepthChroma=%u -p OutputBitDepthLuma=%u -p OutputBitDepthChroma=%u "
		           "-p FrameRate=%u -p YUVFormat=%u "
		           "-p QPISlice=%u -p QPPSlice=%u -p QPBSlice=%u -p IntraPeriod=%u -p FramesToBeEncoded=%u",
		           prog.c_str(), default_cfg.c_str(), cfg.c_str(), yuv_file.c_str(), es_file.c_str(), recon_file.c_str(),
		           encoder_.base_image_description().width(), encoder_.base_image_description().height(),
		           encoder_.base_image_description().bit_depth(), encoder_.base_image_description().bit_depth(),
		           encoder_.base_image_description().bit_depth(), encoder_.base_image_description().bit_depth(), fps_,
		           (unsigned)encoder_.base_image_description().colourspace(), qp_islice, qp_pslice, qp_bslice, intra_period(),
		           frame_count);

		if (encoder_.base_image_description().colourspace() == Colourspace_YUV444)
			cmd_line.append(" -p WeightedPrediction=0 -p WeightedBiprediction=0 -p ProfileIDC=244");
		else if (encoder_.base_image_description().colourspace() == Colourspace_YUV422)
			cmd_line.append(" -p ProfileIDC=122");

		INFO("Running: %s", cmd_line.c_str());
		return system(cmd_line.c_str()) == 0;
	}

	// Insert enhancement data
	//
	int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) override {
		ESFile::NalUnit nal_unit;

		if (encapsulation_ == Encapsulation_None || encapsulation_ == Encapsulation_NAL) {
			// Use LCEVC Standard NALU
			nal_unit = build_nalu(is_idr, payload);
		} else if (encapsulation_ == Encapsulation_SEI_Registered) {
			// Encapsulate LCEVC Data into a NAL unit
			ESFile::NalUnit nal_unit_lcevc = build_nalu(is_idr, payload);
			Packet payload_lcevc = Packet::build().contents(nal_unit_lcevc.m_data).finish();

			// Regsitered SEI Encapsulation of LCEVC NAL unit
			// nal_marker, forbidden_zero_bit, nal_unit_type=6, reserved_flag
			static const uint8_t nalu_header[4] = {0x00, 0x00, 0x01, 0x06};
			nal_unit = {6, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(RegisteredSEI::sei_payload(payload_lcevc));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_SEI_Unregistered) {
			// Ungreistered SEI Encapsulation
			// nal_marker, forbidden_zero_bit, nal_unit_type=6, reserved_flag
			static const uint8_t nalu_header[4] = {0x00, 0x00, 0x01, 0x06};
			nal_unit = {6, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(UnregisteredSEI::sei_payload(payload));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else {
			CHECK(0);
		}

		// Insert after - after AUD and PPS/SPS
		unsigned offset = 0;
		for (offset = 0; offset < au.m_nalUnits.size(); ++offset) {
			if (au.m_nalUnits[offset].m_type < 7 || au.m_nalUnits[offset].m_type > 9)
				break;
		}

		au.m_nalUnits.insert(au.m_nalUnits.begin() + offset, nal_unit);
		return (int)(nal_unit.m_data.size());
	}

	bool run_base_decoder(const string &input, const string &output) override {
		string p = get_program_directory("external_codecs/JM/ldecod");

		INFO("Using base decoder %s", p.c_str());

		return system(format("%s -p InputFile=%s -p OutputFile=%s "
		                     "-p WriteUV=0",
		                     p.c_str(), input.c_str(), output.c_str())
		                  .c_str()) == 0;
	}
};

//// HEVC Base Codec
//
class FileEncoderImplHEVC : public FileEncoderImpl {
public:
	FileEncoderImplHEVC(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
	    : FileEncoderImpl(image_description, fps, parameters) {}

	BaseDecoder::Codec es_file_type() const override { return BaseDecoder::HEVC; };
	BaseCoding base_coding() const override { return BaseCoding_HEVC; }

	bool run_base_encoder(const string &yuv_file, const string &es_file, const string &recon_file,
	                      unsigned frame_count) const override {
		string prog = get_program_directory("external_codecs/HM/TAppEncoder");
		const string cfg = get_program_directory("external_codecs/HM/encoder_randomaccess_main10.cfg");
		if (encoder_.base_image_description().bit_depth() > Bitdepth_10) {
			WARN("Using HM with HIGH_BITDEPTH_SUPPORT enabled.");
			prog.append("_HighBitdepth");
		}

		const unsigned qp = encoder_.base_qp();
		unsigned chroma_format;
		switch (encoder_.base_image_description().colourspace()) {
		case Colourspace_Y:
			chroma_format = 400;
			break;
		case Colourspace_YUV420:
			chroma_format = 420;
			break;
		case Colourspace_YUV422:
			chroma_format = 422;
			break;
		case Colourspace_YUV444:
			chroma_format = 444;
			break;
		default:
			ERR("Not a colour space");
		}

		string cmd_line =
		    format("%s -c %s --InputFile=%s ---BitstreamFile=%s --ReconFile=%s "
		           "--SourceWidth=%d --SourceHeight=%d --InputBitDepth=%d --OutputBitDepth=%d --InternalBitDepth=%d --FrameRate=%d "
		           "--Level=4.1 --QP=%d --IntraPeriod=%d --FramesToBeEncoded=%d --ConformanceWindowMode=1 --InputChromaFormat=%u",
		           prog.c_str(), cfg.c_str(), yuv_file.c_str(), es_file.c_str(), recon_file.c_str(),
		           encoder_.base_image_description().width(), encoder_.base_image_description().height(),
		           encoder_.base_image_description().bit_depth(), encoder_.base_image_description().bit_depth(),
		           encoder_.base_image_description().bit_depth(), fps_, qp, intra_period(), frame_count, chroma_format);

		if (encoder_.base_image_description().bit_depth() > Bitdepth_12) {
			cmd_line.append(" --Profile=main_444_16");
		} else if (encoder_.base_image_description().colourspace() != Colourspace_YUV420 ||
		           encoder_.base_image_description().bit_depth() > Bitdepth_10)
			cmd_line.append(" --Profile=main-RExt");

		INFO("Running: %s", cmd_line.c_str());
		return system(cmd_line.c_str()) == 0;
	}

	// Insert enhancement data NALU
	//
	int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) override {
		ESFile::NalUnit nal_unit;

		if (encapsulation_ == Encapsulation_None || encapsulation_ == Encapsulation_NAL) {
			// Use LCEVC Standard NALU
			nal_unit = build_nalu(is_idr, payload);
		} else if (encapsulation_ == Encapsulation_SEI_Registered) {
			// Encapsulate LCEVC Data into a NAL unit
			ESFile::NalUnit nal_unit_lcevc = build_nalu(is_idr, payload);
			Packet payload_lcevc = Packet::build().contents(nal_unit_lcevc.m_data).finish();

			// Regsitered SEI Encapsulation of LCEVC NAL unit
			static const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x4e, 0x01}; // nal_unit_type=39 (SEI)
			nal_unit = {39, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(RegisteredSEI::sei_payload(payload_lcevc));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_SEI_Unregistered) {
			// Unregistered SEI Encapsulation
			static const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x4e, 0x01}; // nal_unit_type=39 (SEI)
			nal_unit = {39, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(UnregisteredSEI::sei_payload(payload));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else {
			CHECK(0);
		}

		unsigned offset = 0;
		for (offset = 0; offset < au.m_nalUnits.size(); ++offset) {
			if (au.m_nalUnits[offset].m_type < 32 || au.m_nalUnits[offset].m_type > 35)
				break;
		}

		au.m_nalUnits.insert(au.m_nalUnits.begin() + offset, nal_unit);
		return (int)(nal_unit.m_data.size());
	}

	bool run_base_decoder(const string &input, const string &output) override {
		string p = get_program_directory("external_codecs/HM/TAppDecoder");
		if (encoder_.base_image_description().bit_depth() > Bitdepth_10) {
			WARN("Using HM with HIGH_BITDEPTH_SUPPORT enabled.");
			p.append("_HighBitdepth");
		}

		INFO("Using base decoder %s", p.c_str());

		return system(format("\"%s\" -d %u -b %s -o %s", p.c_str(), encoder_.base_image_description().bit_depth(), input.c_str(),
		                     output.c_str())
		                  .c_str()) == 0;
	}
};


class FileEncoderImplX265 : public FileEncoderImpl {
public:
	FileEncoderImplX265(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
	    : FileEncoderImpl(image_description, fps, parameters) {}

	BaseDecoder::Codec es_file_type() const override { return BaseDecoder::HEVC; };
	BaseCoding base_coding() const override { return BaseCoding_X265; }

	bool run_base_encoder(const string &yuv_file, const string &es_file, const string &recon_file,
	                      unsigned frame_count) const override {
		string prog = get_program_directory("external_codecs/x265/x265");
		if (encoder_.base_image_description().bit_depth() > Bitdepth_10) {
			WARN("Using HM with HIGH_BITDEPTH_SUPPORT enabled.");
			prog.append("_HighBitdepth");
		}

		const unsigned qp = encoder_.base_qp();
		unsigned chroma_format;
		switch (encoder_.base_image_description().colourspace()) {
		case Colourspace_Y:
			chroma_format = 400;
			break;
		case Colourspace_YUV420:
			chroma_format = 420;
			break;
		case Colourspace_YUV422:
			chroma_format = 422;
			break;
		case Colourspace_YUV444:
			chroma_format = 444;
			break;
		default:
			ERR("Not a colour space");
		}

		string cmd_line =
			format("%s --input %s --input-res %dx%d --fps %d --qp %d --input-depth %d --frames %d --log-level 2 -o %s --recon %s --recon-depth %d",
			prog.c_str(), yuv_file.c_str(), encoder_.base_image_description().width(), encoder_.base_image_description().height(),
			fps_, qp, encoder_.base_image_description().bit_depth(), frame_count, es_file.c_str(),
			recon_file.c_str(), encoder_.base_image_description().bit_depth()
			);

		// if (encoder_.base_image_description().bit_depth() > Bitdepth_12) {
		// 	cmd_line.append(" --Profile=main_444_16");
		// } else if (encoder_.base_image_description().colourspace() != Colourspace_YUV420 ||
		//            encoder_.base_image_description().bit_depth() > Bitdepth_10)
		// 	cmd_line.append(" --Profile=main-RExt");

		INFO("Running: %s", cmd_line.c_str());
		return system(cmd_line.c_str()) == 0;
	}

	// Insert enhancement data NALU
	//
	int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) override {
		ESFile::NalUnit nal_unit;

		if (encapsulation_ == Encapsulation_None || encapsulation_ == Encapsulation_NAL) {
			// Use LCEVC Standard NALU
			nal_unit = build_nalu(is_idr, payload);
		} else if (encapsulation_ == Encapsulation_SEI_Registered) {
			// Encapsulate LCEVC Data into a NAL unit
			ESFile::NalUnit nal_unit_lcevc = build_nalu(is_idr, payload);
			Packet payload_lcevc = Packet::build().contents(nal_unit_lcevc.m_data).finish();

			// Regsitered SEI Encapsulation of LCEVC NAL unit
			static const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x4e, 0x01}; // nal_unit_type=39 (SEI)
			nal_unit = {39, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(RegisteredSEI::sei_payload(payload_lcevc));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_SEI_Unregistered) {
			// Unregistered SEI Encapsulation
			static const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x4e, 0x01}; // nal_unit_type=39 (SEI)
			nal_unit = {39, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(UnregisteredSEI::sei_payload(payload));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else {
			CHECK(0);
		}

		unsigned offset = 0;
		for (offset = 0; offset < au.m_nalUnits.size(); ++offset) {
			if (au.m_nalUnits[offset].m_type < 32 || au.m_nalUnits[offset].m_type > 35)
				break;
		}

		au.m_nalUnits.insert(au.m_nalUnits.begin() + offset, nal_unit);
		return (int)(nal_unit.m_data.size());
	}

	bool run_base_decoder(const string &input, const string &output) override {
		string p = get_program_directory("external_codecs/HM/TAppDecoder");
		if (encoder_.base_image_description().bit_depth() > Bitdepth_10) {
			WARN("Using HM with HIGH_BITDEPTH_SUPPORT enabled.");
			p.append("_HighBitdepth");
		}

		INFO("Using base decoder %s", p.c_str());

		return system(format("\"%s\" -d %u -b %s -o %s", p.c_str(), encoder_.base_image_description().bit_depth(), input.c_str(),
		                     output.c_str())
		                  .c_str()) == 0;
	}
};



//// VVC Base Codec
//
class FileEncoderImplVVC : public FileEncoderImpl {
public:
	FileEncoderImplVVC(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
	    : FileEncoderImpl(image_description, fps, parameters) {}

	BaseDecoder::Codec es_file_type() const override { return BaseDecoder::VVC; };
	BaseCoding base_coding() const override { return BaseCoding_VVC; }

	bool run_base_encoder(const string &yuv_file, const string &es_file, const string &recon_file,
	                      unsigned frame_count) const override {
		const string prog = get_program_directory("external_codecs/VTM/EncoderApp");
		const string cfg = get_program_directory("external_codecs/VTM/vtm_encoder_randomaccess.cfg");

		const unsigned qp = encoder_.base_qp();
		unsigned chroma_format;
		switch (encoder_.base_image_description().colourspace()) {
		case Colourspace_Y:
			chroma_format = 400;
			break;
		case Colourspace_YUV420:
			chroma_format = 420;
			break;
		case Colourspace_YUV422:
			chroma_format = 422;
			break;
		case Colourspace_YUV444:
			chroma_format = 444;
			break;
		default:
			ERR("Not a colour space");
		}

		unsigned intra_period_vvc = intra_period();
		// since VTM 11, intra period must be multiple of 32 gop period
		if ((intra_period_vvc % 32) != 0)
			intra_period_vvc = round(intra_period_vvc / 32) * 32;
		if (intra_period_vvc < 32)
			intra_period_vvc = 32;

		string cmd_line =
		    format("%s -c %s --InputFile=%s ---BitstreamFile=%s --ReconFile=%s "
		           "--SourceWidth=%d --SourceHeight=%d --InputBitDepth=%d --OutputBitDepth=%d --InternalBitDepth=%d --FrameRate=%d "
		           "--QP=%d --IntraPeriod=%d --FramesToBeEncoded=%d --ConformanceWindowMode=1 --InputChromaFormat=%u",
		           prog.c_str(), cfg.c_str(), yuv_file.c_str(), es_file.c_str(), recon_file.c_str(),
		           encoder_.base_image_description().width(), encoder_.base_image_description().height(),
		           encoder_.base_image_description().bit_depth(), encoder_.base_image_description().bit_depth(),
		           encoder_.base_image_description().bit_depth(), fps_, qp, intra_period_vvc, frame_count, chroma_format);

		INFO("Running: %s", cmd_line.c_str());
		return system(cmd_line.c_str()) == 0;
	}

	// Insert enhancement data NALU
	//
	int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) override {
		ESFile::NalUnit nal_unit;

		if (encapsulation_ == Encapsulation_None || encapsulation_ == Encapsulation_NAL) {
			// Use LCEVC Standard NALU
			nal_unit = build_nalu(is_idr, payload);
		} else if (encapsulation_ == Encapsulation_SEI_Registered) {
			// Encapsulate LCEVC Data into a NAL unit
			ESFile::NalUnit nal_unit_lcevc = build_nalu(is_idr, payload);
			Packet payload_lcevc = Packet::build().contents(nal_unit_lcevc.m_data).finish();

			// Regsitered SEI Encapsulation of LCEVC NAL unit
			const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x00,
			                                static_cast<uint8_t>(0xb8 | (au.m_temporalId + 1))}; // nal_unit_type=23 (SEI)
			nal_unit = {23, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(RegisteredSEI::sei_payload(payload_lcevc));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_SEI_Unregistered) {
			// Unregistered SEI Encapsulation
			const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, 0x00,
			                                static_cast<uint8_t>(0xb8 | (au.m_temporalId + 1))}; // nal_unit_type=23 (SEI)
			nal_unit = {23, DataBuffer(nalu_header, nalu_header + sizeof(nalu_header))};

			Packet rbsp = rbsp_encapsulate(UnregisteredSEI::sei_payload(payload));
			const PacketView v(rbsp);
			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else {
			CHECK(0);
		}

		unsigned offset = 0;
		for (offset = 0; offset < au.m_nalUnits.size(); ++offset) {
			if (au.m_nalUnits[offset].m_type < 12)
				break;
		}

		au.m_nalUnits.insert(au.m_nalUnits.begin() + offset, nal_unit);
		return (int)(nal_unit.m_data.size());
	}

	bool run_base_decoder(const string &input, const string &output) override {
		string p = get_program_directory("external_codecs/VTM/DecoderApp");

		INFO("Using base decoder %s", p.c_str());

		return system(format("\"%s\" -d %u -b %s -o %s", p.c_str(), encoder_.base_image_description().bit_depth(), input.c_str(),
		                     output.c_str())
		                  .c_str()) == 0;
	}
};

//// EVC Base Codec
//
class FileEncoderImplEVC : public FileEncoderImpl {
public:
	FileEncoderImplEVC(const ImageDescription &image_description, unsigned fps, const Parameters &parameters)
	    : FileEncoderImpl(image_description, fps, parameters) {}

	BaseDecoder::Codec es_file_type() const override { return BaseDecoder::EVC; };
	BaseCoding base_coding() const override { return BaseCoding_EVC; }

	bool run_base_encoder(const string &yuv_file, const string &es_file, const string &recon_file,
	                      unsigned frame_count) const override {
		if (encoder_.base_image_description().colourspace() != Colourspace_YUV420)
			ERR("Only encodings with a 4:2:0 colourspace are supported by the current version of ETM.");
		const string prog = get_program_directory("external_codecs/ETM/evca_encoder");
		const string cfg = get_program_directory("external_codecs/ETM/etm_encoder_randomaccess_main.cfg");

		const unsigned qp = encoder_.base_qp();

		string cmd_line = format("%s --config %s --input %s --output %s --recon %s "
		                         "--width %d --height %d --input_bit_depth %d --output_bit_depth %d --hz %d "
		                         "--op_qp %d --iperiod %d --frames %d",
		                         prog.c_str(), cfg.c_str(), yuv_file.c_str(), es_file.c_str(), recon_file.c_str(),
		                         encoder_.base_image_description().width(), encoder_.base_image_description().height(),
		                         encoder_.base_image_description().bit_depth(), encoder_.base_image_description().bit_depth(), fps_,
		                         qp, intra_period(), frame_count);

		INFO("Running: %s", cmd_line.c_str());
		return system(cmd_line.c_str()) == 0;
	}

	// Insert enhancement data NALU
	//
	int add_enhancement(ESFile::AccessUnit &au, bool is_idr, const Packet &payload) override {
		ESFile::NalUnit nal_unit;

		if (encapsulation_ == Encapsulation_None) {
			// No encapsulation - use LCEVC Standard NALU
			nal_unit = build_nalu(is_idr, payload);
		} else if (encapsulation_ == Encapsulation_SEI_Registered) {
			// Encapsulate LCEVC Data into a NAL unit

			ESFile::NalUnit nal_unit_lcevc;
			static const uint8_t nalu_header_lcevc[5] = {0x00, 0x00, 0x01, 0x79, 0xff};
			nal_unit_lcevc = {29, DataBuffer(nalu_header_lcevc, nalu_header_lcevc + sizeof(nalu_header_lcevc))};

			Packet rbsp_lcevc = rbsp_encapsulate(nal_payload(payload));
			const PacketView v_lcevc(rbsp_lcevc);
			nal_unit_lcevc.m_data.insert(nal_unit_lcevc.m_data.end(), v_lcevc.data(), v_lcevc.data() + v_lcevc.size());

			Packet payload_lcevc = Packet::build().contents(nal_unit_lcevc.m_data).finish();

			// Regsitered SEI Encapsulation of LCEVC NAL unit, nal_unit_type_plus1=29 (28 -> SEI)
			const uint8_t nalu_header[2] = {static_cast<uint8_t>(0x3a | (au.m_temporalId >> 2)),
			                                static_cast<uint8_t>((au.m_temporalId << 6) & 255)};

			Packet p = RegisteredSEI::sei_payload(payload_lcevc);
			const PacketView v(p);
			uint32_t payload_length = v.size() + sizeof(nalu_header);
			nal_unit = {27, DataBuffer((uint8_t *)&payload_length, (uint8_t *)&payload_length + sizeof(payload_length))};

			nal_unit.m_data.insert(nal_unit.m_data.end(), nalu_header, nalu_header + sizeof(nalu_header));

			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_SEI_Unregistered) {
			// Unregistered SEI Encapsulation, nal_unit_type_plus1=29 (28 -> SEI)
			const uint8_t nalu_header[2] = {static_cast<uint8_t>(0x3a | (au.m_temporalId >> 2)),
			                                static_cast<uint8_t>((au.m_temporalId << 6) & 255)};

			Packet p = UnregisteredSEI::sei_payload(payload);
			const PacketView v(p);
			uint32_t payload_length = v.size() + sizeof(nalu_header);
			nal_unit = {27, DataBuffer((uint8_t *)&payload_length, (uint8_t *)&payload_length + sizeof(payload_length))};

			nal_unit.m_data.insert(nal_unit.m_data.end(), nalu_header, nalu_header + sizeof(nalu_header));

			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else if (encapsulation_ == Encapsulation_NAL) {
			// NAL Encapsulation, nal_unit_type_plus1=30 (29 -> Reserved)
			const uint8_t nalu_header[2] = {static_cast<uint8_t>(0x3c | (au.m_temporalId >> 2)),
			                                static_cast<uint8_t>((au.m_temporalId << 6) & 255)};

			Packet p = nal_payload(payload);
			const PacketView v(p);

			uint32_t payload_length = v.size() + sizeof(nalu_header);
			nal_unit = {28, DataBuffer((uint8_t *)&payload_length, (uint8_t *)&payload_length + sizeof(payload_length))};

			// NAL Encapsulation
			nal_unit.m_data.insert(nal_unit.m_data.end(), nalu_header, nalu_header + sizeof(nalu_header));

			nal_unit.m_data.insert(nal_unit.m_data.end(), v.data(), v.data() + v.size());
		} else {
			CHECK(0);
		}

		unsigned offset = 0;
		for (offset = 0; offset < au.m_nalUnits.size(); ++offset) {
			if (au.m_nalUnits[offset].m_type < 24 || au.m_nalUnits[offset].m_type > 26)
				break;
		}

		au.m_nalUnits.insert(au.m_nalUnits.begin() + offset, nal_unit);
		return (int)(nal_unit.m_data.size());
	}

	bool run_base_decoder(const string &input, const string &output) override {
		string p = get_program_directory("external_codecs/ETM/evca_decoder");

		INFO("Using base decoder %s", p.c_str());
		return system(format("\"%s\" --output_bit_depth %d --input %s --output %s", p.c_str(),
		                     encoder_.base_image_description().bit_depth(), input.c_str(), output.c_str())
		                  .c_str()) == 0;
	}
};

// Downsample input according to current settings and write to output file
//
void FileEncoderImpl::downsample_file(const string &input_file, const string &output_file, unsigned limit) {
	auto input = CHECK(CreateYUVReader(input_file, encoder_.src_image_description(), fps_));
	auto output = CHECK(CreateYUVWriter(output_file, encoder_.base_image_description(), true));

	const unsigned frame_count = min(input->length(), limit);
	INFO("Downsampling %d frames", frame_count);

	for (unsigned f = 0; f < frame_count; ++f) {
		INFO("  Writing downsampled %d", f);
		const Image src = input->read(f, f);
		const Image img = DownsampleImage(DownsampleImage(src, downsample_luma_, downsample_chroma_, scaling_mode_[LOQ_LEVEL_2]),
		                                  downsample_luma_, downsample_chroma_, scaling_mode_[LOQ_LEVEL_1]);
		output->write(img);
	}
}

// Upsample input according to current settings and write to output file
//
void FileEncoderImpl::upsample_file(const string &input_file, const string &output_file, unsigned limit) {
	auto input = CHECK(CreateYUVReader(input_file, encoder_.base_image_description(), fps_));
	auto output = CHECK(CreateYUVWriter(output_file, encoder_.src_image_description(), true));

	const unsigned frame_count = min(input->length(), limit);
	INFO("Upsampling %d frames", frame_count);

	for (unsigned f = 0; f < frame_count; ++f) {
		INFO("  Writing upsampled %d", f);
		const Image src = input->read(f, f);
		const Image img = UpsampleImage(UpsampleImage(src, upsample_, upsampling_coefficients_, scaling_mode_[LOQ_LEVEL_2]),
		                                upsample_, upsampling_coefficients_, scaling_mode_[LOQ_LEVEL_1]);
		output->write(img);
	}
}

// Factory function
//
unique_ptr<FileEncoder> CreateFileEncoder(BaseCoding base_encoder, const ImageDescription &image_description, unsigned fps,
                                          const Parameters &parameters) {
	try {
		if (base_encoder == BaseCoding_AVC)
			return unique_ptr<FileEncoder>(new FileEncoderImplAVC(image_description, fps, parameters));
		else if (base_encoder == BaseCoding_HEVC)
			return unique_ptr<FileEncoder>(new FileEncoderImplHEVC(image_description, fps, parameters));
		else if (base_encoder == BaseCoding_VVC)
			return unique_ptr<FileEncoder>(new FileEncoderImplVVC(image_description, fps, parameters));
		else if (base_encoder == BaseCoding_EVC)
			return unique_ptr<FileEncoder>(new FileEncoderImplEVC(image_description, fps, parameters));
		else if (base_encoder == BaseCoding_X265)
			return unique_ptr<FileEncoder>(new FileEncoderImplX265(image_description, fps, parameters));
		else
			ERR("Unknown base codec");
	} catch (string e) {
		ERR("CreateFileEncoderImpl: Caught: %s\n", e.c_str());
	}

	return nullptr;
}

} // namespace lctm
