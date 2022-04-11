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
// EntropyDecoder.cpp
//

#include "EntropyDecoder.hpp"

#include "BitstreamUnpacker.hpp"
#include "HuffmanDecoder.hpp"
#include "Packet.hpp"
#include "TemporalDecode.hpp"

#include "Diagnostics.hpp"

namespace lctm {

//// SymbolSource
//
// Abstract interface for reading RLE symbols for surface.
//
class SymbolSource {
public:
	virtual ~SymbolSource(){};
	virtual void start() = 0;
	virtual unsigned get(unsigned state) = 0;
	virtual unsigned get_byte() = 0;
};

// SymbolSourceConstant
//
// Always return a constant
//
class SymbolSourceConstant : public SymbolSource {
public:
	SymbolSourceConstant(unsigned c) : c_(c) {}

	void start() override {}

	unsigned get(unsigned state) override { return c_; }

	unsigned get_byte() override { return c_; }

private:
	const unsigned c_;
};

// SymbolSourceRaw
//
// Treats source data as raw symbols with no coding
//
class SymbolSourceRaw : public SymbolSource {
public:
	SymbolSourceRaw(BitstreamUnpacker &b) : b_(b) {}

	void start() override {}

	unsigned get(unsigned state) override { return b_.byte(); }

	unsigned get_byte() override { return b_.byte(); }

private:
	BitstreamUnpacker &b_;
};

// SymbolSourceHuffman
//
// Source is huffman coded data, using different trees for each part of the RLE coding.
//
class SymbolSourceHuffman : public SymbolSource {
public:
	SymbolSourceHuffman(unsigned num_states, BitstreamUnpacker &b) : states_(num_states), b_(b) {}

	void start() override {
		for (unsigned i = 0; i < states_.size(); ++i) {
			states_[i].read_codes(b_);
		}
	}

	unsigned get(unsigned state) override {
		assert(state < states_.size());
		return states_[state].decode_symbol(b_);
	}

	unsigned get_byte() override { return b_.u(8, "first_symbol"); }

private:
	std::vector<HuffmanDecoder> states_;
	BitstreamUnpacker &b_;
};

// Create symbol source for given encoded data
//
static std::unique_ptr<SymbolSource> create_symbol_source(unsigned num_states, bool entropy_enabled, bool rle_only,
                                                          BitstreamUnpacker &b, unsigned constant) {
	if (entropy_enabled) {
		if (rle_only)
			return std::unique_ptr<SymbolSource>(new SymbolSourceRaw(b));
		else
			return std::unique_ptr<SymbolSource>(new SymbolSourceHuffman(num_states, b));
	} else {
		return std::unique_ptr<SymbolSource>(new SymbolSourceConstant(constant));
	}
}

//// EntropyDecoderResiduals
//
// Run Length coded residuals
//
EntropyDecoderResidualsBase::rle_pel_t EntropyDecoderResidualsBase::decode_pel(SymbolSource &source) const {
	rle_pel_t r = {0, 0};

	unsigned symbol = source.get(STATE_LSB);

	// If bit 0 is set, an MS byte follows
	if (symbol & 0x01) {
		unsigned lsb_symbol = symbol;
		symbol = source.get(STATE_MSB);
		r.pel = ((((symbol & 0x7f) << 8) | (lsb_symbol & 0xfe)) >> 1) - 0x2000;
	} else {
		r.pel = ((symbol & 0x7e) - 0x40) >> 1;
	}

	// if the bit 7 is set, zero run count follows
	if (symbol & 0x80) {
		// LSByte first
		/***
		unsigned bit_offset = 0;
		do {
			symbol = source.get(STATE_ZERO);
			r.zero_runlength |= (symbol & 0x7f) << bit_offset;
			bit_offset += 7;
		} while (symbol & 0x80);
		***/
		// MSByte first
		/***/
		r.zero_runlength = 0;
		do {
			symbol = source.get(STATE_ZERO);
			r.zero_runlength = (r.zero_runlength << 7) | (symbol & 0x7f);
		} while (symbol & 0x80);
		/***/
	}

	return r;
}

// Decoding full frame raster order
Surface EntropyDecoderResiduals::process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only,
                                         BitstreamUnpacker &b) {
	// Set up source of symbols - empty layers are have a constant value of 0x40
	const auto symbol_source(create_symbol_source(STATE_COUNT, entropy_enabled, rle_only, b, 0x40));

	// Make the new surface
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(width, height);

	// Current PEL/run value
	rle_pel_t current = {0, 0};

	// Read any huffman tables
	symbol_source->start();

	for (unsigned y = 0; y < height; ++y) {
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < width; ++x) {
			if (current.zero_runlength > 0) {
				// Extend the run of zeros
				// dest.write(x, y, 0);
				*pdst++ = 0;
				--current.zero_runlength;
			} else {
				// Write PEL value
				current = decode_pel(*symbol_source);
				// dest.write(x, y, current.pel);
				*pdst++ = current.pel;
			}
		}
	}

	return dest.finish();
}

// Decoding in coding unit order
Surface EntropyDecoderResidualsTiled::process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only,
                                              BitstreamUnpacker &b, unsigned transform_block_size) {
	// Set up source of symbols - empty layers are have a constant value of 0x40
	const auto symbol_source(create_symbol_source(STATE_COUNT, entropy_enabled, rle_only, b, 0x40));

	// Make the new surface
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(width, height);

	// Divisor for block->tiles
	const unsigned d = 32 / transform_block_size;

	// Current PEL/run value
	rle_pel_t current = {0, 0};

	// Read any huffman tables
	symbol_source->start();

	for (unsigned ty = 0; ty < height; ty += d) {
		for (unsigned tx = 0; tx < width; tx += d) {

			// For each transform in tile
			// for (unsigned y = ty; y < std::min(ty + d, height); ++y) {
			for (unsigned y = ty; y < LCEVC_MIN(ty + d, height); ++y) {
				//				int16_t *__restrict pdst = dest.data(tx, y);
				// for (unsigned x = tx; x < std::min(tx + d, width); ++x) {
				for (unsigned x = tx; x < LCEVC_MIN(tx + d, width); ++x) {
					if (current.zero_runlength > 0) {
						// Extend the run of zeros
						dest.write(x, y, 0);
						//						*pdst++ = 0;
						--current.zero_runlength;
					} else {
						// Write PEL value
						current = decode_pel(*symbol_source);
						dest.write(x, y, current.pel);
						//						*pdst++ = current.pel;
					}
				}
			}
		}
	}

	return dest.finish();
}

//// EntropyDecoderTemporal
//
// Run Length coded temporal bits
//
unsigned EntropyDecoderTemporal::decode_run(SymbolSource &source, bool symbol) const {
	unsigned count = 0;
	unsigned c = 0;
	do {
		c = source.get(symbol ? STATE_ONE_RUN : STATE_ZERO_RUN);
		count = (count << 7) | (c & 0x7f);
	} while (c & 0x80);

	return count;
}

Surface EntropyDecoderTemporal::process(unsigned width, unsigned height, bool entropy_enabled, bool rle_only, BitstreamUnpacker &b,
                                        unsigned transform_block_size, bool use_reduced_signalling) {
	const auto symbol_source(create_symbol_source(STATE_COUNT, entropy_enabled, rle_only, b, 0));

	// Make the new surface
	auto dest = Surface::build_from<uint8_t>();
	dest.reserve(width, height);

	if (!entropy_enabled)
		return dest.fill(TemporalType::TEMPORAL_PRED, width, height).finish();

	// Divisor for block->tiles
	const unsigned d = 32 / transform_block_size;

	// Read any huffman tables
	symbol_source->start();

	// Get the first symbol & count
	bool symbol = (symbol_source->get_byte() != 0);

	unsigned count = decode_run(*symbol_source, symbol);

	// For each tile ..
	for (unsigned ty = 0; ty < height; ty += d) {
		for (unsigned tx = 0; tx < width; tx += d) {

			bool intra_tile = false;
			// For each transform in tile
			// for (unsigned y = ty; y < std::min(ty + d, height); ++y) {
			for (unsigned y = ty; y < LCEVC_MIN(ty + d, height); ++y) {
				uint8_t *__restrict pdst = dest.data(tx, y);
				// for (unsigned x = tx; x < std::min(tx + d, width); ++x) {
				for (unsigned x = tx; x < LCEVC_MIN(tx + d, width); ++x) {
					if (use_reduced_signalling && intra_tile) {
						// The whole tile was flagged as intra
						*pdst++ = TemporalType::TEMPORAL_INTR;
					} else {
						// Get next symbol
						while (count == 0) {
							// Flip symbol and get next count
							symbol = !symbol;
							count = decode_run(*symbol_source, symbol);
						}

						// Check for intra tile
						if (use_reduced_signalling && symbol && tx == x && ty == y)
							intra_tile = true;

						// Write to dst
						// dest.write(x, y, symbol ? TemporalType::TEMPORAL_INTR : TemporalType::TEMPORAL_PRED);
						*pdst++ = (symbol ? TemporalType::TEMPORAL_INTR : TemporalType::TEMPORAL_PRED);

						count--;
					}
				}
			}
		}
	}

	return dest.finish();
}

//// EntropyDecoderFlags
//
// Run Length coded temporal bits
//
unsigned EntropyDecoderFlags::decode_run(SymbolSource &source, bool symbol) const {
	unsigned count = 0;
	unsigned c = 0;
	do {
		c = source.get(symbol ? STATE_ONE_RUN : STATE_ZERO_RUN);
		count = (count << 7) | (c & 0x7f);
	} while (c & 0x80);

	return count;
}

Surface EntropyDecoderFlags::process(unsigned width, unsigned height, BitstreamUnpacker &b) {
	const auto symbol_source(create_symbol_source(STATE_COUNT, true, true, b, 0));

	// Make the new surface
	auto dest = Surface::build_from<uint8_t>();
	dest.reserve(width, height);

	// Read any huffman tables
	symbol_source->start();

	// Get the first symbol & count
	bool symbol = (symbol_source->get_byte() != 0);

	unsigned count = decode_run(*symbol_source, symbol);

	// For each tile ..
	for (unsigned y = 0; y < height; ++y) {
		uint8_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < width; ++x) {
			bool intra_tile = false;
			// Get next symbol
			while (count == 0) {
				// Flip symbol and get next count
				symbol = !symbol;
				count = decode_run(*symbol_source, symbol);
			}
			// Write to dst
			// dest.write(x, y, symbol ? 0xff : 0x00);
			*pdst++ = (symbol ? 0xff : 0x00);
			count--;
		}
	}

	return dest.finish();
}

//// EntropyDecoderSizes
//
// Coded sizes
//
uint16_t EntropyDecoderSizes::decode_size(SymbolSource &source) const {

	const unsigned l = source.get(STATE_LSB);

	// If bit 0 is set, an MS byte follows
	if (l & 0x01) {
		const unsigned m = source.get(STATE_MSB);
		return (l >> 1) + (m << 7);
	} else {
		return l >> 1;
	}
}

int16_t EntropyDecoderSizes::decode_size_delta(SymbolSource &source) const {

	const unsigned l = source.get(STATE_LSB);

	// If bit 0 is set, an MS byte follows
	//
	// Sign extend from high bit of decoded value to recover signed delta
	//
	if (l & 0x01) {
		const unsigned m = source.get(STATE_MSB);
		const int16_t r = (l >> 1) + (m << 7);
		return r | ((r & 0x4000) << 1);
	} else {
		const uint8_t r = l >> 1;
		return (int8_t)(r | ((r & 0x40) << 1));
	}
}

Surface EntropyDecoderSizes::process(unsigned width, unsigned height, BitstreamUnpacker &b, const std::vector<bool> entropy_enabled,
                                     const unsigned tile_idx, CompressionType compression_type) {
	// Set up source of symbols
	const auto symbol_source(create_symbol_source(STATE_COUNT, true, false, b, 0));

	// Make the new surface
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(width, height);

	// Read any huffman tables
	symbol_source->start();

	if (compression_type == CompressionType_Prefix) {
		for (unsigned y = 0; y < height; ++y) {
			uint16_t *__restrict pdst = dest.data(0, y);
			for (unsigned x = 0; x < width; ++x) {
				if (entropy_enabled[tile_idx + x])
					*pdst++ = decode_size(*symbol_source);
				else
					*pdst++ = 0;
			}
		}
	} else if (compression_type == CompressionType_Prefix_OnDiff) {
		uint16_t p = 0;
		for (unsigned y = 0; y < height; ++y) {
			uint16_t *__restrict pdst = dest.data(0, y);
			for (unsigned x = 0; x < width; ++x) {
				if (entropy_enabled[tile_idx + x]) {
					const int diff = decode_size_delta(*symbol_source);
					p += diff;
					*pdst++ = p;
				} else {
					*pdst++ = 0;
				}
			}
		}
	}

	return dest.finish();
}

} // namespace lctm
