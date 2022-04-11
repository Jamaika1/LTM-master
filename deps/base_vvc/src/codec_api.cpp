//
//
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>

#include "memory.h"

#include "VideoIOYuvMem.h"
#include "DecAppMem.h"

#undef CHECK

// LTM
#include "CodecApi.h"
#include "CodecUtils.h"
#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "RingBuffer.hpp"

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

class Context: public VideoIOYuvMem::Writer  {
public:
	Context();
	//

	void decode();

	// Implement VideoIOYuvMem::Writer
	void picture_begin(bool is16bit, uint32_t width, uint32_t height, ChromaFormat format) override;
	void picture_end() override;
	int write(const uint8_t *data, size_t size) override;

	//
	std::string configuration_;

	State state_;
	stringstream base_bitstream_;

	int frame_count = 0;

	thread decoder_thread_;

	// Base decoder
	DecAppMem m_decoder;

	unsigned output_frame_ = 0;

	// shallow ring buffer used to move pictures from docoder to codec api
	RingBuffer<shared_ptr<DecodedImage>> decoded_images_ = {2};

	// Image being assembled by YUV writer callbacks
	shared_ptr<DecodedImage> writing_image_;

	// Image returned to API client
	shared_ptr<DecodedImage> decoded_image_;
};

Context::Context() : m_decoder(this) {
}

static int context_count  = 0;

static int32_t create_context(CodecContext *cp, const char *json_configuration, CodecError *error) {
	if(error)
		*error = 0;

	Context* context = new Context();
	context->configuration_ = json_configuration;
	context->state_ = StatePushing;
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

			// XXXX
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
		context->decoder_thread_ = std::thread([context] {
				context->decode();
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

void Context::decode() {
	m_decoder.decode(base_bitstream_);
}

void Context::picture_begin(bool is16bit, uint32_t width, uint32_t height, ChromaFormat format) {
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

static const char codec_name[] = "vvc";
static const char codec_version_string[] = "LTM xxx";

CODEC_API_EXPORT Codec* CodecAPI_Create(const char *name, CodecOperation operation, const char *json_create_options) {
	Codec *codec = LTMCodecAllocate(codec_name, codec_version_string, operation);

	codec->create_context = create_context;
	codec->push_packet = push_packet;
	codec->pull_image = pull_image;
	codec->release_context = release_context;

	return codec;
}

CODEC_API_EXPORT void  CodecAPI_Release(Codec *codec) {
	if(!codec)
		return;

	LTMCodecFree(codec);
}
