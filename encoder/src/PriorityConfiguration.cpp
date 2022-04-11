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
//               Lorenzo Ciccarelli(lorenzo.ciccarelli@v-nova.com)
//
// PriorityConfiguration.cpp
//
// Stores information about available modes for usage of priority map
//

#include <cassert>

#include "Diagnostics.hpp"
#include "PriorityConfiguration.hpp"

namespace lctm {

// clang-format off
const std::unordered_map<std::string, std::pair<EncodingMode, PriorityMode>> PriorityConfiguration::modes_ = {
    {"mode_0_0", {EncodingMode::ENCODE_ALL,  PriorityMode::PM_ALL_OFF}}, // SL-2 OFF - SL-1 OFF
    {"mode_1_0", {EncodingMode::ENCODE_ALL,  PriorityMode::PM_SL1_ON }}, // SL-2 OFF - SL-1 ON (ALL)
    {"mode_1_1", {EncodingMode::Ax,          PriorityMode::PM_SL1_ON }}, // SL-2 OFF - SL-1 ON (ONLY ALL AVERAGE)
    {"mode_1_2", {EncodingMode::AA,          PriorityMode::PM_SL1_ON }}, // SL-2 OFF - SL-1 ON (ONLY AA)
    {"mode_1_3", {EncodingMode::NA,          PriorityMode::PM_SL1_ON }}, // SL-2 OFF - SL-1 ON (AD, AV, AH)

    {"mode_2_0", {EncodingMode::ENCODE_ALL,  PriorityMode::PM_SL2_ON }}, // SL-2 ON  - SL-1 OFF

    {"mode_3_0", {EncodingMode::ENCODE_ALL,  PriorityMode::PM_ALL_ON }}, // SL-2 ON  - SL-1 ON (ALL)
    {"mode_3_1", {EncodingMode::Ax,          PriorityMode::PM_ALL_ON }}, // SL-2 ON  - SL-1 ON (ONLY ALL AVERAGE)
    {"mode_3_2", {EncodingMode::AA,          PriorityMode::PM_ALL_ON }}, // SL-2 ON  - SL-1 ON (ONLY AA)
    {"mode_3_3", {EncodingMode::NA,          PriorityMode::PM_ALL_ON }}, // SL-2 ON  - SL-1 ON (AD, AV, AH)
};

const std::unordered_map<std::string,  PriorityMBType> PriorityConfiguration::block_type_ = {
    {"type_0", PriorityMBType::Smooth        }, // Select only smooth coarse 8x8 blocks
    {"type_1", PriorityMBType::Plain         }, // Select only plain 8x8 blocks
    {"type_2", PriorityMBType::Edge          }, // Select only edge 8x8 blocks
    {"type_3", PriorityMBType::Coarse        }, // Select only coarse 8x8 blocks
    {"type_4", PriorityMBType::SmoothAndPlain}, // Select plain and smooth coarse 8x8 blocks
    {"type_5", PriorityMBType::CoarseAndPlain}, // Select coarse and plain  8x8 blocks
};
// clang-format on

PriorityConfiguration::PriorityConfiguration(const std::string &priority_mode, const std::string &priority_type_mb_sl_1, const std::string &priority_type_mb_sl_2) {
	try {
		const auto &mode_config = modes_.at(priority_mode);
		encoding_mode_ = mode_config.first;
		use_pmap_sl_1_ = (mode_config.second == PriorityMode::PM_SL1_ON) || (mode_config.second == PriorityMode::PM_ALL_ON);
		use_pmap_sl_2_ = (mode_config.second == PriorityMode::PM_SL2_ON) || (mode_config.second == PriorityMode::PM_ALL_ON);

		REPORT("PriorityConfiguration: MODE[%s] (%d) PM SL-1[%d] PM SL-2[%d]", priority_mode.c_str(), encoding_mode_, use_pmap_sl_1_, use_pmap_sl_2_);

	} catch (const std::out_of_range &e) {
		ERR("PriorityConfiguration: %s (unrecognized priority mode %s)\n", e.what(), priority_mode.c_str());
	}

	try {
		const auto &type_config = block_type_.at(priority_type_mb_sl_1);
		use_pmap_type_sl_1_ = type_config;

		REPORT("PriorityConfiguration: PM SL-1 type[%s] PM SL-1 num[%d]", priority_type_mb_sl_1.c_str(), use_pmap_type_sl_1_);
	} catch (const std::out_of_range &e) {
		ERR("PriorityConfiguration: %s (unrecognized priority type (sl-1) %s)\n", e.what(), priority_type_mb_sl_1.c_str());
	}

	try {
		const auto &type_config = block_type_.at(priority_type_mb_sl_2);
		use_pmap_type_sl_2_ = type_config;

		REPORT("PriorityConfiguration: PM SL-2 type[%s] PM SL-2 num[%d]", priority_type_mb_sl_2.c_str(), use_pmap_type_sl_2_);

	} catch (const std::out_of_range &e) {
		ERR("PriorityConfiguration: %s (unrecognized priority type (sl-2) %s)\n", e.what(), priority_type_mb_sl_2.c_str());
	}

}

EncodingMode PriorityConfiguration::encoding_mode() const { return encoding_mode_; }

bool PriorityConfiguration::use_priority_map_sl_1() const { return use_pmap_sl_1_; }
bool PriorityConfiguration::use_priority_map_sl_2() const { return use_pmap_sl_2_; }
PriorityMBType PriorityConfiguration::priority_map_type_sl_1() const { return use_pmap_type_sl_1_; }
PriorityMBType PriorityConfiguration::priority_map_type_sl_2() const { return use_pmap_type_sl_2_; }

} // namespace lctm
