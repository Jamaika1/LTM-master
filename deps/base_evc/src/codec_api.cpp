//
//
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>

#include "memory.h"

extern "C" {
#include "evc.h"
#include "evc_decoder_mem.h"
}

// LTM
#include "CodecApi.h"
#include "CodecUtils.h"
#include "Diagnostics.hpp"
#include "Misc.hpp"
#include "RingBuffer.hpp"

using namespace std;

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
	Context();
	//

	void decode();

	//
	void output_begin(bool is16bit, uint32_t width, uint32_t height);
	size_t output_write(const uint8_t *data, size_t size);
	void output_end() ;

	//
	std::string configuration_;

	State state_;
	stringstream base_bitstream_;

	int frame_count_ = 0;

	unsigned output_bitdepth_ = 10;

	thread decoder_thread_;

	// Base decoder
	unsigned output_frame_ = 0;

	// shallow deep ring buffer used to move pictures from docoder to codec api
	RingBuffer<shared_ptr<DecodedImage>> decoded_images_ = {2};

	// Image being assembled by YUV writer callbacks
	shared_ptr<DecodedImage> writing_image_;

	// Image returned to API client
	shared_ptr<DecodedImage> decoded_image_;
};

Context::Context()  {
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
			CHECK(length >= 4);

			// XXX
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
			context->decoder_thread_.join();
			*eos = 1;
			return 0;
		}
	default:
		ERR("Unexpected decoder state");
	}
	return 0;
}


static void output_begin_shim(void *ctx, int is16bit, uint32_t width, uint32_t height) {
	Context *self = (Context *)ctx;
	self->output_begin(is16bit, width, height);
}

static size_t output_write_shim(void *ctx, const uint8_t *data, size_t size) {
	Context *self = (Context *)ctx;
	return self->output_write(data, size);
}

static void output_end_shim(void *ctx) {
	Context *self = (Context *)ctx;
	self->output_end();
}

//
//
void Context::decode() {
	evc_writer_t writer = {
		output_begin_shim,
		output_write_shim,
		output_end_shim,
		(void *)this
	};

	const std::string &s = base_bitstream_.str();
	evc_decode_mem((const uint8_t *)s.data(), s.size(), output_bitdepth_, &writer);
}

void Context::output_begin(bool is16bit, uint32_t width, uint32_t height) {
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

size_t Context::output_write(const uint8_t *data, size_t size) {
	CHECK(writing_image_);

	writing_image_->buffer_.insert(writing_image_->buffer_.end(), data, data + size);
	return size;
}

void Context::output_end() {
	CHECK(writing_image_);

	writing_image_->image_.data_y = writing_image_->buffer_.data();
	writing_image_->image_.data_u = writing_image_->image_.data_y + writing_image_->image_.stride_y * writing_image_->image_.height_y;
	writing_image_->image_.data_v = writing_image_->image_.data_u + writing_image_->image_.stride_uv * writing_image_->image_.height_uv;

	decoded_images_.push(writing_image_);
	writing_image_ = nullptr;
}

/////

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

static const char codec_name[] = "evc";
static const char codec_version_string[] = "LTM xxx";

CODEC_API_EXPORT Codec*  CodecAPI_Create(const char *name, CodecOperation operation, const char *json_create_options) {
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
