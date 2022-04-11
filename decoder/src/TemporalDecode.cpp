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
//
// TemporalRefresh.cpp
//

#include "TemporalDecode.hpp"
#include "Misc.hpp"

#include <algorithm>

namespace lctm {

// Extract the single-bit-per-transform signalling as a mask
//
Surface TemporalExtractMask::process(const Surface &symbols) {
	const auto src_symbols = symbols.view_as<int16_t>();

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint8_t>();
	dest.reserve(src_symbols.width(), src_symbols.height());
	for (unsigned y = 0; y < src_symbols.height(); ++y) {
		const int16_t *__restrict psrc = src_symbols.data(0, y);
		uint8_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_symbols.width(); ++x) {
			// dest.write(x, y, (src_symbols.read(x, y) & 1) ? TemporalType::TEMPORAL_INTR : TemporalType::TEMPORAL_PRED);
			*pdst++ = ((*psrc++ & 1) ? TemporalType::TEMPORAL_INTR : TemporalType::TEMPORAL_PRED);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint8_t>()
	    .generate(src_symbols.width(), src_symbols.height(),
	              [&](unsigned x, unsigned y) -> uint8_t {
		              return (src_symbols.read(x, y) & 1) ? TemporalType::TEMPORAL_INTR : TemporalType::TEMPORAL_PRED;
	              })
	    .finish();
#endif
}

// Filter out the single-bit-per-transform signalling as a mask
//
Surface TemporalClear::process(const Surface &symbols) {
	const auto src_symbols = symbols.view_as<int16_t>();

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_symbols.width(), src_symbols.height());
	for (unsigned y = 0; y < src_symbols.height(); ++y) {
		const int16_t *__restrict psrc = src_symbols.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_symbols.width(); ++x) {
			// dest.write(x, y, (src_symbols.read(x, y) >> 1));
			*pdst++ = (*psrc++ >> 1);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate(src_symbols.width(), src_symbols.height(),
	              [&](unsigned x, unsigned y) -> int16_t { return src_symbols.read(x, y) >> 1; })
	    .finish();
#endif
}

// Combine the previous temporal buffer, the current residuals and the signalling mask to make a new temporal buffer
//
Surface TemporalUpdate::process(const Surface &temporal_plane, const Surface &residuals_plane, Surface &mask_plane,
                                unsigned transform_block_size, bool per_picture_intra, bool use_reduced_signalling) {
	if (mask_plane.empty()) {
		mask_plane = Surface::build_from<uint8_t>()
		                 .generate(residuals_plane.width() / transform_block_size, residuals_plane.height() / transform_block_size,
		                           [&](unsigned x, unsigned y) -> uint8_t { return TemporalType::TEMPORAL_INTR; })
		                 .finish();
	}
	const auto src_temporal = temporal_plane.view_as<int16_t>();
	const auto src_residuals = residuals_plane.view_as<int16_t>();
	const auto src_mask = mask_plane.view_as<uint8_t>();

	const unsigned tb_shift = log2(transform_block_size);
	const unsigned d = 32 / transform_block_size;

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_temporal.width(), src_temporal.height());
	for (unsigned y = 0; y < src_temporal.height(); ++y) {
		const int16_t *__restrict psrc = src_residuals.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_temporal.width(); ++x) {
			const bool per_tile_intra =
			    use_reduced_signalling && (src_mask.read((x >> 5) * d, (y >> 5) * d) == TemporalType::TEMPORAL_INTR);
			const bool per_block_intra = (src_mask.read(x >> tb_shift, y >> tb_shift) == TemporalType::TEMPORAL_INTR);
			// int16_t out = 0;
			if (per_picture_intra || per_tile_intra || per_block_intra) {
				// out = src_residuals.read(x, y);
				*pdst++ = *psrc++;
			} else {
				// out = src_residuals.read(x, y) + src_temporal.read(x, y);
				*pdst++ = *psrc++ + src_temporal.read(x, y);
			}
			// dest.write(x, y, out);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate(src_temporal.width(), src_temporal.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              const bool per_tile_intra =
#if defined __OPT_DIVISION__
		                  use_reduced_signalling && (src_mask.read((x >> 5) * d, (y >> 5) * d) == TemporalType::TEMPORAL_INTR);
#else
		                  use_reduced_signalling && (src_mask.read((x / 32) * d, (y / 32) * d) == TemporalType::TEMPORAL_INTR);
#endif
		              const bool per_block_intra = (src_mask.read(x >> tb_shift, y >> tb_shift) == TemporalType::TEMPORAL_INTR);

		              if (per_picture_intra || per_tile_intra || per_block_intra) {
			              return src_residuals.read(x, y);
		              } else {
			              return src_residuals.read(x, y) + src_temporal.read(x, y);
		              }
	              })
	    .finish();
#endif
}

// Perform a per-tile temporal decision based upon the calculated residuals
// for each pass and store result in a tile-map.
//

const std::vector<SurfaceView<int16_t>> TemporalTileMap::collect_surfaces_DD(Surface symbols[MAX_NUM_LAYERS]) {
	using view_t = SurfaceView<int16_t>;
	const std::vector<view_t> surfaces = {view_t{symbols[0]}, view_t{symbols[1]}, view_t{symbols[2]}, view_t{symbols[3]}};
	return surfaces;
}

const std::vector<SurfaceView<int16_t>> TemporalTileMap::collect_surfaces_DDS(Surface symbols[MAX_NUM_LAYERS]) {
	using view_t = SurfaceView<int16_t>;
	const std::vector<view_t> surfaces = {view_t{symbols[0]},  view_t{symbols[1]},  view_t{symbols[2]},  view_t{symbols[3]},
	                                      view_t{symbols[4]},  view_t{symbols[5]},  view_t{symbols[6]},  view_t{symbols[7]},
	                                      view_t{symbols[8]},  view_t{symbols[9]},  view_t{symbols[10]}, view_t{symbols[11]},
	                                      view_t{symbols[12]}, view_t{symbols[13]}, view_t{symbols[14]}, view_t{symbols[15]}};
	return surfaces;
}

Surface TemporalTileMap::process(Surface intra_symbols[MAX_NUM_LAYERS], Surface inter_symbols[MAX_NUM_LAYERS],
                                 unsigned transform_block_size) {
	const unsigned tile_size = 32;
	const unsigned transforms_per_tile = tile_size / transform_block_size;

	const unsigned level_width = intra_symbols[0].width() * transform_block_size;
	const unsigned level_height = intra_symbols[0].height() * transform_block_size;

	const unsigned tiles_wide = (level_width + tile_size - 1) / tile_size;
	const unsigned tiles_high = (level_height + tile_size - 1) / tile_size;

	// These all have S=2 as template param in inverse.
	const std::vector<SurfaceView<int16_t>> srcs_intra =
	    (transform_block_size == 2) ? collect_surfaces_DD(intra_symbols) : collect_surfaces_DDS(intra_symbols);

	const std::vector<SurfaceView<int16_t>> srcs_inter =
	    (transform_block_size == 2) ? collect_surfaces_DD(inter_symbols) : collect_surfaces_DDS(inter_symbols);

	return Surface::build_from<uint8_t>()
	    .generate(tiles_wide, tiles_high,
	              [&](unsigned x, unsigned y) -> uint8_t {
		              // This can be based on 2x2 regions, even for DDS.
		              unsigned both_z = 0;
		              unsigned inter_nz = 0;
		              unsigned inter_z = 0;
		              unsigned intra_nz = 0;
		              unsigned intra_z = 0;
		              unsigned inter_accum = 0;
		              unsigned intra_accum = 0;
		              unsigned sav_mixed = 0;

		              for (unsigned tile_y = y * transforms_per_tile; tile_y < (y + 1) * transforms_per_tile; ++tile_y) {
			              if (tile_y >= intra_symbols[0].height())
				              continue;

			              for (unsigned tile_x = x * transforms_per_tile; tile_x < (x + 1) * transforms_per_tile; ++tile_x) {
				              if (tile_x >= intra_symbols[0].width())
					              continue;

				              // for each transform in each tile.
				              unsigned intra_sav = 0;
				              unsigned inter_sav = 0;

				              for (unsigned l = 0; l < transform_block_size * transform_block_size; ++l) {
					              intra_sav += std::abs(srcs_intra[l].read(tile_x, tile_y));
					              inter_sav += std::abs(srcs_inter[l].read(tile_x, tile_y));
				              }

				              if (inter_sav == 0 && intra_sav == 0) {
					              both_z++;
				              } else if (inter_sav == 0) {
					              inter_z++;
				              } else if (intra_sav == 0) {
					              intra_z++;
				              } else if (intra_sav < inter_sav) {
					              intra_nz++;
				              } else {
					              inter_nz++;
				              }

				              intra_accum += intra_sav;
				              inter_accum += inter_sav;
				              // sav_mixed += std::min(intra_sav, inter_sav);
				              sav_mixed += LCEVC_MIN(intra_sav, inter_sav);
			              }
		              }

		              unsigned num_temporals = intra_z + intra_nz + inter_z + inter_nz;
		              unsigned intra_pct = (100 * (intra_z + intra_nz)) / (num_temporals + 1);
		              unsigned inter_pct = (100 * inter_z) / (num_temporals + 1);

		              unsigned intra_accum_75 = intra_accum - (inter_accum >> 2);
		              unsigned inter_accum_25 = inter_accum >> 2;

		              if (intra_pct > 38 && inter_pct < 20 && (intra_accum_75 <= sav_mixed || inter_accum_25 > sav_mixed)) {
			              return TemporalType::TEMPORAL_INTR;
		              }

		              return TemporalType::TEMPORAL_PRED;
	              })
	    .finish();
}

// Perform sweep updating temporal mask based upon tile type with reduced signalling.
//
Surface TemporalTileIntraSignal::process(const Surface &temporal_tile_map, const Surface &mask_plane,
                                         unsigned transform_block_size) {
	const unsigned transforms_per_tile = 32 / transform_block_size;
	const auto src_tile_map = temporal_tile_map.view_as<uint8_t>();
	const auto src_mask_plane = mask_plane.view_as<uint8_t>();

	return Surface::build_from<uint8_t>()
	    .generate(mask_plane.width(), mask_plane.height(),
	              [&](unsigned x, unsigned y) -> uint8_t {
		              const unsigned tile_x = x / transforms_per_tile;
		              const unsigned tile_y = y / transforms_per_tile;

		              const bool tile_intra = (src_tile_map.read(tile_x, tile_y) == TemporalType::TEMPORAL_INTR);
		              const bool tile_start = ((x % transforms_per_tile) == 0) && ((y % transforms_per_tile) == 0);

		              if (tile_intra)
			              return TemporalType::TEMPORAL_INTR;
		              else if (tile_start)
			              return TemporalType::TEMPORAL_PRED;

		              // Pass through the mask based on cost
		              return src_mask_plane.read(x, y);
	              })
	    .finish();
}

// Clear out the symbols where the temporal tile type is intra.
//
Surface TemporalTileIntraClear::process(Surface &temporal_tile_map, const Surface &temporal_signal_syms,
                                        unsigned transform_block_size) {
	// transforms_per_tile is 8 for (4x4) and 16 for (2x2)
	const unsigned transforms_per_tile = 32 / transform_block_size;
	// when tile map is empty, create a default one
	if (temporal_tile_map.empty()) {
		temporal_tile_map =
		    Surface::build_from<uint8_t>()
		        .generate(temporal_signal_syms.width() / transforms_per_tile, temporal_signal_syms.height() / transforms_per_tile,
		                  [&](unsigned x, unsigned y) -> uint8_t { return TemporalType::TEMPORAL_INTR; })
		        .finish();
	}
	const auto src_tile_map = temporal_tile_map.view_as<uint8_t>();
	const auto src_syms = temporal_signal_syms.view_as<int16_t>();

	return Surface::build_from<int16_t>()
	    .generate(temporal_signal_syms.width(), temporal_signal_syms.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              const unsigned tile_x = x / transforms_per_tile;
		              const unsigned tile_y = y / transforms_per_tile;

		              const bool tile_intra = (src_tile_map.read(tile_x, tile_y) == TemporalType::TEMPORAL_INTR);
		              const bool tile_start = ((x % transforms_per_tile) == 0) && ((y % transforms_per_tile) == 0);

		              if (tile_start || !tile_intra) {
			              return src_syms.read(x, y);
		              }

		              return 0;
	              })
	    .finish();
}

// For each block in map - zero prediction if corresponding block is set to INTRA in map
//
Surface ApplyTemporalMap::process(const Surface &src_plane, Surface &map_plane, unsigned transform_block_size) {

	// Scale the residual map to cover source image
	//
	const unsigned block_w = tile_size(map_plane.width(), src_plane.width());
	const unsigned block_h = tile_size(map_plane.height(), src_plane.height());
	const unsigned shift_bw = log2(block_w);
	const unsigned shift_bh = log2(block_h);

	CHECK(src_plane.width() == map_plane.width() * block_w);
	CHECK(src_plane.height() == map_plane.height() * block_h);

	const auto src = src_plane.view_as<int16_t>();
	const auto map = map_plane.view_as<uint8_t>();

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src.width(), src.height());
	for (unsigned y = 0; y < src.height(); ++y) {
		const int16_t *__restrict psrc = src.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src.width(); ++x) {
			// apply the "buffer refresh" per block
			if (map.read(x >> shift_bw, y >> shift_bh) == TemporalType::TEMPORAL_INTR)
				// dest.write(x, y, 0);
				*pdst++ = 0;
			else
				// dest.write(x, y, src.read(x, y));
				*pdst++ = *psrc;
			psrc++;
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate(src.width(), src.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              // apply the "buffer refresh" per block
		              if (map.read(x >> shift_bw, y >> shift_bh) == TemporalType::TEMPORAL_INTR)
			              return 0;
		              else
			              return src.read(x, y);
	              })
	    .finish();
#endif
}

// Filter out the embedded user_data
//
Surface UserDataClear::process(const Surface &symbols, UserDataMode user_data) {
	unsigned size;
	switch (user_data) {
	case lctm::UserData_2bits:
		size = 2;
		break;
	case lctm::UserData_6bits:
		size = 6;
		break;
	default:
		CHECK(0);
		break;
	}
	const auto src_symbols = symbols.view_as<int16_t>();

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(src_symbols.width(), src_symbols.height());
	for (unsigned y = 0; y < src_symbols.height(); ++y) {
		const int16_t *__restrict psrc = src_symbols.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < src_symbols.width(); ++x) {
			uint16_t value = *psrc++;
			value >>= size;
			bool sign = (value & 0x01) == 0x00 ? false : true;
			value >>= 1;
			*pdst++ = (int16_t)(sign ? (-value) : (value));
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate(src_symbols.width(), src_symbols.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              uint16_t value = src_symbols.read(x, y);
		              value >>= size;
		              bool sign = (value & 0x01) == 0x00 ? false : true;
		              value >>= 1;
		              return (int16_t)(sign ? (-value) : (value));
	              })
	    .finish();
#endif
}

} // namespace lctm
