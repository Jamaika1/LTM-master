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
// BaseVideoDecoderHM.cpp
//
// Base decoder implementation using HM Test Model
//
#include "BaseVideoDecoder.hpp"

#include <queue>
#include <sstream>
#include <vector>

#include "Diagnostics.hpp"
#include "Packet.hpp"
#include "ScanEnhancement.hpp"
#include "BitstreamStatistic.hpp"

#include "TLibCommon/TComList.h"

#include "TLibCommon/TComPicYuv.h"

#include "TLibDecoder/TDecTop.h"

#include "TLibDecoder/AnnexBread.h"

#include "TLibDecoder/NALread.h"

#include "TVideoIOYuvMem.h"

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

class BaseVideoDecoderHM : public BaseVideoDecoder, public TVideoIOYuvMem::Writer {
public:
	BaseVideoDecoderHM(BaseVideoDecoder::Output &output, Encapsulation encapsulation);
	~BaseVideoDecoderHM() override;

	void start() override;
	void stop() override;

	void push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) override;

	virtual void StatisticsComputation() override;

private:
	void push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr);
	void flush();

	void WriteOutput(TComList<TComPic *> *pcListPic, unsigned int tId);
	void FlushOutput(TComList<TComPic *> *pcListPic);

	// Implement TVideoIOYuvMem::Writer
	void picture_begin(bool is16bit, UInt width, UInt height, ChromaFormat format) override;
	void picture_end() override;
	int write(const uint8_t *data, size_t size) override;

	// Consumer interface
	Output &output_;

	// Type of enhancement payload encapsulation
	Encapsulation encapsulation_;

	// Temp. data
	vector<uint8_t> buffer_;

	// Accumulated AUs for base
	stringstream base_bitstream_;

	// Priority queues of buffers for base & pss ordered by timestamp
	priority_queue<PssPacket, vector<PssPacket>, BufferCompare> enhancement_queue_;

	// Base decoder
	TDecTop m_cTDecTop;
	TVideoIOYuvMem m_cTVideoIOYuvRecon;
	Int m_iPOCLastDisplay;

	Int m_outputBitDepth[MAX_NUM_CHANNEL_TYPE];

	unsigned output_frame_ = 0;
	vector<uint8_t> output_buffer_;
	BasePicture base_picture_;
};

BaseVideoDecoderHM::BaseVideoDecoderHM(BaseVideoDecoder::Output &output, Encapsulation encapsulation)
    : output_(output), encapsulation_(encapsulation), m_iPOCLastDisplay(-MAX_INT) {}

BaseVideoDecoderHM::~BaseVideoDecoderHM() {}

void BaseVideoDecoderHM::start() {
	m_cTDecTop.create();
	m_cTDecTop.init();
}

void BaseVideoDecoderHM::stop() { m_cTDecTop.destroy(); }

void BaseVideoDecoderHM::push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) {
	if (data) {
		// Temp. copy of AU
		vector<uint8_t> b(data, data + data_size);

		// Pick out enhancement data - may be removed - new size is returned
		size_t new_size =
		    scan_enhancement(b.data(), data_size, encapsulation_, BaseCoding_AVC, pts, [is_base_idr, this](const Packet &pkt) {
			    this->enhancement_queue_.push({pkt, is_base_idr});
		    });

		goReportStructure.miTimeStamp = (int)(pts);
		goReportStructure.miPictureType = iPictureType;
		goReportStructure.miBaseSize = (int)(new_size);
		goReportStructure.miEnhancementSize = (int)(data_size - new_size);
		goReportQueue.push(goReportStructure);

		b.resize(new_size);

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

void BaseVideoDecoderHM::push_es(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr) {

	// Fixup NALU start code - if first is 3 bytes, make it 4
	if (data[0] == '\0' && data[1] == '\0' && data[2] == '\1') {
		char const zero = '\0';
		base_bitstream_.put(0);
	}

	// Accumulate AUs into a temporary file
	base_bitstream_.write((const char *)data, data_size);
}

void BaseVideoDecoderHM::flush() {
	Bool openedRecon = false;
	bool loopFiltered = false;
	int poc = 0;
	TComList<TComPic *> *pcListPic = NULL;
	InputByteStream bytestream(base_bitstream_);

	while (!!base_bitstream_) {
		streampos location = base_bitstream_.tellg();

		AnnexBStats stats = AnnexBStats();
		InputNALUnit nalu;
		byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);

		// call actual decoding function
		Bool bNewPicture = false;
		if (nalu.getBitstream().getFifo().empty()) {
			WARN("Attempt to decode an empty NAL unit");
		} else {
			read(nalu);
			int skip = 0;
			bNewPicture = m_cTDecTop.decode(nalu, skip, m_iPOCLastDisplay);

			if (bNewPicture) {
				base_bitstream_.clear();
				base_bitstream_.seekg(location - streamoff(3));
				bytestream.reset();
			}
		}

		if ((bNewPicture || !base_bitstream_ || nalu.m_nalUnitType == NAL_UNIT_EOS) && !m_cTDecTop.getFirstSliceInSequence()) {
			if (!loopFiltered || base_bitstream_) {
				m_cTDecTop.executeLoopFilters(poc, pcListPic);
			}
			loopFiltered = (nalu.m_nalUnitType == NAL_UNIT_EOS);
			if (nalu.m_nalUnitType == NAL_UNIT_EOS) {
				m_cTDecTop.setFirstSliceInSequence(true);
			}
		} else if ((bNewPicture || !base_bitstream_ || nalu.m_nalUnitType == NAL_UNIT_EOS) &&
		           m_cTDecTop.getFirstSliceInSequence()) {
			m_cTDecTop.setFirstSliceInPicture(true);
		}

		if (pcListPic) {
			if (!openedRecon) {
				const BitDepths &bitDepths =
				    pcListPic->front()->getPicSym()->getSPS().getBitDepths(); // use bit depths of first reconstructed picture.
				for (UInt channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++) {
					if (m_outputBitDepth[channelType] == 0) {
						m_outputBitDepth[channelType] = bitDepths.recon[channelType];
					}
				}

				m_cTVideoIOYuvRecon.open(this, m_outputBitDepth, m_outputBitDepth, bitDepths.recon); // write mode
				openedRecon = true;
			}

			// write reconstruction to file
			if (bNewPicture) {
				WriteOutput(pcListPic, nalu.m_temporalId);
			}
			if ((bNewPicture || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) && m_cTDecTop.getNoOutputPriorPicsFlag()) {
				m_cTDecTop.checkNoOutputPriorPics(pcListPic);
				m_cTDecTop.setNoOutputPriorPicsFlag(false);
			}
			if (bNewPicture &&
			    (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
			     nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_N_LP || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_RADL ||
			     nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA_W_LP)) {
				FlushOutput(pcListPic);
			}
			if (nalu.m_nalUnitType == NAL_UNIT_EOS) {
				WriteOutput(pcListPic, nalu.m_temporalId);
				m_cTDecTop.setFirstSliceInPicture(false);
			}
			// write reconstruction to file -- for additional bumping as defined in C.5.2.3
			if (!bNewPicture && nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_TRAIL_N &&
			    nalu.m_nalUnitType <= NAL_UNIT_RESERVED_VCL31) {
				WriteOutput(pcListPic, nalu.m_temporalId);
			}
		}
	}

	FlushOutput(pcListPic);

	// delete buffers
	m_cTDecTop.deletePicBuffer();
}

/** \param pcListPic list of pictures to be written to file
    \param tId       temporal sub-layer ID
 */
void BaseVideoDecoderHM::WriteOutput(TComList<TComPic *> *pcListPic, unsigned int tId) {
	if (pcListPic->empty()) {
		return;
	}

	TComList<TComPic *>::iterator iterPic = pcListPic->begin();
	Int numPicsNotYetDisplayed = 0;
	Int dpbFullness = 0;
	const TComSPS *activeSPS = &(pcListPic->front()->getPicSym()->getSPS());
	UInt numReorderPicsHighestTid;
	UInt maxDecPicBufferingHighestTid;
	UInt maxNrSublayers = activeSPS->getMaxTLayers();

	numReorderPicsHighestTid = activeSPS->getNumReorderPics(maxNrSublayers - 1);
	maxDecPicBufferingHighestTid = activeSPS->getMaxDecPicBuffering(maxNrSublayers - 1);

	while (iterPic != pcListPic->end()) {
		TComPic *pcPic = *(iterPic);
		if (pcPic->getOutputMark() && pcPic->getPOC() > m_iPOCLastDisplay) {
			numPicsNotYetDisplayed++;
			dpbFullness++;
		} else if (pcPic->getSlice(0)->isReferenced()) {
			dpbFullness++;
		}
		iterPic++;
	}

	iterPic = pcListPic->begin();

	if (numPicsNotYetDisplayed > 2) {
		iterPic++;
	}

	TComPic *pcPic = *(iterPic);
	if (numPicsNotYetDisplayed > 2 && pcPic->isField()) // Field Decoding
	{
		TComList<TComPic *>::iterator endPic = pcListPic->end();
		endPic--;
		iterPic = pcListPic->begin();
		while (iterPic != endPic) {
			TComPic *pcPicTop = *(iterPic);
			iterPic++;
			TComPic *pcPicBottom = *(iterPic);

			if (pcPicTop->getOutputMark() && pcPicBottom->getOutputMark() &&
			    (numPicsNotYetDisplayed > numReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid) &&
			    (!(pcPicTop->getPOC() % 2) && pcPicBottom->getPOC() == pcPicTop->getPOC() + 1) &&
			    (pcPicTop->getPOC() == m_iPOCLastDisplay + 1 || m_iPOCLastDisplay < 0)) {
				// write to file
				numPicsNotYetDisplayed = numPicsNotYetDisplayed - 2;
				const Window &conf = pcPicTop->getConformanceWindow();
				const Window defDisp = pcPicTop->getDefDisplayWindow();
				const Bool isTff = pcPicTop->isTopField();

				m_cTVideoIOYuvRecon.write(pcPicTop->getPicYuvRec(), pcPicBottom->getPicYuvRec(),
				                          IPCOLOURSPACE_UNCHANGED /*m_outputColourSpaceConvert*/,
				                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
				                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
				                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
				                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT, isTff);

				// update POC of display order
				m_iPOCLastDisplay = pcPicBottom->getPOC();

				// erase non-referenced picture in the reference picture list after display
				if (!pcPicTop->getSlice(0)->isReferenced() && pcPicTop->getReconMark() == true) {
					pcPicTop->setReconMark(false);

					// mark it should be extended later
					pcPicTop->getPicYuvRec()->setBorderExtension(false);
				}
				if (!pcPicBottom->getSlice(0)->isReferenced() && pcPicBottom->getReconMark() == true) {
					pcPicBottom->setReconMark(false);

					// mark it should be extended later
					pcPicBottom->getPicYuvRec()->setBorderExtension(false);
				}
				pcPicTop->setOutputMark(false);
				pcPicBottom->setOutputMark(false);
			}
		}
	} else if (!pcPic->isField()) // Frame Decoding
	{
		iterPic = pcListPic->begin();

		while (iterPic != pcListPic->end()) {
			pcPic = *(iterPic);

			if (pcPic->getOutputMark() && pcPic->getPOC() > m_iPOCLastDisplay &&
			    (numPicsNotYetDisplayed > numReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid)) {
				// write to file
				numPicsNotYetDisplayed--;
				if (pcPic->getSlice(0)->isReferenced() == false) {
					dpbFullness--;
				}

				const Window &conf = pcPic->getConformanceWindow();
				const Window defDisp = true /*m_respectDefDispWindow*/ ? pcPic->getDefDisplayWindow() : Window();

				m_cTVideoIOYuvRecon.write(pcPic->getPicYuvRec(), IPCOLOURSPACE_UNCHANGED /*m_outputColourSpaceConvert*/,
				                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
				                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
				                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
				                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT,
				                          false /*m_bClipOutputVideoToRec709Range */);

				// update POC of display order
				m_iPOCLastDisplay = pcPic->getPOC();

				// erase non-referenced picture in the reference picture list after display
				if (!pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true) {
					pcPic->setReconMark(false);

					// mark it should be extended later
					pcPic->getPicYuvRec()->setBorderExtension(false);
				}
				pcPic->setOutputMark(false);
			}

			iterPic++;
		}
	}
}

/** \param pcListPic list of pictures to be written to file
 */
void BaseVideoDecoderHM::FlushOutput(TComList<TComPic *> *pcListPic) {
	if (!pcListPic || pcListPic->empty()) {
		return;
	}
	TComList<TComPic *>::iterator iterPic = pcListPic->begin();

	iterPic = pcListPic->begin();
	TComPic *pcPic = *(iterPic);

	if (pcPic->isField()) // Field Decoding
	{
		TComList<TComPic *>::iterator endPic = pcListPic->end();
		endPic--;
		TComPic *pcPicTop, *pcPicBottom = NULL;
		while (iterPic != endPic) {
			pcPicTop = *(iterPic);
			iterPic++;
			pcPicBottom = *(iterPic);

			if (pcPicTop->getOutputMark() && pcPicBottom->getOutputMark() && !(pcPicTop->getPOC() % 2) &&
			    (pcPicBottom->getPOC() == pcPicTop->getPOC() + 1)) {
				// write to file
				const Window &conf = pcPicTop->getConformanceWindow();
				const Window defDisp = true /*m_respectDefDispWindow*/ ? pcPicTop->getDefDisplayWindow() : Window();
				const Bool isTff = pcPicTop->isTopField();
				m_cTVideoIOYuvRecon.write(pcPicTop->getPicYuvRec(), pcPicBottom->getPicYuvRec(),
				                          IPCOLOURSPACE_UNCHANGED /*m_outputColourSpaceConvert*/,
				                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
				                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
				                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
				                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT, isTff);

				// update POC of display order
				m_iPOCLastDisplay = pcPicBottom->getPOC();

				// erase non-referenced picture in the reference picture list after display
				if (!pcPicTop->getSlice(0)->isReferenced() && pcPicTop->getReconMark() == true) {
					pcPicTop->setReconMark(false);

					// mark it should be extended later
					pcPicTop->getPicYuvRec()->setBorderExtension(false);
				}
				if (!pcPicBottom->getSlice(0)->isReferenced() && pcPicBottom->getReconMark() == true) {
					pcPicBottom->setReconMark(false);

					// mark it should be extended later
					pcPicBottom->getPicYuvRec()->setBorderExtension(false);
				}
				pcPicTop->setOutputMark(false);
				pcPicBottom->setOutputMark(false);

				if (pcPicTop) {
					pcPicTop->destroy();
					delete pcPicTop;
					pcPicTop = NULL;
				}
			}
		}
		if (pcPicBottom) {
			pcPicBottom->destroy();
			delete pcPicBottom;
			pcPicBottom = NULL;
		}
	} else // Frame decoding
	{
		while (iterPic != pcListPic->end()) {
			pcPic = *(iterPic);

			if (pcPic->getOutputMark()) {
				// write to file
				const Window &conf = pcPic->getConformanceWindow();
				const Window defDisp = true /*m_respectDefDispWindow*/ ? pcPic->getDefDisplayWindow() : Window();

				m_cTVideoIOYuvRecon.write(pcPic->getPicYuvRec(), IPCOLOURSPACE_UNCHANGED /*m_outputColourSpaceConvert*/,
				                          conf.getWindowLeftOffset() + defDisp.getWindowLeftOffset(),
				                          conf.getWindowRightOffset() + defDisp.getWindowRightOffset(),
				                          conf.getWindowTopOffset() + defDisp.getWindowTopOffset(),
				                          conf.getWindowBottomOffset() + defDisp.getWindowBottomOffset(), NUM_CHROMA_FORMAT,
				                          false /*m_bClipOutputVideoToRec709Range*/);

				// update POC of display order
				m_iPOCLastDisplay = pcPic->getPOC();

				// erase non-referenced picture in the reference picture list after display
				if (!pcPic->getSlice(0)->isReferenced() && pcPic->getReconMark() == true) {
					pcPic->setReconMark(false);

					// mark it should be extended later
					pcPic->getPicYuvRec()->setBorderExtension(false);
				}
				pcPic->setOutputMark(false);
			}
			if (pcPic != NULL) {
				pcPic->destroy();
				delete pcPic;
				pcPic = NULL;
			}
			iterPic++;
		}
	}
	pcListPic->clear();
	m_iPOCLastDisplay = -MAX_INT;
}

void BaseVideoDecoderHM::picture_begin(bool is16bit, UInt width, UInt height, ChromaFormat format) {
	output_frame_++;
	size_t frame_size = (width * height + 2 * (width / 2 * height / 2)) * (is16bit ? 2 : 1);

	output_buffer_.clear();
	output_buffer_.reserve(frame_size);

	base_picture_.bpp = is16bit ? 2 : 1;
	base_picture_.width_y = width;
	base_picture_.height_y = height;
	base_picture_.stride_y = width * base_picture_.bpp;

	base_picture_.width_uv = (format != CHROMA_444) ? (width / 2) : width;
	base_picture_.height_uv = (format == CHROMA_420) ? (height / 2) : height;
	base_picture_.stride_uv = base_picture_.width_uv * base_picture_.bpp;
}

int BaseVideoDecoderHM::write(const uint8_t *data, size_t size) {
	output_buffer_.insert(output_buffer_.end(), data, data + size);
	return (int)size;
}

void BaseVideoDecoderHM::picture_end() {
	base_picture_.data_y = output_buffer_.data();
	base_picture_.data_u = base_picture_.data_y + base_picture_.stride_y * base_picture_.height_y;
	base_picture_.data_v = base_picture_.data_u + base_picture_.stride_uv * base_picture_.height_uv;

	// Get next PSS from queue
	//
	CHECK(!enhancement_queue_.empty());
	PssPacket pss = enhancement_queue_.top();
	enhancement_queue_.pop();

	// Send to enhancement decoder
	//
	PacketView enhancement_view(pss.packet);
	output_.push_base_enhancement(&base_picture_, enhancement_view.data(), enhancement_view.size(), pss.packet.timestamp(),
	                              pss.is_base_idr);
}

void BaseVideoDecoderHM::StatisticsComputation() {
}

// Factory function
//
unique_ptr<BaseVideoDecoder> CreateBaseVideoDecoderHM(BaseVideoDecoder::Output &output, Encapsulation encapsulation) {
	return unique_ptr<BaseVideoDecoder>(new BaseVideoDecoderHM(output, encapsulation));
}
} // namespace lctm
