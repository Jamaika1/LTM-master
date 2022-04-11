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
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//
// FileEncoder.hpp
//
// Takes video YUV files, and encodes
//

#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "Encoder.hpp"
#include "Image.hpp"
#include "Packet.hpp"
#include "Parameters.hpp"
#include "Types.hpp"

namespace lctm {

class FileEncoder {
public:
	virtual void encode_file(const std::string &src_filename, const std::string &dst_filename, const std::string &dst_filename_yuv,
	                         unsigned limit = UINT_MAX) = 0;

	virtual void encode_file_with_decoder(const std::string &src_filename, const std::string base_filename,
	                                      const std::string &dst_filename, const std::string &dst_filename_yuv,
	                                      unsigned limit = UINT_MAX) = 0;

	virtual void encode_file_with_base(const std::string &src_filename, const std::string base_filename,
	                                   const std::string base_recon_filename, const std::string &dst_filename,
	                                   const std::string &dst_filename_yuv, unsigned limit = UINT_MAX) = 0;

	virtual void downsample_file(const std::string &src_filename, const std::string &dst_filename, unsigned limit = UINT_MAX) = 0;
	virtual void upsample_file(const std::string &src_filename, const std::string &dst_filename, unsigned limit = UINT_MAX) = 0;

	virtual ~FileEncoder(){};

protected:
	FileEncoder(){};

private:
	void operator=(const FileEncoder &);
	FileEncoder(const FileEncoder &);
};

// Factory functions
//

//// Create a base video encoder
//
std::unique_ptr<FileEncoder> CreateFileEncoder(BaseCoding base_encoder, const ImageDescription &image_description, unsigned fps,
                                               const Parameters &parameters);

} // namespace lctm
