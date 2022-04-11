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
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//
// PriorityConfiguration.hpp
//

#pragma once

#include <unordered_map>

#include "LayerEncodeFlags.hpp"

namespace lctm {

enum PriorityMode { PM_ALL_OFF = 0, PM_ALL_ON = 1, PM_SL1_ON = 2, PM_SL2_ON = 3, PM_SL1_OFF = 4, PM_SL2_OFF = 5};
enum PriorityMBType { Smooth = 0, Plain = 1, Coarse = 2, Edge =  3, SmoothAndPlain = 4, CoarseAndPlain = 5};

class PriorityConfiguration {
public:
	using value_type = std::pair<EncodingMode, PriorityMode>;
	using value_type_mb = PriorityMBType;

	PriorityConfiguration(const std::string &priority_mode, const std::string &priority_type_mb_sl_1, const std::string &priority_type_mb_sl_2);

	~PriorityConfiguration() = default;

	EncodingMode encoding_mode() const;

	bool use_priority_map_sl_1() const;
	bool use_priority_map_sl_2() const;
	PriorityMBType priority_map_type_sl_1() const;
	PriorityMBType priority_map_type_sl_2() const;

private:
	using key_type = std::string;

	// Container of available configuration modes
	// Each value is a pair [encoding mode, use priority map flag]
	static const std::unordered_map<key_type, value_type> modes_;

	// Container of available configuration modes
	// Each value is PriorityMBType
	static const std::unordered_map<key_type, value_type_mb> block_type_;

	EncodingMode encoding_mode_;

	bool use_pmap_sl_1_;
	bool use_pmap_sl_2_;
	PriorityMBType use_pmap_type_sl_1_;
	PriorityMBType use_pmap_type_sl_2_;
};

} // namespace lctm
