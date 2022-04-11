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
//               Stefano Battista (bautz66@gmail.com)
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//
// Encoder.hpp
//
// Decode/Encode pipeline
//

#pragma once

#include <map>
#include <memory>

#include "Dimensions.hpp"
#include "EncoderConfiguration.hpp"
#include "Image.hpp"
#include "LayerEncodeFlags.hpp"
#include "Parameters.hpp"
#include "SignaledConfiguration.hpp"
#include "Dithering.hpp"
#include "PriorityConfiguration.hpp"

namespace lctm {

// Describes padding applied around image to make it conformant
struct ConformancePadding {
	unsigned left;
	unsigned top;
	unsigned right;
	unsigned bottom;
};

class Encoder {
public:
	Encoder(const ImageDescription &image_description, const Parameters &parameters);
	void initialise_config(const Parameters &parameters, std::vector<std::unique_ptr<Image>> &src_image);

	const Dimensions &dimensions() const { return dimensions_; }

	Packet encode(std::vector<std::unique_ptr<Image>> &src, const Image &intermediate_src, const Image &base_recon,
	              BaseFrameType frame_type, bool is_idr, int gop_frame_num, const std::string &output_file);

	unsigned transform_block_size() const;
	unsigned num_residual_layers() const;
	bool level1_depth_flag() const;
	unsigned base_qp() const;

	const ImageDescription &src_image_description() const { return src_image_description_; }
	const ImageDescription &base_image_description() const { return base_image_description_; }
	const ImageDescription &intermediate_image_description() const { return intermediate_image_description_; }
	const ImageDescription &enhancement_image_description() const { return enhancement_image_description_; }

	void set_last_idr_frame_num(int32_t frame_num) { last_idr_frame_num = frame_num; }

private:
	void update_encoder_configuration(const Parameters &p, const Parameters &d);
	void update_global_configuration(const Parameters &p, const Parameters &d, const ImageDescription &image_description);
	void update_sequence_configuration(const Parameters &p, const Parameters &d, const ImageDescription &image_description);
	void update_picture_configuration(const Parameters &p, const Parameters &d);

	void update_picture_configuration_in_loop();

	bool is_user_data_layer(unsigned loq, unsigned layer) const;

	void encode_residuals(unsigned plane, unsigned loq, const Surface &residuals, Surface symbols[MAX_NUM_LAYERS],
	                      Temporal_SWM swm_type, EncodingMode mode, const Surface &temporal_mask, const Surface &priority_type,
	                      PriorityMBType priority_mb_type, const bool final = false, const Surface &pixel_sad = Surface()) const;

	Surface decode_residuals(unsigned plane, unsigned loq, const Surface symbols[MAX_NUM_LAYERS], Temporal_SWM swm_type,
	                         const Surface &temporal_mask) const;

	// Configuration that is local to encoder
	EncoderConfiguration encoder_configuration_;

	// Configuration that is sent to decoder
	SignaledConfiguration configuration_;

	// Dimensions of the surfaces for the current configuration
	Dimensions dimensions_;

	// Image descriptions (dimensions + format + bitdepth)

	// Input
	ImageDescription src_image_description_;

	// Base image
	ImageDescription base_image_description_;

	// Layer 1
	ImageDescription intermediate_image_description_;

	// Layer 2
	ImageDescription enhancement_image_description_;

	// Residuals from previous frame
	Surface previous_residuals_[MAX_NUM_PLANES];

	// Quantization coefficients remembered for use next frame
	int32_t quant_matrix_coeffs_[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS] = {-1};

	int32_t last_idr_frame_num;

	Dithering dithering_;
};

} // namespace lctm
