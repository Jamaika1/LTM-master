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
// HuffmanEncoder.cpp
//

#include "HuffmanEncoder.hpp"

#include "Diagnostics.hpp"
#include "HuffmanDecoder.hpp"

#include <algorithm>
#include <cassert>
#include <queue>

namespace lctm {

// Add a symbol to pending code tree
//
void HuffmanEncoderBuilder::add_symbol(unsigned symbol, unsigned count) {
	assert(symbol < HuffmanEncoder::MAX_SYMBOL);
	symbol_counts_[symbol] += count;
}

// Node of huffman coding
struct Node {
	Node() {}
	Node(unsigned s, unsigned c) : symbol(s), count(c) {}
	Node(Node *l, Node *r, unsigned node_number)
	    : symbol(HuffmanEncoder::MAX_SYMBOL + node_number), count(l->count + r->count), left(l), right(r) {}

	unsigned symbol = HuffmanEncoder::MAX_SYMBOL;
	unsigned count = 0;
	unsigned bits = 0;
	Node *left = nullptr;
	Node *right = nullptr;
};

HuffmanEncoder HuffmanEncoderBuilder::finish() {
	std::vector<std::unique_ptr<Node>> nodes;
	int num_symbols;

	// Build vector of uncoded nodes, sorted by frequency
	auto node_compare = [](const Node *l, const Node *r) {
		if (l->count == r->count) {
			// Equal counts - order t by symbol or node number - leaves are lower than inter nodes
			return l->symbol < r->symbol;
		} else
			return l->count > r->count;
	};
	std::priority_queue<Node *, std::vector<Node *>, decltype(node_compare)> heap(node_compare);

	for (unsigned s = 0; s < HuffmanEncoder::MAX_SYMBOL; ++s) {
		if (symbol_counts_[s]) {
			nodes.emplace_back(new Node(s, symbol_counts_[s]));
			heap.emplace(nodes.back().get());
		}
	}

	if (heap.empty())
		// No symbols at all
		return HuffmanEncoder();

	num_symbols = static_cast<int>(nodes.size());

	//// Build huffman tree on top of symbol leaves
	//

	// While there is more than 1 pending node:
	//   take 2 smallest by frequency and combine them to make a new internal node
	while (heap.size() > 1) {
		Node *l = heap.top();
		heap.pop();
		Node *r = heap.top();
		heap.pop();
		nodes.emplace_back(new Node(l, r, (unsigned int)nodes.size()));
		heap.emplace(nodes.back().get());
	}

	// Starting with the root (last entry in nodes) - walk backwards filling in code lengths
	assert(heap.top() == nodes.back().get());
	for (auto n = nodes.rbegin(); n != nodes.rend(); ++n) {
		if ((*n)->symbol >= HuffmanEncoder::MAX_SYMBOL) {
			(*n)->left->bits = (*n)->bits + 1;
			(*n)->right->bits = (*n)->bits + 1;
		}
	}

	// The symbol nodes will now have their final code lengths.
	// Sort leaf nodes by (decending coded length, ascending symbol)
	std::sort(nodes.begin(), nodes.begin() + num_symbols, [](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
		if (a->bits == b->bits)
			return a->symbol > b->symbol;
		else
			return a->bits < b->bits;
	});

	// Generate code table for encoder
	//
	std::vector<HuffmanEncoder::HuffmanCode> codes;
	for (auto n = nodes.begin(); n != nodes.begin() + num_symbols; ++n)
		codes.emplace_back((*n)->symbol, (*n)->bits);

	// Assign values to codes
	unsigned current_length = codes.back().bits;
	unsigned current_value = 0;

	for (auto c = codes.rbegin(); c != codes.rend(); ++c) {
		if (c->bits < current_length) {
			current_value >>= (current_length - c->bits);
			current_length = c->bits;
		}
		c->value = current_value++;
	}

	return HuffmanEncoder(codes);
}

// Resolve symbol codes and write codes+lengths to bitstream
void HuffmanEncoder::write_codes(BitstreamPacker &b) {
	BitstreamPacker::ScopedContextLabel label(b, "entropy_code");

	if (codes_.empty()) {
		// No symbols
		b.u(5, 31, "min_code_length");
		b.u(5, 31, "max_code_length");
		return;
	}

	if (codes_.size() == 1) {
		// Single symbol
		b.u(5, 0, "min_code_length");
		b.u(5, 0, "max_code_length");
		b.u(8, codes_.front().symbol, "single_symbol");
		return;
	}

	const unsigned min_code_length = codes_.front().bits;
	const unsigned max_code_length = codes_.back().bits;
	unsigned length_bits = HuffmanDecoder::bit_width(max_code_length - min_code_length);

	b.u(5, min_code_length, "min_code_length");
	b.u(5, max_code_length, "max_code_length");

	if (codes_.size() > 31) {
		b.u(1, 1, "presence_bitmap");

		// More than 31 coded symbols - write a 'presence' bitmap
		//
		unsigned lengths[MAX_SYMBOL] = {0};
		for (const auto &c : codes_)
			lengths[c.symbol] = c.bits;

		for (unsigned s = 0; s < MAX_SYMBOL; ++s) {
			if (lengths[s]) {
				b.u(1, 1, "presence");
				b.u(length_bits, lengths[s] - min_code_length, "length");
			} else {
				b.u(1, 0, "presence");
			}
		}
	} else {
		b.u(1, 0, "presence_bitmap");

		// 31 or less coded symbols - Write symbol/length pairs
		b.u(5, (uint32_t)codes_.size(), "count");
		for (const auto &c : codes_) {
			b.u(8, c.symbol, "symbol");
			b.u(length_bits, c.bits - min_code_length, "length");
		}
	}
}

// Write a coded symbol to the bitstream
void HuffmanEncoder::write_symbol(BitstreamPacker &b, unsigned symbol) {
	BitstreamPacker::ScopedContextLabel label(b, "entropy_symb");
	// Find symbols in codes
	for (auto &c : codes_) {
		if (c.symbol == symbol) {
			b.u(c.bits, c.value, "codebits");
			return;
		}
	}

	FATAL("Uncoded symbol");
}

} // namespace lctm
