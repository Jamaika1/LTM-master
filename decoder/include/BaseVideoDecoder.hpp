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
// BaseVideoDecoder.hpp
//
// Base video layer decoder - consumes AU, produces YUV and enhancement data
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <queue>
#include <vector>

#include "BitstreamStatistic.hpp"
#include "Dimensions.hpp"
#include "Image.hpp"
#include "SignaledConfiguration.hpp"
#include "Types.hpp"

#include "CodecApi.h"

namespace lctm {

//
class BaseVideoDecoder {
public:
	// Description of decoded base picture (more general than Image to cope with base codec data layouts)
	//
	typedef struct CodecImage BasePicture;
	
	// Abstract interface provided by client for passing base yuv + enhancment data back
	//
	class Output {
	public:
		virtual ~Output(){};

		// Send base+enhancement to consumer - buffers are only valid during call
		virtual void push_base_enhancement(const BasePicture *base_picture, const uint8_t *enhancement_data,
		                                   size_t enhancement_data_size, uint64_t pts, bool is_lcevc_idr) = 0;

		// Version for contiguous 420 planar buffer
		virtual void push_base_enhancement(const uint8_t *base_data, size_t base_data_size,
		                                   Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], uint64_t pts,
		                                   bool is_lcevc_idr) = 0;

		virtual void deserialize_enhancement(const uint8_t *enhancement_data, size_t enhancement_data_size,
		                                     Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]) = 0;

		virtual Dimensions get_dimensions() = 0;
		virtual Colourspace get_colourspace() = 0;
		virtual unsigned get_base_bitdepth() = 0;
	};

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void push_au(const uint8_t *data, size_t data_size, uint64_t pts, bool is_base_idr, int iPictureType) = 0;

	virtual void StatisticsComputation() = 0;

	Packet rbsp_encapsulate(const Packet &src) const;

	virtual ~BaseVideoDecoder(){};

protected:
	BaseVideoDecoder(){};

private:
	void operator=(const BaseVideoDecoder &);
	BaseVideoDecoder(const BaseVideoDecoder &);
};

// Factory function
//
std::unique_ptr<BaseVideoDecoder> CreateBaseVideoDecoder(BaseVideoDecoder::Output &output, BaseCoding base,
                                                         Encapsulation encapulation, bool external,
                                                         const std::string &prepared_yuv_file, bool keep_base);

} // namespace lctm
