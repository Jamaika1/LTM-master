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
//               Stefano Battista (bautz66@gmail.com)
//
// Decoder.hpp
//
// Decoding pipeline
//

#pragma once

#include "Dimensions.hpp"
#include "Dithering.hpp"
#include "Image.hpp"
#include "SignaledConfiguration.hpp"
#include "Surface.hpp"
#include "YUVWriter.hpp"

#include <map>
#include <memory>

namespace lctm {

class Decoder {
public:
	Decoder();

	// Decode enhancment layer, and apply it to given base image
	Image decode(const Image &base, Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS], const Image &src_image,
	             bool report, bool dithering_switch, bool dithering_fixed, bool apply_enhancement);

	void initialize_decode(const Packet &enhancement_data, Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]);

	SignaledConfiguration get_configuration() { return configuration_; };
	Dimensions get_dimensions() { return dimensions_; };

	void set_idr(bool is_idr) {
		configuration_.picture_configuration.coding_type = is_idr ? CodingType::CodingType_IDR : CodingType::CodingType_NonIDR;
	};

private:
	bool is_user_data_layer(unsigned loq, unsigned layer) const;

	unsigned transform_block_size() const;
	unsigned num_residual_layers() const;

	bool is_tiled() const;

	Surface get_temporal_mask(Surface temporal_symbols);

	// Generate residuals for a plane's LOQ, along with any embedded temporal signalling
	Surface decode_residuals(unsigned plane, unsigned loq, Surface &temporal_mask, Surface symbols[MAX_NUM_LAYERS]);

	// Current configuration from syntax
	SignaledConfiguration configuration_;

	// Dimensions of the surfaces for the current configuration
	Dimensions dimensions_;

	//// State that persists between frames

	// Temporal buffer that survives between frames
	Surface temporal_buffer_[MAX_NUM_PLANES];

	// Quantization coefficients remembered for use next frame
	int32_t quant_matrix_coeffs_[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];

	Dithering dithering_;
};

} // namespace lctm
