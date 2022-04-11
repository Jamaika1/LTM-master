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
// EntropyEncoder.cpp
//

#include "EntropyEncoder.hpp"

#include "BitstreamPacker.hpp"
#include "HuffmanEncoder.hpp"
#include "Packet.hpp"
#include "TemporalDecode.hpp"

#include "Diagnostics.hpp"

#include <algorithm>

namespace lctm {

//// Residuals
//
class EntropyModelResiduals {
	// State of the residual RLE modelling - used to select between 3 huffman trees
	enum State { STATE_LSB, STATE_MSB, STATE_ZERO, STATE_COUNT };

public:
	EntropyModelResiduals() {}

	void encode_high_count(State state, unsigned symbol);
	void encode_run(int16_t residual, unsigned zeros);
	EncodedChunk finish();

private:
	void encode_symbol(State state, unsigned symbol);

	HuffmanEncoderBuilder huffman_state_[STATE_COUNT];

	struct RleSymbol {
		RleSymbol() = default;
		RleSymbol(State st, uint8_t sy) : state(st), symbol(sy) {}
		State state = STATE_LSB;
		uint8_t symbol = 0;
	};

	std::vector<RleSymbol> rle_symbols_;
};

void EntropyModelResiduals::encode_symbol(State state, unsigned symbol) {
	rle_symbols_.emplace_back(state, symbol);
	huffman_state_[state].add_symbol(symbol, 1);
}

void EntropyModelResiduals::encode_high_count(State s, unsigned c) {
	if (c > 0x7f)
		encode_high_count(s, c >> 7);
	encode_symbol(s, (c & 0x7f) | 0x80);
}

void EntropyModelResiduals::encode_run(int16_t residual, unsigned zeros) {
	// Is there a run?
	const uint16_t zeros_bit = (zeros > 0) ? 0x80 : 0;

	// Encode value
	if (residual >= -32 && residual < 32) {
		encode_symbol(STATE_LSB, (residual * 2 + 0x40) | zeros_bit);
	} else {
		const uint16_t clamped_residual = std::min(std::max(residual + 0x2000, 0), 0x3fff) << 1;
		encode_symbol(STATE_LSB, (clamped_residual & 0xfe) | 0x01);
		encode_symbol(STATE_MSB, ((clamped_residual >> 8) & 0x7f) | zeros_bit);
	}

	// Add any required run symbols
	// LSByte first
	/***
	while (zeros) {
		const uint8_t low_bits = zeros & 0x7f;
		zeros >>= 7;

		if (zeros)
			encode_symbol(STATE_ZERO, low_bits | 0x80);
		else
			encode_symbol(STATE_ZERO, low_bits);
	}
	***/
	// MSByte first
	/***/
	if (zeros_bit) {
		if (zeros > 0x7f)
			encode_high_count(STATE_ZERO, zeros >> 7);
		encode_symbol(STATE_ZERO, zeros & 0x7f);
	}
	/***/
}

EncodedChunk EntropyModelResiduals::finish() {
	// Generate huffman encoders
	std::vector<HuffmanEncoder> huffman_encoders;

	BitstreamPacker raw_packer;
	BitstreamPacker huffman_packer;

	// Finialise huffman encoders state
	for (unsigned h = 0; h < STATE_COUNT; ++h) {
		huffman_encoders.emplace_back(huffman_state_[h].finish());
		huffman_encoders.back().write_codes(huffman_packer);
	}

	// Code symbols both ways: raw vs. prefix
	for (const auto s : rle_symbols_) {
		raw_packer.u(8, s.symbol);
		huffman_encoders[s.state].write_symbol(huffman_packer, s.symbol);
	}

	return EncodedChunk(raw_packer.finish(), huffman_packer.finish());
}

EncodedChunk EntropyEncoderResiduals::process(const Surface &surface) {
	auto view = surface.view_as<int16_t>();
	const unsigned width = view.width();
	const unsigned height = view.height();

	EntropyModelResiduals models;

	int16_t residual = 0;
	unsigned zeros = 0;

	bool has_entropy = false;

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {

			int16_t r = view.read(x, y);

			// Record if there is any data in surface
			if (r)
				has_entropy = true;

			// Special case - always have to encode first symbol, even if it is zero
			if (x == 0 && y == 0) {
				residual = r;
				zeros = 0;
				continue;
			}

			if (r == 0) {
				// Extend current run
				zeros++;
			} else {
				// Emit previous run and record this residual
				models.encode_run(residual, zeros);
				residual = r;
				zeros = 0;
			}
		}
	}

	// Emit last run
	models.encode_run(residual, zeros);

	if (has_entropy)
		return models.finish();
	else
		return EncodedChunk();
}

EncodedChunk EntropyEncoderResidualsTiled::process(const Surface &surface, unsigned transform_block_size) {
	auto view = surface.view_as<int16_t>();
	const unsigned width = view.width();
	const unsigned height = view.height();

	// Divisor for block->tiles
	const unsigned d = 32 / transform_block_size;

	EntropyModelResiduals models;

	int16_t residual = 0;
	unsigned zeros = 0;

	bool has_entropy = false;

	for (unsigned ty = 0; ty < height; ty += d) {
		for (unsigned tx = 0; tx < width; tx += d) {

			// For each transform in tile ...
			for (unsigned y = ty; y < std::min(ty + d, height); ++y) {
				for (unsigned x = tx; x < std::min(tx + d, width); ++x) {
					int16_t r = view.read(x, y);

					// Record if there is any data in surface
					if (r)
						has_entropy = true;

					// Special case - always have to encode first symbol, even if it is zero
					if (x == 0 && y == 0) {
						residual = r;
						zeros = 0;
						continue;
					}

					if (r == 0) {
						// Extend current run
						zeros++;
					} else {
						// Emit previous run and record this residual
						models.encode_run(residual, zeros);
						residual = r;
						zeros = 0;
					}
				}
			}
		}
	}

	// Emit last run
	models.encode_run(residual, zeros);

	if (has_entropy)
		return models.finish();
	else
		return EncodedChunk();
}

//// Temporal
//

//// EntropyModelRleFlag
//
// Encode a mask of 0,1 runs using two models
//
class EntropyModelRleFlag {
	// State of the RLE parser - used to select between 2 huffman trees (0.., 1..)
	enum State { STATE_ZERO_RUN, STATE_ONE_RUN, STATE_COUNT };

public:
	EntropyModelRleFlag() {}

	void encode_high_count(State s, unsigned c);
	void encode_run(bool value, unsigned count);
	EncodedChunk finish(bool start_value);

private:
	void encode_symbol(State state, unsigned symbol);

	HuffmanEncoderBuilder huffman_state_[STATE_COUNT];

	struct RleSymbol {
		RleSymbol() = default;
		RleSymbol(State st, uint8_t sy) : state(st), symbol(sy) {}
		State state = STATE_ZERO_RUN;
		uint8_t symbol = 0;
	};

	std::vector<RleSymbol> rle_symbols_;
};

void EntropyModelRleFlag::encode_symbol(State state, unsigned symbol) {
	rle_symbols_.emplace_back(state, symbol);
	huffman_state_[state].add_symbol(symbol, 1);
}

void EntropyModelRleFlag::encode_high_count(State s, unsigned c) {
	if (c > 0x7f)
		encode_high_count(s, c >> 7);
	encode_symbol(s, (c & 0x7f) | 0x80);
}

void EntropyModelRleFlag::encode_run(bool value, unsigned count) {

	const State s = value ? STATE_ONE_RUN : STATE_ZERO_RUN;

	if (count > 0x7f)
		encode_high_count(s, count >> 7);
	encode_symbol(s, count & 0x7f);
}

EncodedChunk EntropyModelRleFlag::finish(bool start_value) {

	// Generate huffman encoders
	std::vector<HuffmanEncoder> huffman_encoders;

	BitstreamPacker raw_packer;
	BitstreamPacker huffman_packer;

	// Finialise huffman encoders state
	for (unsigned h = 0; h < STATE_COUNT; ++h) {
		huffman_encoders.emplace_back(huffman_state_[h].finish());
		huffman_encoders.back().write_codes(huffman_packer);
	}

	// Add starting value to each bitstream
	raw_packer.u(8, start_value ? 1 : 0);
	huffman_packer.u(8, start_value ? 1 : 0);

	// Code symbols both ways: raw vs. prefix
	for (const auto s : rle_symbols_) {
		raw_packer.u(8, s.symbol);
		huffman_encoders[s.state].write_symbol(huffman_packer, s.symbol);
	}

	return EncodedChunk(raw_packer.finish(), huffman_packer.finish());
}

EncodedChunk EntropyEncoderTemporal::process(const Surface &surface, unsigned transform_block_size, bool use_reduced_signalling) {
	auto view = surface.view_as<uint8_t>();
	const unsigned width = view.width();
	const unsigned height = view.height();

	EntropyModelRleFlag models;

	// Divisor for block->tiles
	const unsigned d = 32 / transform_block_size;

	// RLE state
	bool start = 0;
	bool value = 0;
	unsigned count = 0;

	bool has_entropy = false;

	// For each tile (including any part tiles) ...
	for (unsigned ty = 0; ty < height; ty += d) {
		for (unsigned tx = 0; tx < width; tx += d) {

			bool intra_tile = false;

			// For each transform in tile ...
			for (unsigned y = ty; y < std::min(ty + d, height); ++y) {
				for (unsigned x = tx; x < std::min(tx + d, width); ++x) {
					const bool first_transform_in_tile = (tx == x) && (ty == y);
					const bool v = (view.read(x, y) == TemporalType::TEMPORAL_INTR) ? 1 : 0;

					// Record if there is any data in surface
					if (v)
						has_entropy = true;

					// Record intra flag for whole tile
					if (use_reduced_signalling && first_transform_in_tile && v != 0)
						intra_tile = true;

					// Reduced signalling - don't encode rest of tile if first is intra
					if (!first_transform_in_tile && use_reduced_signalling && intra_tile)
						continue;

					if (x == 0 && y == 0) {
						// Special case - store first symbol
						start = value = v;
						count = 1;
					} else if (v == value) {
						// Extend run
						++count;
					} else {
						// Encode run
						models.encode_run(value, count);
						value = v;
						count = 1;
					}
				}
			}
		}
	}

	// Emit last run
	models.encode_run(value, count);

	if (has_entropy)
		return models.finish(start);
	else
		return EncodedChunk();
}

EncodedChunk EntropyEncoderFlags::process(const Surface &surface) {
	auto view = surface.view_as<uint8_t>();
	const unsigned width = view.width();
	const unsigned height = view.height();

	EntropyModelRleFlag models;

	// RLE state
	bool start = 0;
	bool value = 0;
	unsigned count = 0;

	// For each flag ...
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {

			const bool v = (view.read(x, y) != 0) ? true : false;

			if (x == 0 && y == 0) {
				// Special case - store first symbol
				start = value = v;
				count = 1;
			} else if (v == value) {
				// Extend run
				++count;
			} else {
				// Encode run
				models.encode_run(value, count);
				value = v;
				count = 1;
			}
		}
	}

	// Emit last run
	models.encode_run(value, count);

	return models.finish(start);
}

////EntropyEncoderSizes
//
// Used for tile sizes
//
class EntropyModelSizes {
	// State of the size modelling - used to select between 2 huffman trees
	enum State { STATE_LSB, STATE_MSB, STATE_COUNT };

public:
	EntropyModelSizes() {}

	void encode_size(uint16_t value);
	void encode_size_delta(int32_t value);
	EncodedChunk finish();

private:
	void encode_symbol(State state, unsigned symbol);

	HuffmanEncoderBuilder huffman_state_[STATE_COUNT];

	struct RleSymbol {
		RleSymbol() = default;
		RleSymbol(State st, uint8_t sy) : state(st), symbol(sy) {}
		State state = STATE_LSB;
		uint8_t symbol = 0;
	};

	std::vector<RleSymbol> rle_symbols_;
};

void EntropyModelSizes::encode_symbol(State state, unsigned symbol) {
	rle_symbols_.emplace_back(state, symbol);
	huffman_state_[state].add_symbol(symbol, 1);
}

void EntropyModelSizes::encode_size(uint16_t value) {
	CHECK(value <= 32767);

	if (value < 0x80) {
		encode_symbol(STATE_LSB, (value * 2) | 0x00);
	} else {
		encode_symbol(STATE_LSB, ((value & 0x7f) * 2) | 0x01);
		encode_symbol(STATE_MSB, value >> 7);
	}
}

void EntropyModelSizes::encode_size_delta(int32_t value) {
	CHECK(value >= -16384 && value <= 16383);

	// Decoder will sign extend
	if (value >= -64 && value <= 63) {
		encode_symbol(STATE_LSB, ((value * 2) | 0x00) & 0xff);
	} else {
		encode_symbol(STATE_LSB, ((value & 0x7f) * 2) | 0x01);
		encode_symbol(STATE_MSB, (value >> 7) & 0xff);
	}
}

EncodedChunk EntropyModelSizes::finish() {
	// Generate huffman encoders
	std::vector<HuffmanEncoder> huffman_encoders;

	BitstreamPacker raw_packer;
	BitstreamPacker huffman_packer;

	// Finialise huffman encoders state
	for (unsigned h = 0; h < STATE_COUNT; ++h) {
		huffman_encoders.emplace_back(huffman_state_[h].finish());
		huffman_encoders.back().write_codes(huffman_packer);
	}

	// Code symbols both ways: raw vs. prefix
	for (const auto s : rle_symbols_) {
		raw_packer.u(8, s.symbol);
		huffman_encoders[s.state].write_symbol(huffman_packer, s.symbol);
	}

	return EncodedChunk(raw_packer.finish(), huffman_packer.finish());
}

EncodedChunk EntropyEncoderSizes::process(const Surface &surface, const std::vector<bool> entropy_enabled, const unsigned tile_idx,
                                          CompressionType compression_type) {
	auto view = surface.view_as<uint16_t>();
	const unsigned width = view.width();
	const unsigned height = view.height();

	EntropyModelSizes models;

	if (compression_type == CompressionType_Prefix) {
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				if (entropy_enabled[tile_idx + x]) {
					const uint16_t v = view.read(x, y);
					CHECK(v != 0);
					models.encode_size(v);
				}
			}
		}
	} else {
		uint16_t p = 0;
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				if (entropy_enabled[tile_idx + x]) {
					const uint16_t v = view.read(x, y);
					CHECK(v != 0);
					models.encode_size_delta(v - p);
					p = v;
				}
			}
		}
	}

	return models.finish();
}

} // namespace lctm
