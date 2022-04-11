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
// Binary interface to loadable codecs
// NB: This is a C interface
//
//
#pragma once

#include <stdlib.h>
#include <stdint.h>

#define LOADABLE_CODEC_API_VERSION 1

typedef void *CodecContext;
typedef void *CodecError;
typedef void *CodecMetadata;

typedef enum CodecOperation {
	CodecOperation_Encode, // Images to packets
	CodecOperation_Decode, // Packets to images + metadata
	CodecOperation_Parse, // Packets to metadata
} CodecOperation;

//
typedef struct CodecImage {
	uint32_t bpp;
	uint32_t width_y;
	uint32_t height_y;
	uint32_t stride_y;

	uint32_t width_uv;
	uint32_t height_uv;
	uint32_t stride_uv;

	const uint8_t *data_y;
	const uint8_t *data_u;
	const uint8_t *data_v;
} CodecImage;

// This is the low level shared API across the app->loadable codec interface
//
typedef struct Codec {
	uint32_t api_version;
	const char *name;
	const char *version_string;
	CodecOperation operation;

	// For use by underlying codec
	void *internal;

	int32_t (*create_context)(CodecContext *context, const char *json_configuration, CodecError *error);

	//
	int32_t (*push_packet)(CodecContext context, const uint8_t *data, size_t length, CodecMetadata metadata, int8_t eos, CodecError *error);
	int32_t (*pull_packet)(CodecContext context, const uint8_t *data, size_t length, CodecMetadata *metadata, int8_t* eos, CodecError *error);

	//
	int32_t (*push_image)(CodecContext context, const CodecImage* image, CodecMetadata metadata, int8_t eos, CodecError *error);
	int32_t (*pull_image)(CodecContext context, CodecImage* image, CodecMetadata *metadata, int8_t* eos, CodecError *error);

	int32_t (*create_metadata)(CodecMetadata *metadata, CodecError *error);

	void (*release_context)(CodecContext context);
	void (*release_error)(CodecError error);
	void (*release_metadata)(CodecMetadata metadata);

	// Properties of metadata
	const char * (*get_metadata_property_name)(CodecMetadata metadata, uint32_t id);
	uint32_t (*get_metadata_property_u32)(CodecMetadata metadata, uint32_t id);
	int32_t (*get_metadata_property_i32)(CodecMetadata metadata, uint32_t id);
	uint64_t (*get_metadata_property_u64)(CodecMetadata metadata, uint32_t id);
	int64_t (*get_metadata_property_i64)(CodecMetadata metadata, uint32_t id);
	int8_t (*get_metadata_property_bool)(CodecMetadata metadata, uint32_t id);

	void (*set_metadata_property_u32)(CodecMetadata metadata, uint32_t id, uint32_t v);
	void (*set_metadata_property_i32)(CodecMetadata metadata, uint32_t id, int32_t v);
	void (*set_metadata_property_u64)(CodecMetadata metadata, uint32_t id, uint64_t v);
	void (*set_metadata_property_i64)(CodecMetadata metadata, uint32_t id, uint64_t v);
	void (*set_metadata_property_bool)(CodecMetadata metadata, uint32_t id, uint8_t v);

	// Properties of error
	int32_t (*get_error_code)(CodecError error);
	size_t (*get_error_message)(CodecError error, char *data, size_t length);
	size_t (*get_error_file)(CodecError error, char *filename, size_t length);
	uint32_t (*get_error_line)(CodecError error);
} Codec;

// Some well know property IDs

enum PropertyID {
	PropertyID_None = 0,

	// CodecMetadata
	PropertyID_Timestamp = 1,
	PropertyID_PictureOrderCount = 2,
	PropertyID_QP = 3,
	PropertyID_FrameType = 4, // 0=I 1=P 2=B
};

