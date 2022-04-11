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
// BaseVideoDecoder.cpp
//
// Base decoder implementation using external executable to decode base layer
//
#include "BaseVideoDecoder.hpp"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include <cassert>

#include "Config.hpp"
#include "Diagnostics.hpp"
#include "Dimensions.hpp"
#include "Misc.hpp"

#include "Packet.hpp"

#include "Probe.hpp"
#include "ScanEnhancement.hpp"
#include "uESFile.h"
#include "uYUVDesc.h"

#include "BaseVideoDecoderCodecApi.hpp"

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern std::priority_queue<ReportStructure, std::vector<ReportStructure, std::allocator<ReportStructure>>, ReportStructureComp> goReportQueue;

using namespace vnova::utility;

namespace lctm {

namespace {

#define QUEUE_LIMIT 32

static bool compare_packet_timestamps(const lctm::Packet &l, const lctm::Packet &r) {
	uint64_t lt = l.timestamp();
	uint64_t rt = r.timestamp();
	int64_t d = (rt - lt);
	return d < 0;
}

// PSS data that is queued whilst base frames are decoded
typedef struct {
	Packet packet;
	bool is_lcevc_idr;
} PssPacket;

struct BufferCompare {
	bool operator()(const PssPacket &l, const PssPacket &r) { return compare_packet_timestamps(l.packet, r.packet); }
};

} // namespace

// Common code for external base decoders.
//
// Specialised for AVC/HEVC/VVC/EVC
//
class BaseVideoDecoderExternal : public BaseVideoDecoder {
public:
	BaseVideoDecoderExternal(BaseVideoDecoder::Output &output, Encapsulation encapsulation,
	                         const std::string &prepared_yuv_file_name, bool keep_base);
	~BaseVideoDecoderExternal() override;

	void start() override;
	void stop() override;

	void push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) override;

protected:
	virtual BaseCoding base_coding() const = 0;

	virtual bool run_decoder(const string &input, const string &output) const = 0;

	void push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr);
	void flush();

	// Consumer interface
	Output &output_;

	// Type of enhancement payload encapsulation
	Encapsulation encapsulation_;

	// If not-null -name of predecoded YUV base images
	const std::string prepared_yuv_file_name_;

	// Temp. files
	vector<uint8_t> buffer_;

	string es_file_name_;
	FILE *es_file_ = nullptr;

	string yuv_file_name_;

	// Keep base files
	bool keep_base_;

	// Priority queues of buffers for base & pss ordered by timestamp
	priority_queue<PssPacket, vector<PssPacket>, BufferCompare> enhancement_queue_;
	vector<PssPacket> enhancement_vector_;

	virtual void StatisticsComputation() override;

	// Size of base video - initisalized on first decoded base frame
	uint32_t base_buffer_size_ = 0;
	uint32_t base_width_ = 0;
	uint32_t base_height_ = 0;
	uint32_t base_bit_depth_ = 0;
};

BaseVideoDecoderExternal::BaseVideoDecoderExternal(Output &output, Encapsulation encapsulation,
                                                   const std::string &prepared_yuv_file_name, bool keep_base)
    : output_(output), encapsulation_(encapsulation), prepared_yuv_file_name_(prepared_yuv_file_name), keep_base_(keep_base) {}

BaseVideoDecoderExternal::~BaseVideoDecoderExternal() { stop(); }

void BaseVideoDecoderExternal::start() {

	if (base_coding() != BaseCoding::BaseCoding_YUV) {
		// Create a temporary file for base ES
		es_file_name_ = make_temporary_filename("_base.es");
		es_file_ = CHECK(fopen(es_file_name_.c_str(), "wb"));
		INFO("Using temporary file for ES data: '%s'", es_file_name_.c_str());
	}

	if (prepared_yuv_file_name_.empty()) {
		// Create a temporary file for base YUV
		yuv_file_name_ = make_temporary_filename("_base.yuv");

		INFO("Using temporary file for YUV data: '%s'", yuv_file_name_.c_str());
	} else {
		INFO("Using prepared file for YUV data: '%s'", prepared_yuv_file_name_.c_str());
	}
}

static inline bool is_nal_marker(const uint8_t *data) { return data[0] == 0 && data[1] == 0 && data[2] == 1; }

void BaseVideoDecoderExternal::push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) {
	if (data) {
		// Temp. copy of AU
		vector<uint8_t> b(data, data + data_size);

		// Pick out enhancement data - may be removed - new size is returned
		size_t new_size = scan_enhancement(b.data(), data_size, encapsulation_, base_coding(), pts, is_base_idr,
		                                   [this](const Packet &pkt, const bool is_lcevc_idr) {
			                                   PacketView v(pkt);
			                                   this->enhancement_queue_.push({pkt, is_lcevc_idr});
		                                   });

		b.resize(new_size);

		goReportStructure.miTimeStamp = (int)(pts);
		goReportStructure.miPictureType = iPictureType;
		goReportStructure.miBaseSize = (int)(new_size);
		goReportStructure.miEnhancementSize = (int)(data_size - new_size);
		goReportQueue.push(goReportStructure);

		// Flush any previous buffer
		if (!buffer_.empty())
			push_es(buffer_.data(), buffer_.size(), pts, is_base_idr);

		buffer_.swap(b);
	} else {
		if (!buffer_.empty()) {
			push_es(buffer_.data(), buffer_.size(), pts, is_base_idr);
		}

		flush();
	}
}

void BaseVideoDecoderExternal::push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr) {
	// Fixup NALU start code - if first is 3 bytes, make it 4
	if (data[0] == '\0' && data[1] == '\0' && data[2] == '\1') {
		char const zero = '\0';
		CHECK(fwrite(&zero, 1, 1, es_file_) == 1);
	}

	// Accumulate AUs into a temporary file
	CHECK(fwrite(data, 1, data_size, es_file_) == data_size);
}

void BaseVideoDecoderExternal::flush() {

	int iPictureCount = 0;

	if (base_coding() != BaseCoding::BaseCoding_YUV) {
		// Got end of input - finish up ES file
		fflush(es_file_);
		fclose(es_file_);
		es_file_ = nullptr;
	}

	while (!enhancement_queue_.empty()) {
		enhancement_vector_.push_back(std::move(const_cast<PssPacket &>(enhancement_queue_.top())));
		enhancement_queue_.pop();
	}

	if (keep_base_) {
		string es_enhancement_file_name_ = make_temporary_filename("_enhancement.es");
		FILE *es_enhancement_file_ = CHECK(fopen(es_enhancement_file_name_.c_str(), "wb"));
		INFO("Using temporary file for enhancement data: '%s'", es_enhancement_file_name_.c_str());

		for (auto const &element : enhancement_vector_) {
			const uint8_t nal_type = 28 + element.is_lcevc_idr;
			const uint8_t nalu_header[5] = {0x00, 0x00, 0x01, static_cast<uint8_t>(0x41 | (nal_type << 1)), 0xff};
			CHECK(fwrite(&nalu_header, 1, sizeof(nalu_header), es_enhancement_file_) == sizeof(nalu_header));

			Packet rbsp = rbsp_encapsulate(element.packet);
			const PacketView v(rbsp);
			CHECK(fwrite(v.data(), 1, v.size(), es_enhancement_file_) == v.size());
		}

		fflush(es_enhancement_file_);
		fclose(es_enhancement_file_);
		es_enhancement_file_ = nullptr;
	}

	Surface symbols_initial[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];
	PssPacket pss_initial;
	if (!enhancement_vector_.empty()) {
		// Get PSS from queue
		pss_initial = enhancement_vector_.front();
		enhancement_vector_.erase(enhancement_vector_.begin());

		PacketView enhancement_view(pss_initial.packet);
		output_.deserialize_enhancement(enhancement_view.data(), enhancement_view.size(), symbols_initial);
		iPictureCount++;
	} else
		INFO("No LCEVC Data found in bitstream!");

	// Find base size
	//
	unsigned width = 0, height = 0, bit_depth = 0, chroma_format_idc = 0;
	if (prepared_yuv_file_name_.empty()) {
		if (!probe_es_file(es_file_name_, base_coding(), width, height, bit_depth, chroma_format_idc)) {
			ERR("Cannot get base size, depth, and chroma_format_idc");
			return;
		}
		if (iPictureCount > 0) {
			// Verify base dimensions with signalled LCEVC configuration
			Dimensions dimensions = output_.get_dimensions();
			CHECK(bit_depth == output_.get_base_bitdepth());
			CHECK(width == dimensions.base_width());
			CHECK(height == dimensions.base_height());
			CHECK(chroma_format_idc == output_.get_colourspace());
		}
	} else {
		// Get base dimensions from singalled LCEVC configuration
		if (iPictureCount == 0)
			ERR("No LCEVC Data and no base bitstream -> Cannot get width/height/format");
		Dimensions dimensions = output_.get_dimensions();
		width = dimensions.base_width();
		height = dimensions.base_height();
		bit_depth = output_.get_base_bitdepth();
		chroma_format_idc = output_.get_colourspace();
	}

	// Figure ESFile format
	YUVFormat::Enum format;

	switch (bit_depth) {
	case 8:
		format = static_cast<YUVFormat::Enum>(0 + (chroma_format_idc == 0 ? 3 : chroma_format_idc - 1));
		break;

	case 10:
		format = static_cast<YUVFormat::Enum>(4 + (chroma_format_idc == 0 ? 3 : chroma_format_idc - 1));
		break;

	case 12:
		format = static_cast<YUVFormat::Enum>(8 + (chroma_format_idc == 0 ? 3 : chroma_format_idc - 1));
		break;

	case 14:
		format = static_cast<YUVFormat::Enum>(12 + (chroma_format_idc == 0 ? 3 : chroma_format_idc - 1));
		break;
	default:
		CHECK(0);
		break;
	}

	YUVDesc yuv_desc;
	yuv_desc.Initialise(format, width, height);
	size_t base_size = yuv_desc.GetMemorySize();

	const char *fmt = "";
	YUVFormat::ToString(&fmt, format);
	INFO("Base is %s %dx%d %d bit (%zu bytes)\n", fmt, width, height, bit_depth, base_size);

	base_width_ = width;
	base_height_ = height;
	base_bit_depth_ = bit_depth;

	UniquePtrFile yuv_file;

	if (!prepared_yuv_file_name_.empty()) {
		// We have been given decoded YUV already
		yuv_file.reset(CHECK(fopen(prepared_yuv_file_name_.c_str(), "rb")));
	} else {
		// Run the decoder esfile->yuvfile
		//
		run_decoder(es_file_name_, yuv_file_name_);

		// Open temp. file
		yuv_file.reset(CHECK(fopen(yuv_file_name_.c_str(), "rb")));
	}

	// Buffer for the YUV frames
	vector<uint8_t> base(base_size);

	if (iPictureCount > 0) {
		// First frame: Copy data through, already deserialized previously
		CHECK(fread(base.data(), 1, base.size(), yuv_file.get()) == base.size());
		output_.push_base_enhancement(base.data(), base.size(), symbols_initial, pss_initial.packet.timestamp(),
		                              pss_initial.is_lcevc_idr);

		// Consume queue in PTS order
		while (!enhancement_vector_.empty()) {
			// Read base
			CHECK(fread(base.data(), 1, base.size(), yuv_file.get()) == base.size());

			// Get PSS from queue
			Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];
			PssPacket pss = enhancement_vector_.front();
			enhancement_vector_.erase(enhancement_vector_.begin());
			PacketView enhancement_view(pss.packet);
			output_.deserialize_enhancement(enhancement_view.data(), enhancement_view.size(), symbols);
			output_.push_base_enhancement(base.data(), base.size(), symbols, pss.packet.timestamp(), pss.is_lcevc_idr);
			iPictureCount++;
		}

		int iFrames = iPictureCount;

		float fAccMse[3];
		float fPsnr[3];
		for (unsigned plane = 0; plane < yuv_desc.GetPlaneCount(); plane++) {
			fAccMse[plane] = goPsnr.mfAccMse[plane] / iFrames;
			fPsnr[plane] = (float)(10.0f * log10((32767.0f * 32767.0f) / fAccMse[plane]));
		}
		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
		if (yuv_desc.GetPlaneCount() > 1)
			REPORT("PSNR -- YUV %8.4f -- Y %8.4f U %8.4f V %8.4f", (6 * fPsnr[0] + fPsnr[1] + fPsnr[2]) / 8, fPsnr[0], fPsnr[1],
			       fPsnr[2]);
		else
			REPORT("PSNR -- Y %8.4f", fPsnr[0]);
		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
		REPORT("BITS -- base %8d bps -- enha %8d bps ", (goPsnr.miBaseBytes * 8 * /* this->fps_ */ 60) / iFrames,
		       (goPsnr.miEnhancementBytes * 8 * /* this->fps_ */ 60) / iFrames);
		REPORT("========= ========= ========= ========= ========= ========= ========= ========= ");
	}
}

void BaseVideoDecoderExternal::stop() {
	if (!keep_base_) {
		if (!es_file_name_.empty())
			::remove(es_file_name_.c_str());

		if (!yuv_file_name_.empty())
			::remove(yuv_file_name_.c_str());
	}
}

void BaseVideoDecoderExternal::StatisticsComputation() {
}

// RBSP encapsualtion (0b00000000 0b00000000 0b000000xx -> 0b00000000 0b00000000 0b00000011 0b000000xx)
//
Packet BaseVideoDecoder::rbsp_encapsulate(const Packet &src) const {
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

// AVC Base Codec
//
class BaseVideoDecoderExternalAVC : public BaseVideoDecoderExternal {
public:
	BaseVideoDecoderExternalAVC(BaseVideoDecoder::Output &output, Encapsulation encapsulation, const std::string &yuv_file,
	                            bool keep_base)
	    : BaseVideoDecoderExternal(output, encapsulation, yuv_file, keep_base) {}

	BaseCoding base_coding() const override { return BaseCoding_AVC; };

	bool run_decoder(const string &es_file, const string &yuv_file) const override {
		string p = get_program_directory("external_codecs/JM/ldecod");

		INFO("Using base decoder %s", p.c_str());

		int r = system(
		    format("%s -p InputFile=%s -p OutputFile=%s -p WriteUV=0", p.c_str(), es_file.c_str(), yuv_file.c_str()).c_str());

		return r == 0;
	}

	virtual void StatisticsComputation() override{};
};

// HEVC Base Codec
//
class BaseVideoDecoderExternalHEVC : public BaseVideoDecoderExternal {
public:
	BaseVideoDecoderExternalHEVC(BaseVideoDecoder::Output &output, Encapsulation encapsulation, const std::string &yuv_file,
	                             bool keep_base)
	    : BaseVideoDecoderExternal(output, encapsulation, yuv_file, keep_base) {}

	BaseCoding base_coding() const override { return BaseCoding_HEVC; };

	bool run_decoder(const string &es_file, const string &yuv_file) const override {
		string p = get_program_directory("external_codecs/HM/TAppDecoder");
		if (base_bit_depth_ > Bitdepth_10) {
			WARN("Using HM with HIGH_BITDEPTH_SUPPORT enabled.");
			p.append("_HighBitdepth");
		}

		INFO("Using base decoder %s", p.c_str());

		int r = system(format("\"%s\" -b %s -o %s", p.c_str(), es_file.c_str(), yuv_file.c_str()).c_str());

		return r == 0;
	}

	virtual void StatisticsComputation() override{};
};

// VVC Base Codec
//
class BaseVideoDecoderExternalVVC : public BaseVideoDecoderExternal {
public:
	BaseVideoDecoderExternalVVC(BaseVideoDecoder::Output &output, Encapsulation encapsulation, const std::string &yuv_file,
	                            bool keep_base)
	    : BaseVideoDecoderExternal(output, encapsulation, yuv_file, keep_base) {}

	BaseCoding base_coding() const override { return BaseCoding_VVC; };

	bool run_decoder(const string &es_file, const string &yuv_file) const override {
		string p = get_program_directory("external_codecs/VTM/DecoderApp");

		INFO("Using base decoder %s", p.c_str());

		int r = system(format("\"%s\" -b %s -o %s", p.c_str(), es_file.c_str(), yuv_file.c_str()).c_str());

		return r == 0;
	}

	virtual void StatisticsComputation() override{};
};

// EVC Base Codec
//
class BaseVideoDecoderExternalEVC : public BaseVideoDecoderExternal {
public:
	BaseVideoDecoderExternalEVC(BaseVideoDecoder::Output &output, Encapsulation encapsulation, const std::string &yuv_file,
	                            bool keep_base)
	    : BaseVideoDecoderExternal(output, encapsulation, yuv_file, keep_base) {}

	BaseCoding base_coding() const override { return BaseCoding_EVC; };

	bool run_decoder(const string &es_file, const string &yuv_file) const override {
		string p = get_program_directory("external_codecs/ETM/evca_decoder");

		INFO("Using base decoder %s", p.c_str());

		int r = system(format("\"%s\" --input %s --output %s --output_bit_depth %d", p.c_str(), es_file.c_str(), yuv_file.c_str(), base_bit_depth_).c_str());

		return r == 0;
	}

	virtual void StatisticsComputation() override{};
};

// No Base Codec (base YUV file as input)
//
class BaseVideoDecoderExternalYUV : public BaseVideoDecoderExternal {
public:
	BaseVideoDecoderExternalYUV(BaseVideoDecoder::Output &output, Encapsulation encapsulation, const std::string &yuv_file,
	                             bool keep_base)
	    : BaseVideoDecoderExternal(output, encapsulation, yuv_file, keep_base) {}

	BaseCoding base_coding() const override { return BaseCoding_YUV; };

	bool run_decoder(const string &es_file, const string &yuv_file) const override { return true; }

	virtual void StatisticsComputation() override{};
};

// Factory function
//
unique_ptr<BaseVideoDecoder> CreateBaseVideoDecoder(BaseVideoDecoder::Output &output, BaseCoding base, Encapsulation encapsulation,
                                                    bool external, const std::string &yuv_file, bool keep_base) {

	try {
		if (external || keep_base || !yuv_file.empty()) {
			if (base == BaseCoding_AVC)
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalAVC(output, encapsulation, yuv_file, keep_base));
			else if (base == BaseCoding_HEVC)
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalHEVC(output, encapsulation, yuv_file, keep_base));
			else if (base == BaseCoding_VVC)
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalVVC(output, encapsulation, yuv_file, keep_base));
			else if (base == BaseCoding_EVC)
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalEVC(output, encapsulation, yuv_file, keep_base));
			else if (base == BaseCoding_YUV)
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalYUV(output, encapsulation, yuv_file, keep_base));
			else
				ERR("Unknown base");
		} else {
			if (base == BaseCoding_AVC) {
#if defined(LTM_ENABLE_CODECAPI_AVC)
				return CreateBaseVideoDecoderCodecApi(output, encapsulation, base, "avc","","");
#else
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalAVC(output, encapsulation, yuv_file, keep_base));
#endif
			} else if (base == BaseCoding_HEVC) {
#if defined(LTM_ENABLE_CODECAPI_HEVC)
				return CreateBaseVideoDecoderCodecApi(output, encapsulation,base, "hevc","","");
#else
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalHEVC(output, encapsulation, yuv_file, keep_base));
#endif
			} else if (base == BaseCoding_VVC) {
#if defined(LTM_ENABLE_CODECAPI_VVC)
				return CreateBaseVideoDecoderCodecApi(output, encapsulation, base, "vvc","","");
#else
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalVVC(output, encapsulation, yuv_file, keep_base));
#endif
			} else if (base == BaseCoding_EVC) {
#if defined(LTM_ENABLE_CODECAPI_EVC)
				return CreateBaseVideoDecoderCodecApi(output, encapsulation, base, "evc","","");
#else
				return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderExternalEVC(output, encapsulation, yuv_file, keep_base));
#endif
			} else {
				ERR("Unknown base");
			}
		}
	} catch (string e) {
		ERR("CreateBaseVideoDecoder: Caught: %s\n", e.c_str());
	}
	return nullptr;
}
} // namespace lctm
