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
// Dimensions.hpp
//
// Calculates various image dimensions, given a signalled configuration
//
#pragma once

#include "SignaledConfiguration.hpp"

namespace lctm {

class Dimensions {
public:
	Dimensions();

	void set(const SignaledConfiguration &configuration, unsigned width, unsigned height);

	unsigned plane_width(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return plane_width_[plane][loq]; }
	unsigned plane_height(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return plane_height_[plane][loq]; }

	unsigned layer_width(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return layer_width_[plane][loq]; }
	unsigned layer_height(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return layer_height_[plane][loq]; }

	unsigned tile_width(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return tile_width_[plane][loq]; }
	unsigned tile_height(unsigned plane, unsigned loq = LOQ_LEVEL_2) const { return tile_height_[plane][loq]; }

	unsigned conformant_width(unsigned plane = 0) const { return plane_width_[plane][LOQ_LEVEL_2]; }
	unsigned conformant_height(unsigned plane = 0) const { return plane_height_[plane][LOQ_LEVEL_2]; }

	unsigned crop_unit_width(unsigned plane = 0) const { return plane == 0 ? chroma_scale_width_ : 1; }
	unsigned crop_unit_height(unsigned plane = 0) const { return plane == 0 ? chroma_scale_height_ : 1; }

	unsigned intermediate_width() const { return intermediate_width_; }
	unsigned intermediate_height() const { return intermediate_height_; }

	unsigned base_width() const { return base_width_; }
	unsigned base_height() const { return base_height_; }

private:
	void set_plane_dimensions(const SignaledConfiguration &configuration, unsigned plane, unsigned loq, unsigned width,
	                          unsigned height, unsigned scale_tile_width, unsigned scale_tile_height);
	void set_loq_dimensions(const SignaledConfiguration &configuration, unsigned loq, unsigned width, unsigned height);
	void set_dimensions(const SignaledConfiguration &configuration, unsigned width, unsigned height);

	// Dimensions of LOQs per plane
	unsigned plane_width_[MAX_NUM_PLANES][MAX_NUM_LOQS];
	unsigned plane_height_[MAX_NUM_PLANES][MAX_NUM_LOQS];

	// Dimensions of layers
	unsigned layer_width_[MAX_NUM_PLANES][MAX_NUM_LOQS];
	unsigned layer_height_[MAX_NUM_PLANES][MAX_NUM_LOQS];

	// Dimensions of tiles
	unsigned tile_width_[MAX_NUM_PLANES][MAX_NUM_LOQS];
	unsigned tile_height_[MAX_NUM_PLANES][MAX_NUM_LOQS];

	unsigned intermediate_width_;
	unsigned intermediate_height_;

	unsigned base_width_;
	unsigned base_height_;

	unsigned chroma_scale_width_;
	unsigned chroma_scale_height_;
};

} // namespace lctm
