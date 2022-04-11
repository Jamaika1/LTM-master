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

#include "Codec.hpp"

#include <memory>

#include "Misc.hpp"
#include "Diagnostics.hpp"
#include "Platform.hpp"
#include "SharedLibrary.hpp"

#include "CodecApi.h"

using namespace std;

namespace lctm {

typedef uint32_t (*codec_api_version_fn_t)();
typedef uint32_t (*codec_api_query_fn_t)(int num, const char *buffer, uint32_t buffer_size);
typedef Codec* (*codec_api_create_fn_t)(const char *name, CodecOperation operation, const char *json_configuration);

Codec* CodecCreate(const string &name, CodecOperation operation, const string &create_options) {

	// Try to load a library of form "<program>/external_codecs/libs/base_<name>.so"

	string library_file = get_program_directory(format("external_codecs/libs/%sbase_%s.%s", SHARED_PREFIX, name.c_str(), SHARED_SUFFIX));

	if(file_size(library_file) < 0) {
		ERR("Cannot find base codec library: \"%s\"", library_file.c_str());
		return nullptr;
	}

	codec_api_version_fn_t version_fn;
	codec_api_query_fn_t query_fn;
	codec_api_create_fn_t create_fn;

	shared_handle_t h = CHECK(SHARED_LOAD(library_file.c_str()));

	if(!h)
		return nullptr;

  	version_fn = (codec_api_version_fn_t)CHECK(SHARED_SYMBOL(h, "CodecAPI_Version"));

	const uint32_t version = version_fn();
	if(version != LOADABLE_CODEC_API_VERSION) {
		ERR("Base codec library \"%s\" has wrong version: %d", library_file.c_str(), version);
		return nullptr;
	}

 	query_fn = (codec_api_query_fn_t)CHECK(SHARED_SYMBOL(h, "CodecAPI_Query"));
 	create_fn = (codec_api_create_fn_t)CHECK(SHARED_SYMBOL(h, "CodecAPI_Create"));

	Codec* codec = create_fn(name.c_str(), operation, create_options.c_str());

	return codec;
}

void CodecRelease(Codec* codec) {
}

// Convert a codec error to a user string
//
string CodecErrorToString(Codec* codec, CodecError error) {
	const size_t buffer_len = codec->get_error_message(error, 0, 0);
	char *buffer = CHECK((static_cast<char *>(alloca(buffer_len))));
	codec->get_error_message(error, buffer, buffer_len);
	codec->release_error(error);
	return string(buffer, buffer + buffer_len);
}

} // namespace lctm
