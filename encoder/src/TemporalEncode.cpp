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
// TemporalRefresh.cpp
//
// Processes used to generate the various temporal refresh signals (per frame, per tile, per block)
//

#include "TemporalEncode.hpp"

#include "Misc.hpp"
#include "TemporalDecode.hpp"

#include <algorithm>

namespace lctm {

// Generate cost for each block
//
Surface TemporalCost_2x2::process(const Surface &sour_plane, const Surface &reco_plane, const Surface *symb_plane,
                                  unsigned transform_block_size, unsigned scale, bool intra) {

	CHECK(sour_plane.width() == reco_plane.width() && sour_plane.height() == reco_plane.height());
	CHECK(sour_plane.width() == symb_plane[0].width() * transform_block_size &&
	      sour_plane.height() == symb_plane[0].height() * transform_block_size);

	const auto src = sour_plane.view_as<int16_t>();
	const auto recon = reco_plane.view_as<int16_t>();
	const auto symb00 = symb_plane[0].view_as<int16_t>();
	const auto symb01 = symb_plane[1].view_as<int16_t>();
	const auto symb02 = symb_plane[2].view_as<int16_t>();
	const auto symb03 = symb_plane[3].view_as<int16_t>();

	const unsigned dst_width = src.width() / transform_block_size;
	const unsigned dst_height = src.height() / transform_block_size;
	const float    lambda     = ((float)(scale) *0.6f);

	return Surface::build_from<int16_t>()
	    .generate(dst_width, dst_height,
	              [&](unsigned block_x, unsigned block_y) -> int16_t {
		              // SAD term
		              int32_t t0 = 0;
		              for (unsigned int x = (block_x)*transform_block_size; x < (block_x + 1) * transform_block_size; ++x) {
			              for (unsigned int y = (block_y)*transform_block_size; y < (block_y + 1) * transform_block_size; ++y) {
				              t0 += abs(src.read(x, y) - recon.read(x, y));
			              }
		              }

		              // non zero coefficients term
		              int32_t t1 = 0;
		              t1 = (symb00.read(block_x, block_y) ? 1 : 0) + (intra ? 1 : (symb01.read(block_x, block_y) ? 1 : 0)) +
		                   (symb02.read(block_x, block_y) ? 1 : 0) + (symb03.read(block_x, block_y) ? 1 : 0);

		              // Intra signalling term
		              int32_t t2 = 0;

		              float fCost = (float)(t0 + (lambda  * (t1 + t2)));
		              int32_t iCost = (fCost < ((1 << 15) - 1) ? (int)fCost : ((1 << 15) - 1));
		              return (int16_t)(iCost);
	              })
	    .finish();
}

Surface TemporalCost_4x4::process(const Surface &sour_plane, const Surface &reco_plane, const Surface *symb_plane,
                                  unsigned transform_block_size, unsigned scale, bool intra) {

	CHECK(sour_plane.width() == reco_plane.width() && sour_plane.height() == reco_plane.height());
	CHECK(sour_plane.width() == symb_plane[0].width() * transform_block_size &&
	      sour_plane.height() == symb_plane[0].height() * transform_block_size);

	const auto src = sour_plane.view_as<int16_t>();
	const auto recon = reco_plane.view_as<int16_t>();
	const auto symb00 = symb_plane[0].view_as<int16_t>();
	const auto symb01 = symb_plane[1].view_as<int16_t>();
	const auto symb02 = symb_plane[2].view_as<int16_t>();
	const auto symb03 = symb_plane[3].view_as<int16_t>();
	const auto symb04 = symb_plane[4].view_as<int16_t>();
	const auto symb05 = symb_plane[5].view_as<int16_t>();
	const auto symb06 = symb_plane[6].view_as<int16_t>();
	const auto symb07 = symb_plane[7].view_as<int16_t>();
	const auto symb08 = symb_plane[8].view_as<int16_t>();
	const auto symb09 = symb_plane[9].view_as<int16_t>();
	const auto symb10 = symb_plane[10].view_as<int16_t>();
	const auto symb11 = symb_plane[11].view_as<int16_t>();
	const auto symb12 = symb_plane[12].view_as<int16_t>();
	const auto symb13 = symb_plane[13].view_as<int16_t>();
	const auto symb14 = symb_plane[14].view_as<int16_t>();
	const auto symb15 = symb_plane[15].view_as<int16_t>();

	const unsigned dst_width = src.width() / transform_block_size;
	const unsigned dst_height = src.height() / transform_block_size;

	return Surface::build_from<int16_t>()
	    .generate(dst_width, dst_height,
	              [&](unsigned block_x, unsigned block_y) -> int16_t {
		              // SAD term
		              int32_t t0 = 0;
		              for (unsigned int x = (block_x)*transform_block_size; x < (block_x + 1) * transform_block_size; ++x) {
			              for (unsigned int y = (block_y)*transform_block_size; y < (block_y + 1) * transform_block_size; ++y) {
				              t0 += abs(src.read(x, y) - recon.read(x, y));
			              }
		              }

		              // non zero residuals term
		              int32_t t1 = 0;
		              t1 = (symb00.read(block_x, block_y) ? 1 : 0) + (symb01.read(block_x, block_y) ? 1 : 0) +
		                   (symb02.read(block_x, block_y) ? 1 : 0) + (symb03.read(block_x, block_y) ? 1 : 0) +
		                   (symb04.read(block_x, block_y) ? 1 : 0) + (intra ? 1 : (symb05.read(block_x, block_y) ? 1 : 0)) +
		                   (symb06.read(block_x, block_y) ? 1 : 0) + (symb07.read(block_x, block_y) ? 1 : 0) +
		                   (symb08.read(block_x, block_y) ? 1 : 0) + (symb09.read(block_x, block_y) ? 1 : 0) +
		                   (symb10.read(block_x, block_y) ? 1 : 0) + (symb11.read(block_x, block_y) ? 1 : 0) +
		                   (symb12.read(block_x, block_y) ? 1 : 0) + (symb13.read(block_x, block_y) ? 1 : 0) +
		                   (symb14.read(block_x, block_y) ? 1 : 0) + (symb15.read(block_x, block_y) ? 1 : 0);

		              // Intra signalling term
		              int32_t t2 = 0;

		              float fCost = (float)(t0 + scale * (t1 + t2));
		              int32_t iCost = (fCost < ((1 << 15) - 1) ? (int)fCost : ((1 << 15) - 1));
		              return (int16_t)(iCost);
	              })
	    .finish();
}

// Calculate temporal cost solely based on SAD, used for no_enhancement part
Surface TemporalCost_SAD::process(const Surface &sour_plane, const Surface &reco_plane, unsigned transform_block_size) {
	const unsigned dst_width = sour_plane.width() / transform_block_size;
	const unsigned dst_height = sour_plane.height() / transform_block_size;

	if (!reco_plane.empty()) {
		// SAD
		CHECK(sour_plane.width() == reco_plane.width() && sour_plane.height() == reco_plane.height());
		const auto src = sour_plane.view_as<int16_t>();
		const auto recon = reco_plane.view_as<int16_t>();
		return Surface::build_from<int16_t>()
		    .generate(dst_width, dst_height,
		              [&](unsigned block_x, unsigned block_y) -> int16_t {
			              // SAD term
			              int32_t t0 = 0;
			              for (unsigned int x = (block_x)*transform_block_size; x < (block_x + 1) * transform_block_size; ++x) {
				              for (unsigned int y = (block_y)*transform_block_size; y < (block_y + 1) * transform_block_size; ++y) {
					              t0 += abs(src.read(x, y) - recon.read(x, y));
				              }
			              }
			              int32_t iCost = (t0 < ((1 << 15) - 1) ? (int)t0 : ((1 << 15) - 1));
			              return (int16_t)(iCost);
		              })
		    .finish();
	} else if (sour_plane.bpp() == 1) {
		// Sum of absolute values (uint8_t)
		const auto src = sour_plane.view_as<uint8_t>();
		return Surface::build_from<int16_t>()
		    .generate(dst_width, dst_height,
		              [&](unsigned block_x, unsigned block_y) -> int16_t {
			              // SAD term
			              int32_t t0 = 0;
			              for (unsigned int x = (block_x)*transform_block_size; x < (block_x + 1) * transform_block_size; ++x) {
				              for (unsigned int y = (block_y)*transform_block_size; y < (block_y + 1) * transform_block_size; ++y) {
					              t0 += abs(src.read(x, y));
				              }
			              }
			              int32_t iCost = (t0 < ((1 << 15) - 1) ? (int)t0 : ((1 << 15) - 1));
			              return (int16_t)(iCost);
		              })
		    .finish();
	} else {
		// Sum of absolute values (uint16_t)
		const auto src = sour_plane.view_as<uint16_t>();
		return Surface::build_from<int16_t>()
		    .generate(dst_width, dst_height,
		              [&](unsigned block_x, unsigned block_y) -> int16_t {
			              // SAD term
			              int32_t t0 = 0;
			              for (unsigned int x = (block_x)*transform_block_size; x < (block_x + 1) * transform_block_size; ++x) {
				              for (unsigned int y = (block_y)*transform_block_size; y < (block_y + 1) * transform_block_size; ++y) {
					              t0 += abs(src.read(x, y));
				              }
			              }
			              int32_t iCost = (t0 < ((1 << 15) - 1) ? (int)t0 : ((1 << 15) - 1));
			              return (int16_t)(iCost);
		              })
		    .finish();
	}
}

// Given the new temporal buffer, the previous temporal buffer and the signalling mask - generate the transmitted residuals
//
Surface TemporalSelect::process(const Surface &inter_plane, const Surface &intra_plane, const Surface &mask_plane) {

	CHECK(inter_plane.width() == intra_plane.width() && inter_plane.height() == intra_plane.height());

	// Scale the mask to cover source images
	const unsigned block_w = tile_size(mask_plane.width(), inter_plane.width());
	const unsigned block_h = tile_size(mask_plane.height(), inter_plane.height());
	const unsigned shift_w = log2(block_w);
	const unsigned shift_h = log2(block_h);

	const auto src_inter = inter_plane.view_as<int16_t>();
	const auto src_intra = intra_plane.view_as<int16_t>();
	const auto src_mask = mask_plane.view_as<uint8_t>();

	return Surface::build_from<int16_t>()
	    .generate(src_inter.width(), src_inter.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              if (src_mask.read(x >> shift_w, y >> shift_h) == TemporalType::TEMPORAL_PRED) {
			              return src_inter.read(x, y);
		              } else {
			              return src_intra.read(x, y);
		              }
	              })
	    .finish();
}

// Insert the single-bit-per-transform signalling as a mask
//
Surface TemporalInsertMask::process(const Surface &symbols, const Surface &mask, const bool refresh) {
	const auto src_symbols = symbols.view_as<int16_t>();

	if (!mask.empty() && !refresh) {
		CHECK(symbols.width() == mask.width() && symbols.height() == mask.height());
		const auto src_mask = mask.view_as<uint8_t>();

		return Surface::build_from<int16_t>()
		    .generate(src_symbols.width(), src_symbols.height(),
		              [&](unsigned x, unsigned y) -> int16_t {
			              return (src_symbols.read(x, y) * 2) | (src_mask.read(x, y) == TemporalType::TEMPORAL_INTR ? 1 : 0);
		              })
		    .finish();
	} else {
		return Surface::build_from<int16_t>()
		    .generate(src_symbols.width(), src_symbols.height(),
		              [&](unsigned x, unsigned y) -> int16_t { return (src_symbols.read(x, y) * 2); })
		    .finish();
	}
}

// Insert the user_data
//
Surface UserDataInsert::process(const Surface &symbols, UserDataMethod method, UserDataMode user_data, FILE* file) {
	unsigned size;
	switch (user_data) {
	case UserData_2bits:
		size = 2;
		break;
	case UserData_6bits:
		size = 6;
		break;
	default:
		CHECK(0);
	}

	const auto src_symbols = symbols.view_as<int16_t>();
	return Surface::build_from<int16_t>()
	    .generate(src_symbols.width(), src_symbols.height(),
	              [&](unsigned x, unsigned y) -> int16_t {
		              int16_t value = src_symbols.read(x, y);
		              const bool sign = value < 0;
		              uint16_t value_abs = std::min((uint16_t)abs(value), (uint16_t)(((uint16_t)0x1fff) >> (size + 1)));
		              unsigned char data;
		              switch (method) {
		              case UserDataMethod_Zeros:
			              data = 0x00;
			              break;
		              case UserDataMethod_Ones:
			              data = 0xff;
			              break;
		              case UserDataMethod_FixedRandom:
		              case UserDataMethod_Random:
			              data = Random().rand();
			              break;
		              default:
			              CHECK(0);
		              }
		              data &= (size == 6 ? 0x3f : 0x03);
#if USER_DATA_EXTRACTION
		              fwrite(&data, 1, 1, file);
#endif
		              return (int16_t)(((value_abs << (size + 1)) | data) | (sign << size));
	              })
	    .finish();
}

} // namespace lctm
