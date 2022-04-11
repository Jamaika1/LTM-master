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
// Contributors: Martin Vymazal(martin.vymazal@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//               Lorenzo Ciccarelli(lorenzo.ciccarelli@v-nova.com)
//
// PriorityMap.cpp
//
// Generate a map indicating which residuals should be kept/killed
//

#include "PriorityMap.hpp"
#include "Misc.hpp"
#include "Types.hpp"
#include "SignaledConfiguration.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lctm {

PriorityMap::TileGrid::TileGrid(const Surface &surface, unsigned tile_size_x, unsigned tile_size_y)
    : tile_size_x_{tile_size_x}, tile_size_y_{tile_size_y} {

	n_tiles_x_ = surface.width() / tile_size_x;
	if (n_tiles_x_ * tile_size_x < surface.width()) {
		n_tiles_x_++;
	}

	n_tiles_y_ = surface.height() / tile_size_y;
	if (n_tiles_y_ * tile_size_y < surface.height()) {
		n_tiles_y_++;
	}
}

PriorityMap::TileGrid::TileGrid(unsigned tile_size_x, unsigned tile_size_y, unsigned n_tiles_x, unsigned n_tiles_y)
    : tile_size_x_{tile_size_x}, tile_size_y_{tile_size_y}, n_tiles_x_{n_tiles_x}, n_tiles_y_{n_tiles_y} {}

std::array<PriorityMap::pmap_function, 3> PriorityMap::priority_functions_ = {&kill_all_residuals, &keep_all_residuals,
                                                                              &threshold_cutoff};

// For each block in map - kill coefficients if corresponding block is set to 0 in map
Surface PriorityMap::process(const Surface &src_plane, unsigned priority_tile_x, unsigned priority_tile_y,
                             KillingFunction killing_function) {
	assert(0 <= killing_function);
	assert(killing_function < priority_functions_.size());

	const TileGrid grid{src_plane, priority_tile_x, priority_tile_y};

	// Store priority values in raster order
	// This implementation uses a hardcoded 'dummy' value - should
	// be replaced by a more sophisticated algorithm
	std::vector<double> pvalues(grid.n_tiles_x_ * grid.n_tiles_y_);

	for (unsigned tile_y = 0; tile_y < grid.n_tiles_y_; ++tile_y) {
		for (unsigned tile_x = 0; tile_x < grid.n_tiles_x_; ++tile_x) {

			pvalues[tile_y * grid.n_tiles_x_ + tile_x] = 0.7;
		}
	}

	return priority_functions_[killing_function](src_plane, pvalues, grid);
}

Surface PriorityMap::kill_all_residuals(const Surface &src_surface, const std::vector<double> &priority_values,
                                        const TileGrid &tgrid) {
	return Surface::build_from<value_type>()
	    .generate(src_surface.width(), src_surface.height(),
	              [](unsigned x, unsigned y) -> value_type { return ResidualLabel::RESIDUAL_KILL; })
	    .finish();
}

Surface PriorityMap::keep_all_residuals(const Surface &src_surface, const std::vector<double> &priority_values,
                                        const TileGrid &tgrid) {
	return Surface::build_from<value_type>()
	    .generate(src_surface.width(), src_surface.height(),
	              [](unsigned x, unsigned y) -> value_type { return ResidualLabel::RESIDUAL_LIVE; })
	    .finish();
}

Surface PriorityMap::threshold_cutoff(const Surface &src_surface, const std::vector<double> &priority_values,
                                      const TileGrid &tgrid) {

	const double T = 0.5;
	const auto threshold_kill_function = [T](const double priority) -> value_type {
		return (priority > T) ? ResidualLabel::RESIDUAL_LIVE : ResidualLabel::RESIDUAL_KILL;
	};

	Surface pixel_priority_map =
	    Surface::build_from<value_type>()
	        .generate(src_surface.width(), src_surface.height(),
	                  [priority_values, tgrid, threshold_kill_function](unsigned x, unsigned y) -> value_type {
		                  const auto tile_x = x / tgrid.tile_size_x_;
		                  const auto tile_y = y / tgrid.tile_size_y_;
		                  const auto priority_value = priority_values[tile_y * tgrid.n_tiles_x_ + tile_x];
		                  const auto priority_label = threshold_kill_function(priority_value);
		                  return priority_label;
	                  })
	        .finish();
	return pixel_priority_map;
}

Surface PriorityMapVis::process(const Surface &src_plane) {
	// Read-only access to source surface
	constexpr auto scale_factor = std::numeric_limits<int16_t>::max() / std::numeric_limits<uint8_t>::max();

	const auto src_plane_view = src_plane.view_as<int16_t>();
	Surface rescaled_priority_map =
	    Surface::build_from<int16_t>()
	        .generate(src_plane.width(), src_plane.height(),
	                  [scale_factor, &src_plane_view](unsigned x, unsigned y) -> int16_t {
		                  const auto rescaled_value = static_cast<int16_t>(std::round(src_plane_view.read(x, y) * scale_factor));
		                  return rescaled_value;
	                  })
	        .finish();
	return rescaled_priority_map;
}

Surface PriorityMap::ComputeMeanValue(const Surface &src_surface) {
	const auto src_view = src_surface.view_as<int16_t>();
	auto mean = Surface::build_from<uint8_t>();
	mean.reserve((src_surface.width() + BLOCK_SIZE - 1) / BLOCK_SIZE, (src_surface.height() + BLOCK_SIZE - 1) / BLOCK_SIZE);
	for (unsigned y = 0; y < src_surface.height(); y += BLOCK_SIZE) {
		for (unsigned x = 0; x < src_surface.width(); x += BLOCK_SIZE) {
			int acc = 0;
			unsigned maxk = LCEVC_MIN(BLOCK_SIZE, src_surface.height() - y);
			unsigned maxh = LCEVC_MIN(BLOCK_SIZE, src_surface.width() - x);
			for (unsigned k = 0; k < maxk; k++) {
				for (unsigned h = 0; h < maxh; h++) {
					// for both 8 bpp and 10 bpp, work with averages of 8 bpp values
					acc += (src_view.read(x + h, y + k) >> 7);
				}
			}
			acc = acc / (BLOCK_SIZE * BLOCK_SIZE);
			mean.write(x / BLOCK_SIZE, y / BLOCK_SIZE, clamp(acc, 0, 255));
		}
	}
	return mean.finish();
}

Surface PriorityMap::ComputeContrastTexture(const Surface &src_surface, Surface &mean_surface) {
	const auto src_view = src_surface.view_as<int16_t>();
	const auto mean_view = mean_surface.view_as<uint8_t>();
	auto type = Surface::build_from<uint8_t>();
	type.reserve((src_surface.width() + BLOCK_SIZE - 1) / BLOCK_SIZE, (src_surface.height() + BLOCK_SIZE - 1) / BLOCK_SIZE);

	const float textureThreshold = 0.15f;  // 0.15f
	const float contrastThreshold = 0.10f; // 0.10f

	for (unsigned y = 0; y < src_surface.height(); y += BLOCK_SIZE) {
		for (unsigned x = 0; x < src_surface.width(); x += BLOCK_SIZE) {
			int crossings = 0;
			int mean = mean_view.read(x / BLOCK_SIZE, y / BLOCK_SIZE);
			int accLo = 0, numLo = 0;
			int accHi = 0, numHi = 0;
			unsigned maxk = LCEVC_MIN(BLOCK_SIZE, src_surface.height() - y);
			unsigned maxh = LCEVC_MIN(BLOCK_SIZE, src_surface.width() - x);
			for (unsigned k = 0; k < maxk; k++) {
				for (unsigned h = 0; h < maxh; h++) {
					// X = current; L = left (x-1); U = up (y-1);
					int pixelX = src_view.read(x + h, y + k) >> 7;

					// CONTRAST = difference in high/low mean values
					if (pixelX <= mean) {
						accLo += pixelX;
						numLo++;
					} else {
						accHi += pixelX;
						numHi++;
					}

					if (y > 0) {
						int pixelU = src_view.read(x + h, y + k - 1) >> 7;
						// TEXTURE = number of crossings
						// horizontal egde -- UP delta
						if ((pixelX > mean && pixelU <= mean) || (pixelX <= mean && pixelU > mean))
							crossings += (abs(pixelX - pixelU) > 4);
					}
					if (x > 0) {
						int pixelL = src_view.read(x + h - 1, y + k) >> 7;
						// TEXTURE = number of crossings
						// vertical edge -- LEFT delta
						if ((pixelX > mean && pixelL <= mean) || (pixelX <= mean && pixelL > mean))
							crossings += (abs(pixelX - pixelL) > 4);
					}
				}
			}
			float meanLo = 0.0f;
			float meanHi = 0.0f;
			if (numLo > 0 && numHi > 0) {
				meanLo = (float)accLo / numLo;
				meanHi = (float)accHi / numHi;
			}
			float contrast = (meanHi - meanLo) / 255.0f;
			float texture = (float)(crossings) / (2 * BLOCK_SIZE * (BLOCK_SIZE - 1));

			EnumBlockType blockType = EnumBlockType::Plain;
			if (texture <= textureThreshold) {
				// if TEXTURE is "small" classify for CONTRAST
				if (contrast <= contrastThreshold)
					blockType = EnumBlockType::Plain;
				else
					blockType = EnumBlockType::Edge;
			} else {
				// if TEXTURE is "large" classify for TEXTURE
				if (contrast <= contrastThreshold)
					blockType = EnumBlockType::SmoothTexture;
				else
					blockType = EnumBlockType::CoarseTexture;

				if ((blockType == EnumBlockType::SmoothTexture) && (contrast <= 0.02f)) {
					blockType = EnumBlockType::Plain;
				}
			}
			type.write(x / BLOCK_SIZE, y / BLOCK_SIZE, (uint8_t)blockType);
		}
	}
	return type.finish();
}

// NEED REVISITATION FOR SL-2 TO INCLUDE ADDITIONAL PM TYPE AND MODE
Surface PriorityMap::ApplyPriorityResiduals(Surface &src_surface, Surface &type_surface, unsigned step_width) {
	const auto src_view = src_surface.view_as<int16_t>();
	const auto type_view = type_surface.view_as<uint8_t>();
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_surface.width(), src_surface.height());
	EnumBlockType blockType = EnumBlockType::Plain;

	for (unsigned y = 0; y < src_surface.height(); y += BLOCK_SIZE) {
		for (unsigned x = 0; x < src_surface.width(); x += BLOCK_SIZE) {
			blockType = (EnumBlockType)type_view.read(x / BLOCK_SIZE, y / BLOCK_SIZE);
			unsigned maxk = LCEVC_MIN(BLOCK_SIZE, src_surface.height() - y);
			unsigned maxh = LCEVC_MIN(BLOCK_SIZE, src_surface.width() - x);
			if (blockType == EnumBlockType::Plain || (blockType == EnumBlockType::CoarseTexture && step_width > 300)) {
				for (unsigned k = 0; k < maxk; k++) {
					for (unsigned h = 0; h < maxh; h++) {
						dest.write(x + h, y + k, 0);
					}
				}
			} else {
				for (unsigned k = 0; k < maxk; k++) {
					for (unsigned h = 0; h < maxh; h++) {
						dest.write(x + h, y + k, src_view.read(x + h, y + k));
					}
				}
			}
		}
	}
	return dest.finish();
}

void PriorityMap::ApplyPriorityCoefficients(int width, int height, int tu_size, int sublayer, Surface src_surface[],
                                            Surface dst_surface[], const Surface &type_surface, const Surface &pixel_sad,
                                            unsigned step_width, PriorityMBType priority_mb_type, EncodingMode mode) {
	const bool sad = !pixel_sad.empty();
	if (tu_size == 4) {
		// case 4x4
		const auto src00_view = src_surface[0].view_as<int16_t>();
		const auto src01_view = src_surface[1].view_as<int16_t>();
		const auto src02_view = src_surface[2].view_as<int16_t>();
		const auto src03_view = src_surface[3].view_as<int16_t>();
		const auto src04_view = src_surface[4].view_as<int16_t>();
		const auto src05_view = src_surface[5].view_as<int16_t>();
		const auto src06_view = src_surface[6].view_as<int16_t>();
		const auto src07_view = src_surface[7].view_as<int16_t>();
		const auto src08_view = src_surface[8].view_as<int16_t>();
		const auto src09_view = src_surface[9].view_as<int16_t>();
		const auto src10_view = src_surface[10].view_as<int16_t>();
		const auto src11_view = src_surface[11].view_as<int16_t>();
		const auto src12_view = src_surface[12].view_as<int16_t>();
		const auto src13_view = src_surface[13].view_as<int16_t>();
		const auto src14_view = src_surface[14].view_as<int16_t>();
		const auto src15_view = src_surface[15].view_as<int16_t>();
		const auto pixel_sad_view = (sad ? pixel_sad.view_as<int16_t>() : src_surface[0].view_as<int16_t>());
		auto dest00 = Surface::build_from<int16_t>();
		dest00.reserve(width, height);
		auto dest01 = Surface::build_from<int16_t>();
		dest01.reserve(width, height);
		auto dest02 = Surface::build_from<int16_t>();
		dest02.reserve(width, height);
		auto dest03 = Surface::build_from<int16_t>();
		dest03.reserve(width, height);
		auto dest04 = Surface::build_from<int16_t>();
		dest04.reserve(width, height);
		auto dest05 = Surface::build_from<int16_t>();
		dest05.reserve(width, height);
		auto dest06 = Surface::build_from<int16_t>();
		dest06.reserve(width, height);
		auto dest07 = Surface::build_from<int16_t>();
		dest07.reserve(width, height);
		auto dest08 = Surface::build_from<int16_t>();
		dest08.reserve(width, height);
		auto dest09 = Surface::build_from<int16_t>();
		dest09.reserve(width, height);
		auto dest10 = Surface::build_from<int16_t>();
		dest10.reserve(width, height);
		auto dest11 = Surface::build_from<int16_t>();
		dest11.reserve(width, height);
		auto dest12 = Surface::build_from<int16_t>();
		dest12.reserve(width, height);
		auto dest13 = Surface::build_from<int16_t>();
		dest13.reserve(width, height);
		auto dest14 = Surface::build_from<int16_t>();
		dest14.reserve(width, height);
		auto dest15 = Surface::build_from<int16_t>();
		dest15.reserve(width, height);
		const auto type_view = type_surface.view_as<uint8_t>();
		EnumBlockType blockType = EnumBlockType::Plain;
		if (sublayer == LOQ_LEVEL_1) {
			for (unsigned y = 0; y < (unsigned)height; y++) {
				for (unsigned x = 0; x < (unsigned)width; x++) {

					if (mode != EncodingMode::ENCODE_ALL)
					{
						blockType = (EnumBlockType)type_view.read((x * 4) / (BLOCK_SIZE), (y * 4) / (BLOCK_SIZE));

						bool keepAverage = false;

						//Decision type
						switch (priority_mb_type)
						{
							case PriorityMBType::SmoothAndPlain: // Keep smooth and plain
								keepAverage = (blockType == EnumBlockType::Plain || blockType == EnumBlockType::SmoothTexture);
								break;
						
							case PriorityMBType::CoarseAndPlain: // Remove coarse and plain
								keepAverage = (blockType == EnumBlockType::Plain || blockType == EnumBlockType::CoarseTexture);
								break;

							case PriorityMBType::Smooth: // Keep smooth only
								keepAverage = (blockType == EnumBlockType::SmoothTexture);
								break;
						
							case PriorityMBType::Plain: // Keep plain only
								keepAverage = (blockType == EnumBlockType::Plain);
								break;
							
							case PriorityMBType::Coarse: // Keep coarse only
								keepAverage = (blockType == EnumBlockType::CoarseTexture);
								break;

							case PriorityMBType::Edge: // Keep Edges only
								keepAverage = (blockType == EnumBlockType::Edge);
								break;
						
							default: break;
						}

						// layer1 -- keep average
						if (keepAverage)
						{
							switch (mode)
							{
								case EncodingMode::Ax: // Keep all average
								{
									dest00.write(x, y, src00_view.read(x, y));
									dest01.write(x, y, src01_view.read(x, y));
									dest02.write(x, y, src02_view.read(x, y));
									dest03.write(x, y, src03_view.read(x, y));
								}
								break;
								case EncodingMode::AA: // Keep only AA
								{
									dest00.write(x, y, src00_view.read(x, y));
									dest01.write(x, y, 0);
									dest02.write(x, y, 0);
									dest03.write(x, y, 0);
								}
								break;
								case EncodingMode::NA: // Keep AV, AH and AD
								{
									dest00.write(x, y, 0);
									dest01.write(x, y, src01_view.read(x, y));
									dest02.write(x, y, src02_view.read(x, y));
									dest03.write(x, y, src03_view.read(x, y));
								}
								break;
								default: break;
							}
						}
						else
						{
							dest00.write(x, y, 0);
							dest01.write(x, y, 0);
							dest02.write(x, y, 0);
							dest03.write(x, y, 0);
						}
						dest04.write(x, y, 0);
						dest05.write(x, y, 0);
						dest06.write(x, y, 0);
						dest07.write(x, y, 0);
						dest08.write(x, y, 0);
						dest09.write(x, y, 0);
						dest10.write(x, y, 0);
						dest11.write(x, y, 0);
						dest12.write(x, y, 0);
						dest13.write(x, y, 0);
						dest14.write(x, y, 0);
						dest15.write(x, y, 0);
					}
					else
					{
						dest00.write(x, y, src00_view.read(x, y));
						dest01.write(x, y, src01_view.read(x, y));
						dest02.write(x, y, src02_view.read(x, y));
						dest03.write(x, y, src03_view.read(x, y));
						dest04.write(x, y, src04_view.read(x, y));
						dest05.write(x, y, src05_view.read(x, y));
						dest06.write(x, y, src06_view.read(x, y));
						dest07.write(x, y, src07_view.read(x, y));
						dest08.write(x, y, src08_view.read(x, y));
						dest09.write(x, y, src09_view.read(x, y));
						dest10.write(x, y, src10_view.read(x, y));
						dest11.write(x, y, src11_view.read(x, y));
						dest12.write(x, y, src12_view.read(x, y));
						dest13.write(x, y, src13_view.read(x, y));
						dest14.write(x, y, src14_view.read(x, y));
						dest15.write(x, y, src15_view.read(x, y));
					}
					
				} // for x
			} // for y
		} 
		else if (sublayer == LOQ_LEVEL_2)
		{
			for (unsigned y = 0; y < (unsigned)height; y++) {
				for (unsigned x = 0; x < (unsigned)width; x++) {
					blockType = (EnumBlockType)type_view.read((x * 4) / (BLOCK_SIZE), (y * 4) / (BLOCK_SIZE));

					bool removeCoeff = false;

					// Only apply priority map if block is non-static
					if (sad && pixel_sad_view.read(x, y) > 1000) {
						// Decision type
						switch (priority_mb_type) {
						case PriorityMBType::SmoothAndPlain: // Remove smooth and plain
							removeCoeff = (blockType == EnumBlockType::Plain ||
							               (blockType == EnumBlockType::SmoothTexture && step_width > 300));
							break;

						case PriorityMBType::CoarseAndPlain: // Remove coarse and plain
							removeCoeff = (blockType == EnumBlockType::Plain ||
							               (blockType == EnumBlockType::CoarseTexture && step_width > 300));
							break;

						case PriorityMBType::Smooth: // Remove smooth only
							removeCoeff = (blockType == EnumBlockType::SmoothTexture && step_width > 300);
							break;

						case PriorityMBType::Plain: // Remove plain only
							removeCoeff = (blockType == EnumBlockType::Plain);
							break;

						case PriorityMBType::Coarse: // Remove coarse only
							removeCoeff = (blockType == EnumBlockType::CoarseTexture && step_width > 300);
							break;

						case PriorityMBType::Edge: // Remove Edges only
							removeCoeff = (blockType == EnumBlockType::Edge);
							break;

						default:
							break;
						}
					}

					if (removeCoeff) {
						// layer2 -- plain or coarse_texture
						dest00.write(x, y, 0);
						dest01.write(x, y, 0);
						dest02.write(x, y, 0);
						dest03.write(x, y, 0);
						dest04.write(x, y, 0);
						dest05.write(x, y, 0);
						dest06.write(x, y, 0);
						dest07.write(x, y, 0);
						dest08.write(x, y, 0);
						dest09.write(x, y, 0);
						dest10.write(x, y, 0);
						dest11.write(x, y, 0);
						dest12.write(x, y, 0);
						dest13.write(x, y, 0);
						dest14.write(x, y, 0);
						dest15.write(x, y, 0);
					} else {
						// layer2 -- edge or smooth_texture
						dest00.write(x, y, src00_view.read(x, y));
						dest01.write(x, y, src01_view.read(x, y));
						dest02.write(x, y, src02_view.read(x, y));
						dest03.write(x, y, src03_view.read(x, y));
						dest04.write(x, y, src04_view.read(x, y));
						dest05.write(x, y, src05_view.read(x, y));
						dest06.write(x, y, src06_view.read(x, y));
						dest07.write(x, y, src07_view.read(x, y));
						dest08.write(x, y, src08_view.read(x, y));
						dest09.write(x, y, src09_view.read(x, y));
						dest10.write(x, y, src10_view.read(x, y));
						dest11.write(x, y, src11_view.read(x, y));
						dest12.write(x, y, src12_view.read(x, y));
						dest13.write(x, y, src13_view.read(x, y));
						dest14.write(x, y, src14_view.read(x, y));
						dest15.write(x, y, src15_view.read(x, y));
					}
				} // for x
			}     // for y
		}
		dst_surface[0] = dest00.finish();
		dst_surface[1] = dest01.finish();
		dst_surface[2] = dest02.finish();
		dst_surface[3] = dest03.finish();
		dst_surface[4] = dest04.finish();
		dst_surface[5] = dest05.finish();
		dst_surface[6] = dest06.finish();
		dst_surface[7] = dest07.finish();
		dst_surface[8] = dest08.finish();
		dst_surface[9] = dest09.finish();
		dst_surface[10] = dest10.finish();
		dst_surface[11] = dest11.finish();
		dst_surface[12] = dest12.finish();
		dst_surface[13] = dest13.finish();
		dst_surface[14] = dest14.finish();
		dst_surface[15] = dest15.finish();
	} else if (tu_size == 2) {
		// case 2x2
		const auto src00_view = src_surface[0].view_as<int16_t>();
		const auto src01_view = src_surface[1].view_as<int16_t>();
		const auto src02_view = src_surface[2].view_as<int16_t>();
		const auto src03_view = src_surface[3].view_as<int16_t>();
		const auto pixel_sad_view = (sad ? pixel_sad.view_as<int16_t>() : src_surface[0].view_as<int16_t>());
		auto dest00 = Surface::build_from<int16_t>();
		dest00.reserve(width, height);
		auto dest01 = Surface::build_from<int16_t>();
		dest01.reserve(width, height);
		auto dest02 = Surface::build_from<int16_t>();
		dest02.reserve(width, height);
		auto dest03 = Surface::build_from<int16_t>();
		dest03.reserve(width, height);
		const auto type_view = type_surface.view_as<uint8_t>();
		EnumBlockType blockType = EnumBlockType::Plain;
		if (sublayer == LOQ_LEVEL_1) {
			for (unsigned y = 0; y < (unsigned)height; y++) {
				for (unsigned x = 0; x < (unsigned)width; x++) {
					blockType = (EnumBlockType)type_view.read((x * 2) / (BLOCK_SIZE), (y * 2) / (BLOCK_SIZE));

					bool keepAverage = false;
					
					switch (priority_mb_type)
					{
						case PriorityMBType::SmoothAndPlain: // Keep smooth and plain
							keepAverage = (blockType == EnumBlockType::Plain || blockType == EnumBlockType::SmoothTexture);
							break;
					
						case PriorityMBType::CoarseAndPlain: // Remove coarse and plain
							keepAverage = (blockType == EnumBlockType::Plain || blockType == EnumBlockType::CoarseTexture);
							break;
					
						case PriorityMBType::Smooth: // Keep smooth only
							keepAverage = (blockType == EnumBlockType::SmoothTexture);
							break;
					
						case PriorityMBType::Plain: // Keep plain only
							keepAverage = (blockType == EnumBlockType::Plain);
							break;
						
						case PriorityMBType::Coarse: // Keep coarse only
							keepAverage = (blockType == EnumBlockType::CoarseTexture);
							break;
					
						case PriorityMBType::Edge: // Keep Edges only
							keepAverage = (blockType == EnumBlockType::Edge);
							break;
					
						default: break;
					}
					
					// layer1 -- keep average
					if (keepAverage) {
						dest00.write(x, y, src00_view.read(x, y));
					} else {
						dest00.write(x, y, 0);
					}
					dest01.write(x, y, 0);
					dest02.write(x, y, 0);
					dest03.write(x, y, 0);
				} // for x
			}     // for y
		} else if (sublayer == LOQ_LEVEL_2) {
			for (unsigned y = 0; y < (unsigned)height; y++) {
				for (unsigned x = 0; x < (unsigned)width; x++) {
					blockType = (EnumBlockType)type_view.read((x * 2) / (BLOCK_SIZE), (y * 2) / (BLOCK_SIZE));

					bool removeCoeff = false;

					// Only apply priority map if block is non-static
					if (step_width > 300 && sad && pixel_sad_view.read(x, y) > 500) {
						// Decision type
						switch (priority_mb_type) {
						case PriorityMBType::SmoothAndPlain: // Remove smooth and plain
							removeCoeff = (blockType == EnumBlockType::Plain ||
							               (blockType == EnumBlockType::SmoothTexture && step_width > 300));
							break;

						case PriorityMBType::CoarseAndPlain: // Remove coarse and plain
							removeCoeff = (blockType == EnumBlockType::Plain ||
							               (blockType == EnumBlockType::CoarseTexture && step_width > 300));
							break;

						case PriorityMBType::Smooth: // Remove smooth only
							removeCoeff = (blockType == EnumBlockType::SmoothTexture && step_width > 300);
							break;

						case PriorityMBType::Plain: // Remove plain only
							removeCoeff = (blockType == EnumBlockType::Plain);
							break;

						case PriorityMBType::Coarse: // Remove coarse only
							removeCoeff = (blockType == EnumBlockType::CoarseTexture && step_width > 300);
							break;

						case PriorityMBType::Edge: // Remove Edges only
							removeCoeff = (blockType == EnumBlockType::Edge);
							break;

						default:
							break;
						}
					}

					if (removeCoeff) {
						// layer2 -- plain or coarse_texture
						dest00.write(x, y, 0);
						dest01.write(x, y, 0);
						dest02.write(x, y, 0);
						dest03.write(x, y, 0);
					} else {
						// layer2 -- edge or smooth_texture
						dest00.write(x, y, src00_view.read(x, y));
						dest01.write(x, y, src01_view.read(x, y));
						dest02.write(x, y, src02_view.read(x, y));
						dest03.write(x, y, src03_view.read(x, y));
					}
				} // for x
			}     // for y
		}
		dst_surface[0] = dest00.finish();
		dst_surface[1] = dest01.finish();
		dst_surface[2] = dest02.finish();
		dst_surface[3] = dest03.finish();
	} // tu_size = 4 or 2
	
	return;
}

void StaticResiduals::process(Surface src_coeffs[], Surface dst_coeffs[], const Surface &pixel_sad_plane, unsigned num_layers,
                              unsigned step_width, unsigned sad_threshold, unsigned sad_coeff_threshold) {
	const auto pixel_sad = pixel_sad_plane.view_as<int16_t>();

	for (unsigned layer = 0; layer < num_layers; ++layer) {
		const auto src_view = src_coeffs[layer].view_as<int16_t>();
		dst_coeffs[layer] = Surface::build_from<int16_t>()
		                        .generate(src_view.width(), src_view.height(),
		                                  [&](unsigned x, unsigned y) -> int16_t {
			                                  if ((unsigned)pixel_sad.read(x, y) >= sad_threshold) {
				                                  return (unsigned)std::abs(src_view.read(x, y)) > sad_coeff_threshold * step_width
				                                             ? src_view.read(x, y)
				                                             : 0;
			                                  } else {
				                                  return src_view.read(x, y);
			                                  }
		                                  })
		                        .finish();
	}
}

} // namespace lctm
