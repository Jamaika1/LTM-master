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
// Dithering.cpp
//

#include "Dithering.hpp"

#include <time.h>

namespace lctm {

//// Pseudo-random number generator
// RAND_MAX assumed to be 32767
static unsigned long random_next;

int Random::rand() {
	random_next = random_next * 1103515245 + 12345;
	return ((unsigned)(random_next / 65536) % 32768);
}

void Random::srand(unsigned seed) { random_next = seed; }

void Dithering::make_buffer(int strength, int enhancement_depth, bool fixed_seed) {
	strength = strength * (1 << (15 - enhancement_depth)); // scale to the internal 15 bit per pixel representation
	if (fixed_seed)
		Random().srand(45721);
	else
		Random().srand((unsigned)time(nullptr));
	for (unsigned i = 0; i < DITHER_BUFFER_SIZE; i++)
		maiDitheringBuffer[i] = abs(Random().rand()) % (2 * strength + 1) - strength;
}

Surface Dithering::process(/* const */ Surface &src_plane, unsigned block_size) {
	const unsigned width = src_plane.width();
	const unsigned height = src_plane.height();
	auto src_view = src_plane.view_as<int16_t>();
	auto dst_plane = Surface::build_from<int16_t>();
	dst_plane.reserve(src_plane.width(), src_plane.height());
	// apply the dithering on a block basis
	for (unsigned y = 0; y < height; y += block_size) {
		for (unsigned x = 0; x < width; x += block_size) {
			// initialize to a random position in DitheringBuffer
			const int32_t *dither_buffer = &(maiDitheringBuffer[rand() % (DITHER_BUFFER_SIZE - block_size * block_size)]);
			for (unsigned h = 0; h < block_size; h++) {
				for (unsigned k = 0; k < block_size; k++) {
					*dst_plane.data(x + k, y + h) = src_view.read(x + k, y + h) + *dither_buffer++;
				}
			}
		}
	}
	return dst_plane.finish();
}

} // namespace lctm
