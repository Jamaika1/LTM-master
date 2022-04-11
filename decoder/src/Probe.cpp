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
// Utilities to peek into an ES file pull out dimensions, formats & depths
//
#include "Probe.hpp"

#include "Diagnostics.hpp"
#include "uESFile.h"

using namespace std;
using namespace vnova::utility;

// Number of AUs to scan for SPS/PPS when figuring base dimensions
//
const int PROBE_AU_LIMIT = 50;

namespace lctm {

bool probe_es_file(const string &name, BaseCoding coding, unsigned &width, unsigned &height, unsigned &bit_depth,
                   unsigned &chroma_format_idc) {
	// Open the file
	BaseDecoder::Codec es_type;
	switch (coding) {
	case BaseCoding_AVC:
		es_type = BaseDecoder::AVC;
		break;
	case BaseCoding_HEVC:
		es_type = BaseDecoder::HEVC;
		break;
	case BaseCoding_VVC:
		es_type = BaseDecoder::VVC;
		break;
	case BaseCoding_EVC:
		es_type = BaseDecoder::EVC;
		break;
	default:
		CHECK(0);
	}

	ESFile es_file;
	CHECK(es_file.Open(name, es_type));

	// Go through  AUs until SPS has been seen ...
	for (int i = 0; i < PROBE_AU_LIMIT; ++i) {
		ESFile::AccessUnit au;
		ESFile::Result r = es_file.NextAccessUnit(au);

		// Oh dear ...
		if (r != ESFile::Success)
			return false;

		// Have we got a good width, height & depth?
		unsigned w = es_file.GetPictureWidth();
		unsigned h = es_file.GetPictureHeight();
		unsigned d = es_file.GetBitDepth();
		unsigned c = es_file.GetChromaFormatIDC();

		if (w && h && d) {
			width = w;
			height = h;
			bit_depth = d;
			chroma_format_idc = c;

			return true;
		}
	}

	return false;
}

} // namespace lctm
