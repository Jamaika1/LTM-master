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
// Dimensions.cpp
//

#include "Dimensions.hpp"
#include "Diagnostics.hpp"
#include "SignaledConfiguration.hpp"

#include <algorithm>

namespace lctm {

Dimensions::Dimensions() {}

// Set surface dimensions for one plane
//
void Dimensions::set_plane_dimensions(const SignaledConfiguration &configuration, unsigned plane, unsigned loq,
									  unsigned width, unsigned height,
									  unsigned scale_tile_width, unsigned scale_tile_height) {
	const unsigned tbs = configuration.global_configuration.transform_block_size;

	plane_width_[plane][loq] = width;
	plane_height_[plane][loq] = height;

	layer_width_[plane][loq] = (width + tbs - 1) / tbs;
	layer_height_[plane][loq] = (height + tbs - 1) / tbs;

	// Convert signalled tile size (in top level luma pels) into per layer size
	if(configuration.global_configuration.tile_width && configuration.global_configuration.tile_height) {
		CHECK(configuration.global_configuration.tile_width % tbs == 0);
		CHECK(configuration.global_configuration.tile_height % tbs == 0);

		tile_width_[plane][loq] = configuration.global_configuration.tile_width / (tbs * scale_tile_width);
		tile_height_[plane][loq] = configuration.global_configuration.tile_height / (tbs * scale_tile_height);
	} else {
		tile_width_[plane][loq] = 0;
		tile_height_[plane][loq] = 0;
	}
}

// Set surface dimensions for all planes in loq
//
void Dimensions::set_loq_dimensions(const SignaledConfiguration &configuration, unsigned loq, unsigned width, unsigned height) {
	// Figure out UV dimensions from Y dimensions
	//
	const unsigned chroma_width = (width + chroma_scale_width_ - 1) / chroma_scale_width_;
	const unsigned chroma_height = (height + chroma_scale_height_ - 1) / chroma_scale_height_;

	// Y
	if (configuration.global_configuration.num_image_planes >= 1)
		set_plane_dimensions(configuration, 0, loq, width, height, 1, 1);

	if (configuration.global_configuration.num_image_planes == 3) {
		// U
		set_plane_dimensions(configuration, 1, loq, chroma_width, chroma_height, chroma_scale_width_, chroma_scale_height_);
		// V
		set_plane_dimensions(configuration, 2, loq, chroma_width, chroma_height, chroma_scale_width_, chroma_scale_height_);
	}
}

// Work out surface dimensions from configuration
//
void Dimensions::set_dimensions(const SignaledConfiguration &configuration, unsigned width, unsigned height) {
	set_loq_dimensions(configuration, LOQ_LEVEL_2, width, height);

	switch (configuration.global_configuration.scaling_mode[LOQ_LEVEL_2]) {
	case ScalingMode_None:
		set_loq_dimensions(configuration, LOQ_LEVEL_1, width, height);
		break;
	case ScalingMode_1D:
		set_loq_dimensions(configuration, LOQ_LEVEL_1, (width + 1) / 2, height);
		break;
	case ScalingMode_2D:
		set_loq_dimensions(configuration, LOQ_LEVEL_1, (width + 1) / 2, (height + 1) / 2);
		break;
	default:
		CHECK(0);
	}
}

void Dimensions::set(const SignaledConfiguration &configuration, unsigned width, unsigned height) {

	// Figure chroma scaling
	switch (configuration.global_configuration.colourspace) {
	case Colourspace_Y:
		chroma_scale_width_ = 1;
		chroma_scale_height_ = 1;
		break;

	case Colourspace_YUV420:
		chroma_scale_width_ = 2;
		chroma_scale_height_ = 2;
		break;

	case Colourspace_YUV422:
		chroma_scale_width_ = 2;
		chroma_scale_height_ = 1;
		break;

	case Colourspace_YUV444:
		chroma_scale_width_ = 1;
		chroma_scale_height_ = 1;
		break;
	default:
		CHECK(0);
		break;
	}

	// Try with given dimensions
	set_dimensions(configuration, width, height);

	CHECK(configuration.global_configuration.num_image_planes >= configuration.global_configuration.num_processed_planes);

	//// Figure conformant size
	//
	// Get size of smallest enhanced plane's layers
	unsigned last_plane = std::max(configuration.global_configuration.num_processed_planes, 1u) - 1;

	// Start with layer size in pels
	unsigned w = layer_width_[last_plane][LOQ_LEVEL_1] * configuration.global_configuration.transform_block_size;
	unsigned h = layer_height_[last_plane][LOQ_LEVEL_1] * configuration.global_configuration.transform_block_size;

	// Scale up until >= original dimensions
	while (w < width)
		w *= 2;

	while (h < height)
		h *= 2;

	// Use conforming dimensions
	set_dimensions(configuration, w, h);

	// Figure out sizes of scaling layers
	switch (configuration.global_configuration.scaling_mode[LOQ_LEVEL_2]) {
	case ScalingMode_1D:
		w /= 2;
		break;
	case ScalingMode_2D:
		w /= 2;
		h /= 2;
		break;
	case ScalingMode_None:
		break;
	default:
		CHECK(0);
	}

	intermediate_width_ = w;
	intermediate_height_ = h;

	switch (configuration.global_configuration.scaling_mode[LOQ_LEVEL_1]) {
	case ScalingMode_1D:
		w /= 2;
		break;
	case ScalingMode_2D:
		w /= 2;
		h /= 2;
		break;
	case ScalingMode_None:
		break;
	default:
		CHECK(0);
	}

	base_width_ = w;
	base_height_ = h;

	// where is the frame rate ???
	// configuration.global_configuration.temporal_fps;
}

} // namespace lctm
