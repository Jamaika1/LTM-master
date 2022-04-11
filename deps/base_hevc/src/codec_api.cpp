//
//
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>

#include "memory.h"

#include "CodecApi.h"
#include "CodecUtils.h"
#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "RingBuffer.hpp"

#include "TLibCommon/TComList.h"

#include "TLibCommon/TComPicYuv.h"

#include "TLibDecoder/TDecTop.h"

#include "TLibDecoder/AnnexBread.h"

#include "TLibDecoder/NALread.h"

#include "TVideoIOYuvMem.h"

using namespace std;

struct Metadata {
	int32_t width;
	int32_t height;
	int32_t planes;
	int32_t bpp;

	uint64_t timestamp;
	uint64_t poc;
	uint32_t qp;

	uint32_t frame_type;
};

struct Error {
	uint32_t code;
	std::string msg;
	std::string file;
	uint32_t line;
};

enum State {
	StateNone = 0,
	StatePushing,
	StateDecoding,
	StateFlushing,
	StateFinished,
};

struct DecodedImage {
	vector<uint8_t> buffer_;
	CodecImage image_;
};

class Context: public TVideoIOYuvMem::Writer  {
public:
	//
	void flush();
	void WriteOutput(TComList<TComPic *> *pcListPic, unsigned int tId);
	void FlushOutput(TComList<TComPic *> *pcListPic);

	// Implement TVideoIOYuvMem::Writer
	void picture_begin(bool is16bit, UInt width, UInt height, ChromaFormat format) override;
	void picture_end() override;
	int write(const uint8_t *data, size_t size) override;

	//
	std::string configuration_;

	State state_;
	stringstream base_bitstream_;

	int frame_count = 0;

	thread decoder_thread_;

	// Base decoder
	TDecTop m_cTDecTop;
	TVideoIOYuvMem m_cTVideoIOYuvRecon;
	Int m_iPOCLastDisplay;

	Int m_outputBitDepth[MAX_NUM_CHANNEL_TYPE];

	unsigned output_frame_ = 0;
//	vector<uint8_t> output_buffer_;

	// shallow ring buffer used to move pictures from decoder to codec api
	RingBuffer<shared_ptr<DecodedImage>> decoded_images_ = {2};

	// Image being assembled by YUV writer callbacks
	shared_ptr<DecodedImage> writing_image_;

	// Image returned to API client
	shared_ptr<DecodedImage> decoded_image_;
};

static int32_t create_context(CodecContext *cp, const char *json_configuration, CodecError *error) {
	if(error)
		*error = 0;

	Context* context = new Context();
	context->configuration_ = json_configuration;
	context->state_ = StatePushing;
	context->m_iPOCLastDisplay = -MAX_INT;
	*cp = (void *)context;

	return 1;
}

// Push encoded data into decoder
//
static int32_t push_packet(CodecContext c, const uint8_t *data, size_t length, CodecMetadata metadata, int8_t eos, CodecError *error) {
	Context *context = (Context *)c;
	if(error)
		*error = 0;

	switch(context->state_) {
	case StatePushing:
		if(!eos) {
			CHECK(data);
			CHECK(length >= 3);
			// Fixup NALU start code - if first is 3 bytes, make it 4
			if (data[0] == '\0' && data[1] == '\0' && data[2] == '\1') {
				char const zero = '\0';
				context->base_bitstream_.put(0);
			}

			// Accumulate AUs into a temporary buffer
			context->base_bitstream_.write((const char *)data, length);
		} else {
			//// Move to decoding
			context->state_ = StateDecoding;
		}
		return 0;

	default:
		ERR("Unexpected decoder state");
	}
	return 0;
}

static int32_t pull_image(CodecContext c, CodecImage* image, CodecMetadata *metadata, int8_t* eos, CodecError *error){
	Context *context = (Context *)c;
	if(error)
		*error = 0;

	switch(context->state_) {
	case StatePushing:
		// Still accumulating input stream
		*eos = 0;
		return 0;

	case StateDecoding:
		context->m_cTDecTop.create();
		context->m_cTDecTop.init();

		context->decoder_thread_ = std::thread([context] {
				context->flush();
				context->decoded_images_.push(nullptr);
			});

		context->state_ = StateFlushing;
		/* Fall through */

	case StateFlushing:
		context->decoded_images_.pop(context->decoded_image_);
		if(context->decoded_image_) {
			*image = context->decoded_image_->image_;
			*eos = 0;
			return 1;
		} else {
			if(context->decoder_thread_.joinable())
				context->decoder_thread_.join();
			*eos = 1;
			return 0;
		}
	default:
		ERR("Unexpected decoder state");
	}
	return 0;
}


void Context::flush() {
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
void Context::WriteOutput(TComList<TComPic *> *pcListPic, unsigned int tId) {
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
void Context::FlushOutput(TComList<TComPic *> *pcListPic) {
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

void Context::picture_begin(bool is16bit, UInt width, UInt height, ChromaFormat format) {
	output_frame_++;
	size_t frame_size = (width * height + 2 * (width / 2 * height / 2)) * (is16bit ? 2 : 1);

	CHECK(!writing_image_);

	writing_image_ = std::shared_ptr<DecodedImage>(new DecodedImage());

	writing_image_->buffer_.reserve(frame_size);

	writing_image_->image_.bpp = is16bit ? 2 : 1;
	writing_image_->image_.width_y = width;
	writing_image_->image_.height_y = height;
	writing_image_->image_.stride_y = width * writing_image_->image_.bpp;

	writing_image_->image_.width_uv = width / 2;
	writing_image_->image_.height_uv = height / 2;
	writing_image_->image_.stride_uv = (width / 2) * writing_image_->image_.bpp;
}

int Context::write(const uint8_t *data, size_t size) {
	CHECK(writing_image_);
	writing_image_->buffer_.insert(writing_image_->buffer_.end(), data, data + size);
	return (int)size;
}

void Context::picture_end() {

	CHECK(writing_image_);
	writing_image_->image_.data_y = writing_image_->buffer_.data();
	writing_image_->image_.data_u = writing_image_->image_.data_y + writing_image_->image_.stride_y * writing_image_->image_.height_y;
	writing_image_->image_.data_v = writing_image_->image_.data_u + writing_image_->image_.stride_uv * writing_image_->image_.height_uv;

	decoded_images_.push(writing_image_);
	writing_image_ = nullptr;
}

static void release_context(CodecContext c) {
	Context *context = (Context *)c;
	if(!context)
		return;

	///
	free(context);
}

CODEC_API_EXPORT uint32_t CodecAPI_Version() {
	return LOADABLE_CODEC_API_VERSION;
}

CODEC_API_EXPORT uint32_t CodecAPI_Query(int num, const char *buffer, uint32_t buffer_size) {
	return 0;
}

static const char codec_name[] = "hevc";
static const char codec_version_string[] = "LTM xxx";

CODEC_API_EXPORT Codec* CodecAPI_Create(const char *name, CodecOperation operation, const char *json_create_options) {
	Codec *codec = LTMCodecAllocate(codec_name, codec_version_string, operation);

	codec->create_context = create_context;
	codec->push_packet = push_packet;
	codec->pull_image = pull_image;
	codec->release_context = release_context;

	return codec;
}

CODEC_API_EXPORT void CodecAPI_Release(Codec *codec) {
	if(!codec)
		return;

	LTMCodecFree(codec);
}
