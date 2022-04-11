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
// BaseVideoDecoderCodecApi.cpp
//
// Base decoder wrapper using utils/include/CodecApi.h to talk to loadable codecs.
//
#include "BaseVideoDecoderCodecApi.hpp"

#include <queue>
#include <vector>

#include "BitstreamStatistic.hpp"
#include "Codec.hpp"
#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "Packet.hpp"
#include "ScanEnhancement.hpp"

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern std::priority_queue<ReportStructure, std::vector<ReportStructure, std::allocator<ReportStructure>>, ReportStructureComp>
    goReportQueue;

namespace lctm {

// PSS data that is queued whilst base frames are decoded
typedef struct {
	Packet packet;
	bool is_lcevc_idr;
} PssPacket;

#define QUEUE_LIMIT 32

static bool compare_packet_timestamps(const lctm::Packet &l, const lctm::Packet &r) {
	uint64_t lt = l.timestamp();
	uint64_t rt = r.timestamp();
	int64_t d = (rt - lt);
	return d < 0;
}

struct BufferCompare {
	bool operator()(const PssPacket &l, const PssPacket &r) { return compare_packet_timestamps(l.packet, r.packet); }
};

class BaseVideoDecoderCodecApi : public BaseVideoDecoder {
public:
	BaseVideoDecoderCodecApi(BaseVideoDecoder::Output &output, Encapsulation encapsulation, BaseCoding base,
	                         const std::string &name, const std::string &selection_options, const std::string &create_options);
	~BaseVideoDecoderCodecApi() override;

	void start() override;
	void stop() override;

	void push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_idr, int iPictureType) override;

	virtual void StatisticsComputation() override;

private:
	void push_es(const uint8_t *data, size_t data_size, uint64_t pts);

	// Consumer interface
	Output &output_;

	// Type of enhancement payload encapsulation
	const Encapsulation encapsulation_;
	const BaseCoding base_;

	// Temp. buffer
	vector<uint8_t> buffer_;

	// Priority queues of buffers for base & pss ordered by timestamp
	priority_queue<PssPacket, vector<PssPacket>, BufferCompare> enhancement_queue_;

	//
	const string name_;
	const string selection_options_;
	const string create_options_;
	Codec *codec_ = nullptr;
	CodecContext context_ = 0;
};

BaseVideoDecoderCodecApi::BaseVideoDecoderCodecApi(BaseVideoDecoder::Output &output, Encapsulation encapsulation, BaseCoding base,
                                                   const std::string &name, const std::string &selection_options,
                                                   const std::string &create_options)
    : output_(output), encapsulation_(encapsulation), base_(base), name_(name), selection_options_(selection_options),
      create_options_(create_options) {}

BaseVideoDecoderCodecApi::~BaseVideoDecoderCodecApi() {}

void BaseVideoDecoderCodecApi::start() {
	// Create base codec
	codec_ = CHECK(CodecCreate(name_, CodecOperation_Decode, selection_options_));

	CodecError error = 0;
	if (!codec_->create_context(&context_, create_options_.c_str(), &error)) {
		ERR("Codec error: %s", CodecErrorToString(codec_, error).c_str());
		error = 0;
	}
}

void BaseVideoDecoderCodecApi::stop() {
	if (context_) {
		CHECK(codec_);
		codec_->release_context(context_);
		context_ = 0;
	}

	if (codec_) {
		CodecRelease(codec_);
		codec_ = 0;
	}
}

void BaseVideoDecoderCodecApi::push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) {
	if (data) {
		// Temp. copy of AU
		vector<uint8_t> b(data, data + data_size);

		// Pick out enhancement data - may be removed - new size is returned
		size_t new_size = scan_enhancement(b.data(), data_size, encapsulation_, base_, pts, is_base_idr,
		                                   [this](const Packet &pkt, const bool is_lcevc_idr) {
			                                   this->enhancement_queue_.push({pkt, is_lcevc_idr});
		                                   });

		b.resize(new_size);

		goReportStructure.miTimeStamp = (int)(pts);
		goReportStructure.miPictureType = iPictureType;
		goReportStructure.miBaseSize = (int)(new_size);
		goReportStructure.miEnhancementSize = (int)(data_size - new_size);
		goReportQueue.push(goReportStructure);

		// Flush any previous buffer
		if (!buffer_.empty()) {
			push_es(buffer_.data(), buffer_.size(), pts);
		}

		buffer_.swap(b);
	} else {
		if (!buffer_.empty()) {
			push_es(buffer_.data(), buffer_.size(), pts);
		}

		push_es(0, 0, pts);
	}
}

void BaseVideoDecoderCodecApi::push_es(const uint8_t *data, size_t data_size, uint64_t pts) {
	CHECK(context_);

	// Push packet into codec
	CodecError error = 0;
	codec_->push_packet(context_, data, data_size, 0, data == nullptr, &error);

	// Pull images while codec continues to return them
	int32_t n = 0;
	int8_t eos = 0;
	do {
		CodecImage codec_image = {0};
		CodecError error = 0;
		n = codec_->pull_image(context_, &codec_image, 0, &eos, &error);
		if (n) {
			// Get next PSS from queue
			//
			CHECK(!enhancement_queue_.empty());
			PssPacket pss = enhancement_queue_.top();
			enhancement_queue_.pop();

			// Send to enhancement decoder
			//
			PacketView enhancement_view(pss.packet);
			output_.push_base_enhancement(&codec_image, enhancement_view.data(), enhancement_view.size(), pss.packet.timestamp(),
			                              pss.is_lcevc_idr);
		}
	} while (n);
}

void BaseVideoDecoderCodecApi::StatisticsComputation() {}

// Factory function
//
unique_ptr<BaseVideoDecoder> CreateBaseVideoDecoderCodecApi(BaseVideoDecoder::Output &output, Encapsulation encapsulation,
                                                            BaseCoding base, const std::string &name,
                                                            const std::string &selection_options,
                                                            const std::string &create_options) {
	return unique_ptr<BaseVideoDecoder>(
	    new BaseVideoDecoderCodecApi(output, encapsulation, base, name, selection_options, create_options));
}
} // namespace lctm
