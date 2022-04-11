//
//
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "memory.h"

#include "CodecApi.h"
#include "CodecUtils.h"
#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "RingBuffer.hpp"

extern "C" {
#include "h264decoder.h"
#include "inject_annexb.h"
#include "configfile.h"

extern DecoderParams *p_Dec;
}

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

class Context {
public:
	void flush();
	void decode_enhanced_frames(DecodedPicList *pic, bool all_frames);

	std::string configuration_;

	State state_;
	vector<uint8_t> base_bitstream_;

	int frame_count_ = 0;

	thread decoder_thread_;

	unsigned output_frame_ = 0;

	// shallow ring buffer used to move pictures from docoder to codec api
	RingBuffer<shared_ptr<DecodedImage>> decoded_images_ = {2};

	// Image returned to API client
	shared_ptr<DecodedImage> decoded_image_;
};

static int32_t create_context(CodecContext *cp, const char *json_configuration, CodecError *error) {
	if (error)
		*error = 0;

	Context *context = new Context();
	context->configuration_ = json_configuration;
	context->state_ = StatePushing;
	*cp = (void *)context;

	return 1;
}

// Push encoded data into decoder
//
// To avoid messing with JM any more than necessary, this cheats a bit and saves the entire input bitstream
// until EOS, then starts the JM decoder with the whole stream.
//
static int32_t push_packet(CodecContext c, const uint8_t *data, size_t length, CodecMetadata metadata, int8_t eos,
                           CodecError *error) {
	Context *context = (Context *)c;
	if (error)
		*error = 0;

	switch (context->state_) {
	case StatePushing:
		if (!eos) {
			CHECK(data);
			CHECK(length >= 3);
			// Fixup NALU start code - if first is 3 bytes, make it 4
			if (data[0] == '\0' && data[1] == '\0' && data[2] == '\1') {
				char const zero = '\0';
				context->base_bitstream_.push_back(0);
			}

			// Accumulate AUs into a temporary buffer
			context->base_bitstream_.insert(context->base_bitstream_.end(), data, data + length);
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

static bool extract_image(DecodedPicList *pic, CodecImage *codec_image) {

	// Is there a picture?
	if (!pic || !pic->pY || !pic->pU || !pic->pV) {
		return false;
	}

	if (!pic->bValid) {
		INFO("Picture not marked valid?");
		return false;
	}

	CHECK(codec_image);
	codec_image->bpp = ((pic->iBitDepth + 7) >> 3);
	codec_image->width_y = pic->iWidth;
	codec_image->height_y = pic->iHeight;
	codec_image->stride_y = pic->iYBufStride;

	codec_image->width_uv = (pic->iYUVFormat != YUV444) ? (pic->iWidth / 2) : pic->iWidth;
	codec_image->height_uv = (pic->iYUVFormat == YUV420) ? (pic->iHeight / 2) : pic->iHeight;
	codec_image->stride_uv = pic->iUVBufStride;

	codec_image->data_y = pic->pY;
	codec_image->data_u = pic->pU;
	codec_image->data_v = pic->pV;

	return true;
}

static int32_t pull_image(CodecContext c, CodecImage *image, CodecMetadata *metadata, int8_t *eos, CodecError *error) {
	Context *context = (Context *)c;
	if (error)
		*error = 0;

	switch (context->state_) {
	case StatePushing:
		// Still accumulating input stream
		*eos = 0;
		return 0;

	case StateDecoding:
		context->decoder_thread_ = std::thread([context] {
			context->flush();
			context->decoded_images_.push(nullptr);
		});

		context->state_ = StateFlushing;
		/* Fall through */

	case StateFlushing:
		context->decoded_images_.pop(context->decoded_image_);
		if (context->decoded_image_) {
			*image = context->decoded_image_->image_;
			*eos = 0;
			return 1;
		} else {
			if (context->decoder_thread_.joinable())
				context->decoder_thread_.join();
			*eos = 1;
			return 0;
		}
	default:
		ERR("Unexpected decoder state");
	}

	return 0;
}

//
//
void Context::flush() {

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

//
//
void Context::decode_enhanced_frames(DecodedPicList *pic, bool all_frames) {
	// Check picture and plane pointers - valid flag seems to be wrong sometimes!?
	if (!pic || !pic->pY || !pic->pU || !pic->pV) {
		return;
	}

	if (!pic->bValid) {
		INFO("Picture not marked valid?");
	}

	do {
		auto output_image = std::shared_ptr<DecodedImage>(new DecodedImage());

		output_image->image_.bpp = ((pic->iBitDepth + 7) >> 3);
		output_image->image_.width_y = pic->iWidth;
		output_image->image_.height_y = pic->iHeight;
		output_image->image_.stride_y = pic->iYBufStride;

		output_image->image_.width_uv = (pic->iYUVFormat != YUV444) ? (pic->iWidth / 2) : pic->iWidth;
		output_image->image_.height_uv = (pic->iYUVFormat == YUV420) ? (pic->iHeight / 2) : pic->iHeight;
		output_image->image_.stride_uv = pic->iUVBufStride;

		const size_t plane_size_y = output_image->image_.height_y * output_image->image_.stride_y;
		const size_t plane_size_uv = output_image->image_.height_uv * output_image->image_.stride_uv;

		output_image->buffer_.reserve(plane_size_y + 2 * plane_size_uv);

		memcpy(output_image->buffer_.data(), pic->pY, plane_size_y);
		memcpy(output_image->buffer_.data() + plane_size_y, pic->pU, plane_size_uv);
		memcpy(output_image->buffer_.data() + plane_size_y + plane_size_uv, pic->pV, plane_size_uv);

		output_image->image_.data_y = output_image->buffer_.data();
		output_image->image_.data_u = output_image->image_.data_y + plane_size_y;
		output_image->image_.data_v = output_image->image_.data_u + plane_size_uv;

		decoded_images_.push(output_image);
		// Mark as used and move to next
		pic->bValid = 0;
		pic = pic->pNext;
	} while (pic != NULL && pic->bValid && all_frames);
}

static void release_context(CodecContext c) {
	Context *context = (Context *)c;
	if (!context)
		return;

	///
	free(context);
}

CODEC_API_EXPORT uint32_t CodecAPI_Version() { return LOADABLE_CODEC_API_VERSION; }

CODEC_API_EXPORT uint32_t CodecAPI_Query(int num, const char *buffer, uint32_t buffer_size) { return 0; }

static const char codec_name[] = "avc";
static const char codec_version_string[] = "LTM xxx";

CODEC_API_EXPORT Codec *CodecAPI_Create(const char *name, CodecOperation operation, const char *json_create_options) {
	Codec *codec = LTMCodecAllocate(codec_name, codec_version_string, operation);

	codec->create_context = create_context;
	codec->push_packet = push_packet;
	codec->pull_image = pull_image;
	codec->release_context = release_context;

	return codec;
}

CODEC_API_EXPORT void CodecAPI_Release(Codec *codec) {
	if (!codec)
		return;

	LTMCodecFree(codec);
}
