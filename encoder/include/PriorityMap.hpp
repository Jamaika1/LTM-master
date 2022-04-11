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
// Contributors: Martin Vymazal (martin.vymazal@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//               Lorenzo Ciccarelli(lorenzo.ciccarelli@v-nova.com)
//
// PriorityMap.hpp
//

#pragma once

#include <array>
#include <limits>

#include "Component.hpp"
#include "Surface.hpp"
#include "PriorityConfiguration.hpp"

// #define PRIORITY_TEST
// #define PRIORITY_RESI
// #define PRIORITY_RESI_SL1
// #define PRIORITY_RESI_SL2

#define PRIORITY_COEF

#define BLOCK_SIZE 8

namespace lctm {

enum KillingFunction { KILL_ALL = 0, KEEP_ALL = 1, THRESHOLD_CUTOFF = 2 };

class PriorityMap : public Component {
public:
	enum EnumBlockType { Plain = 32, Edge = 64, SmoothTexture = 96, CoarseTexture = 128, Max = 255 };

	// Value type inside priority map
	using value_type = int16_t;

	PriorityMap() : Component("PriorityMap") {}

	Surface process(const Surface &src_plane, unsigned priority_tile_x, unsigned priority_tile_y,
	                KillingFunction killing_function = KillingFunction::KILL_ALL);

	Surface ComputeMeanValue(const Surface &src_surface);

	Surface ComputeContrastTexture(const Surface &src_surface, Surface &mean_surface);

	// apply anlalysis on block type on residuals (before Trans & Quant)
	Surface ApplyPriorityResiduals(Surface &src_surface, Surface &type_surface, unsigned step_width);

	// apply anlalysis on block type on coefficients (after Trans & Quant)
	void ApplyPriorityCoefficients(int width, int height, int tu_size, int sublayer, Surface src_surface[], Surface dst_surface[],
	                               const Surface &type_surface, const Surface &pixel_sad, unsigned step_width,
	                               PriorityMBType priority_mb_type, EncodingMode mode);

private:
	struct TileGrid {
		TileGrid(const Surface &surface, unsigned tile_size_x, unsigned tile_size_y);
		TileGrid(unsigned tile_size_x, unsigned tile_size_y, unsigned n_tiles_x, unsigned n_tiles_y);

		unsigned tile_size_x_ = 0;
		unsigned tile_size_y_ = 0;
		unsigned n_tiles_x_ = 0;
		unsigned n_tiles_y_ = 0;
	};

	// Residual killing functions: decide which residuals to keep
	static Surface kill_all_residuals(const Surface &src_surface, const std::vector<double> &priority_values,
	                                  const TileGrid &tgrid);
	static Surface keep_all_residuals(const Surface &src_surface, const std::vector<double> &priority_values,
	                                  const TileGrid &tgrid);
	static Surface threshold_cutoff(const Surface &src_surface, const std::vector<double> &priority_values, const TileGrid &tgrid);

	using pmap_function =
	    std::function<Surface(const Surface &src_surface, const std::vector<double> &priority_values, const TileGrid &tgrid)>;

	static std::array<pmap_function, 3> priority_functions_;
};

// This rescales priority map values before dumping them to file
// Use only for visualization
class PriorityMapVis : public Component {
public:
	PriorityMapVis() : Component("PriorityMapVis") {}

	Surface process(const Surface &src_plane);
};

// Apply the removal of non-static residuals/coefficients
//
class StaticResiduals : public Component {
public:
	StaticResiduals() : Component("StaticResiduals") {}

	void process(Surface src_coeffs[], Surface dst_coeffs[], const Surface &pixel_sad, unsigned num_layers, unsigned step_width,
	             unsigned sad_threshold, unsigned sad_coeff_threshold);
};

} // namespace lctm
