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
// Support utils for codecs
//

#include "CodecUtils.h"

#include <memory.h>
#include <string.h>

//// Default entries for codec functions
//
static int32_t create_metadata(CodecMetadata *metadata, CodecError *error) {
	return 0;
}

static void release_context(CodecContext c) {
}

static void release_error(CodecError e) {
}

static void release_metadata(CodecMetadata m) {
}

// Properties of metadata
static const char * get_metadata_property_name(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static uint32_t get_metadata_property_u32(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static int32_t get_metadata_property_i32(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static uint64_t get_metadata_property_u64(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static int64_t get_metadata_property_i64(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static int8_t get_metadata_property_bool(CodecMetadata metadata, uint32_t id) {
	return 0;
}

static void set_metadata_property_u32(CodecMetadata metadata, uint32_t id, uint32_t v) {
}

static void set_metadata_property_i32(CodecMetadata metadata, uint32_t id, int32_t v) {
}

static void set_metadata_property_u64(CodecMetadata metadata, uint32_t id, uint64_t v) {
}

static void set_metadata_property_i64(CodecMetadata metadata, uint32_t id, uint64_t v) {
}

static void set_metadata_property_bool(CodecMetadata metadata, uint32_t id, uint8_t v) {
}

// Properties of error
static int32_t get_error_code(CodecError error) {
	return 0;
}

static size_t get_error_message(CodecError error, char *data, size_t length) {
	return 0;
}

static size_t get_error_file(CodecError error, char *filename, size_t length) {
	return 0;
}

static uint32_t get_error_line(CodecError error) {
	return 0;
}

static int32_t push_packet(CodecContext c, const uint8_t *data, size_t length, CodecMetadata metadata, int8_t eos, CodecError *error) {
	return 0;
}

static int32_t pull_packet(CodecContext c, const uint8_t *data, size_t length, CodecMetadata *metadata, int8_t* eos, CodecError *error) {
	return 0;
}

static int32_t push_image(CodecContext c, const CodecImage* image, CodecMetadata metadata, int8_t eos, CodecError *error) {
	return 0;
}

static int32_t pull_image(CodecContext c, CodecImage* image, CodecMetadata *metadata, int8_t* eos, CodecError *error) {
	return 0;
}

Codec* LTMCodecAllocate(const char *name, const char *version_string, CodecOperation operation) {
	Codec *codec = (Codec *)calloc(1, sizeof(Codec));

	codec->api_version = LOADABLE_CODEC_API_VERSION;

	codec->name = strdup(name);
	codec->version_string = strdup(name);
	codec->operation = operation;
	codec->push_packet = push_packet;
	codec->pull_packet = pull_packet;
	codec->push_image = push_image;
	codec->pull_image = pull_image;
	codec->create_metadata = create_metadata;
	codec->release_context = release_context;
	codec->release_error = release_error;
	codec->release_metadata = release_metadata;
	codec->get_metadata_property_name = get_metadata_property_name;
	codec->get_metadata_property_u32 = get_metadata_property_u32;
	codec->get_metadata_property_i32 = get_metadata_property_i32;
	codec->get_metadata_property_u64 = get_metadata_property_u64;
	codec->get_metadata_property_i64 = get_metadata_property_i64;
	codec->get_metadata_property_bool = get_metadata_property_bool;
	codec->set_metadata_property_u32 = set_metadata_property_u32;
	codec->set_metadata_property_i32 = set_metadata_property_i32;
	codec->set_metadata_property_u64 = set_metadata_property_u64;
	codec->set_metadata_property_i64 = set_metadata_property_i64;
	codec->set_metadata_property_bool = set_metadata_property_bool;
	codec->get_error_code = get_error_code;
	codec->get_error_message = get_error_message;
	codec->get_error_file = get_error_file;
	codec->get_error_line = get_error_line;

	return codec;
}

void LTMCodecFree(Codec *codec) {
	if(!codec)
		return;

	if(codec->name)
		free((void *)codec->name);

	if(codec->version_string)
		free((void *)codec->name);

	free(codec);
}
