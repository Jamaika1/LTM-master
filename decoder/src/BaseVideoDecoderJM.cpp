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
// BaseVideoDecoderJM.cpp
//
// Base decoder implementation using JM Test Model
//
#include "BaseVideoDecoder.hpp"

#include <queue>
#include <vector>

#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "Packet.hpp"
#include "ScanEnhancement.hpp"
#include "BitstreamStatistic.hpp"
#include "Codec.hpp"

extern "C" {

#include "h264decoder.h"

#include "inject_annexb.h"

#include "configfile.h"

extern DecoderParams *p_Dec;
}

using namespace std;

// bitstream statistics
#include "BitstreamStatistic.hpp"
extern PsnrStatistic goPsnr;
extern uint8_t gaucMd5Digest[][16];
extern ReportStructure goReportStructure;
extern std::priority_queue<ReportStructure, std::vector<ReportStructure, std::allocator<ReportStructure>>, ReportStructureComp> goReportQueue;

namespace lctm {

// PSS data that is queued whilst base frames are decoded
typedef struct {
	Packet packet;
	bool is_base_idr;
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

class BaseVideoDecoderJM : public BaseVideoDecoder {
public:
	BaseVideoDecoderJM(BaseVideoDecoder::Output &output, Encapsulation encapsulation);
	~BaseVideoDecoderJM() override;

	void start() override;
	void stop() override;

	void push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) override;

	virtual void StatisticsComputation() override;

private:
	void push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr);
	void flush();

	void decode_enhanced_frames(DecodedPicList *pic_list, bool all_frames);

	// Consumer interface
	Output &output_;

	// Type of enhancement payload encapsulation
	Encapsulation encapsulation_;

	// Temp. buffer
	vector<uint8_t> buffer_;

	// Accumulated AUs for base
	vector<uint8_t> base_bitstream_;

	// Priority queues of buffers for base & pss ordered by timestamp
	priority_queue<PssPacket, vector<PssPacket>, BufferCompare> enhancement_queue_;

	//
	Codec* codec_ = nullptr;
	CodecContext context_ =0;
};

BaseVideoDecoderJM::BaseVideoDecoderJM(BaseVideoDecoder::Output &output, Encapsulation encapsulation)
    : output_(output), encapsulation_(encapsulation) {}

BaseVideoDecoderJM::~BaseVideoDecoderJM() {}


void BaseVideoDecoderJM::start() {
	// Create base codec
	codec_ = CHECK(CodecCreate("avc", CodecOperation_Decode, ""));

	CodecError error=0;
	if(!codec_->create_context(&context_, "", &error)) {
		ERR("Codec error: %s", CodecErrorToString(codec_, error).c_str());
		error = 0;
	}
}

void BaseVideoDecoderJM::stop() {
	if(context_) {
		CHECK(codec_);
		codec_->release_context(context_);
		context_ = 0;
	}

	if(codec_) {
		CodecRelease(codec_);
		codec_ = 0;
	}
}

void BaseVideoDecoderJM::push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) {
	if (data) {
		// Temp. copy of AU
		vector<uint8_t> b(data, data + data_size);

		// Pick out enhancement data - may be removed - new size is returned
		size_t new_size =
		    scan_enhancement(b.data(), data_size, encapsulation_, BaseCoding_AVC, pts, [is_base_idr, this](const Packet &pkt) {
			    this->enhancement_queue_.push({pkt, is_base_idr});
		    });

		b.resize(new_size);

		goReportStructure.miTimeStamp = (int)(pts);
		goReportStructure.miPictureType = iPictureType;
		goReportStructure.miBaseSize = (int)(new_size);
		goReportStructure.miEnhancementSize = (int)(data_size - new_size);
		goReportQueue.push(goReportStructure);

		// Flush any previous buffer
		if (!buffer_.empty()) {
			push_es(buffer_.data(), buffer_.size(), pts, is_base_idr);
		}

		buffer_.swap(b);
	} else {
		// this comment is specifically to trigger the pipeline (and for Florian) and serves no other purpose
		if (!buffer_.empty()) {
			push_es(buffer_.data(), buffer_.size(), pts, is_base_idr);
		}

		push_es(0, 0, pts, is_base_idr);
	}
}

void BaseVideoDecoderJM::push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr) {
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

		if(n)  {
			// Get next PSS from queue
			//
			CHECK(!enhancement_queue_.empty());
			PssPacket pss = enhancement_queue_.top();
			enhancement_queue_.pop();

			// Send to enhancement decoder
			//
			PacketView enhancement_view(pss.packet);
			output_.push_base_enhancement(&codec_image, enhancement_view.data(), enhancement_view.size(), pss.packet.timestamp(),
										  pss.is_base_idr);
		}
	} while(n);
}

void BaseVideoDecoderJM::flush() {

	// Poke bitstream into our JM hooks
	inject_annex_b_bitstream(base_bitstream_.data(), (unsigned)base_bitstream_.size());

	InputParameters input_parameters;
	memset(&input_parameters, 0, sizeof(input_parameters));
	strcpy(input_parameters.infile, "");
	strcpy(input_parameters.outfile, "");
	strcpy(input_parameters.reffile, "");

	const char *const decoder_args[] = {"ldecod"};
	ParseCommand(&input_parameters, ARRAY_SIZE(decoder_args), (char **)decoder_args);

	// Open decoder
	if (OpenDecoder(&input_parameters) != DEC_OPEN_NOERR) {
		ERR("Cannot open base decoder");
	}

	p_Dec->p_Vid->p_out = -2;

	// Run decoder
	//
	DecodedPicList *pDecPicList = 0;
	int frame_count = 0;

	int r = 0;
	do {
		r = DecodeOneFrame(&pDecPicList);

		if (r == DEC_EOS || r == DEC_SUCCEED) {
			decode_enhanced_frames(pDecPicList, false);
			frame_count++;
		} else {
			ERR("Error in decoding process: 0x%x", r);
		}
	} while ((r == DEC_SUCCEED) && ((p_Dec->p_Inp->iDecFrmNum == 0) || (frame_count < p_Dec->p_Inp->iDecFrmNum)));

	FinitDecoder(&pDecPicList);

	// Write last frame
	decode_enhanced_frames(pDecPicList, true);

	CloseDecoder();
}

void BaseVideoDecoderJM::decode_enhanced_frames(DecodedPicList *pic, bool all_frames) {
	// Check picture and plane pointers - valid flag seems to be wrong sometimes!?
	if (!pic || !pic->pY || !pic->pU || !pic->pV) {
		return;
	}

	if (!pic->bValid) {
		INFO("Picture not marked valid?");
	}

	do {
		BasePicture base_picture = {0};
		base_picture.bpp = ((pic->iBitDepth + 7) >> 3);
		base_picture.width_y = pic->iWidth;
		base_picture.height_y = pic->iHeight;
		base_picture.stride_y = pic->iYBufStride;

		base_picture.width_uv = (pic->iYUVFormat != YUV444) ? (pic->iWidth / 2) : pic->iWidth;
		base_picture.height_uv = (pic->iYUVFormat == YUV420) ? (pic->iHeight / 2) : pic->iHeight;
		base_picture.stride_uv = pic->iUVBufStride;

		base_picture.data_y = pic->pY;
		base_picture.data_u = pic->pU;
		base_picture.data_v = pic->pV;

		// Get next PSS from queue
		//
		CHECK(!enhancement_queue_.empty());
		PssPacket pss = enhancement_queue_.top();
		enhancement_queue_.pop();

		// Send to enhancement decoder
		//
		PacketView enhancement_view(pss.packet);
		output_.push_base_enhancement(&base_picture, enhancement_view.data(), enhancement_view.size(), pss.packet.timestamp(),
		                              pss.is_base_idr);

		// Mark as used and move to next
		pic->bValid = 0;
		pic = pic->pNext;
	} while (pic != NULL && pic->bValid && all_frames);
}

void BaseVideoDecoderJM::StatisticsComputation() {
}

// Factory function
//
unique_ptr<BaseVideoDecoder> CreateBaseVideoDecoderJM(BaseVideoDecoder::Output &output, Encapsulation encapsulation) {
	return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderJM(output, encapsulation));
}
} // namespace lctm
