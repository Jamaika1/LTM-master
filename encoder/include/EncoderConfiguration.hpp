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
//               Lorenzo Ciccarelli (lorenzo.ciccarelli@v-nova.com)
//
// EncoderConfiguration.hpp
//

#pragma once

#include <iostream>

#include "Image.hpp"
#include "LayerEncodeFlags.hpp"
#include "PriorityConfiguration.hpp"
#include "SignaledConfiguration.hpp"
#include "Types.hpp"

namespace lctm {
// Current encoder configuration - local to encoder - derived from command line and json parameters
//
struct EncoderConfiguration {
	unsigned temporal_cq_sw_multiplier;

	unsigned delta_sw_mult_gop08;
	unsigned delta_sw_mult_gop04;
	unsigned delta_sw_mult_gop02;
	unsigned delta_sw_mult_gop01;

	EncodingMode encoding_mode;
	bool temporal_use_priority_map_sl_1;
	bool temporal_use_priority_map_sl_2;
	PriorityMBType priority_type_sl_1;
	PriorityMBType priority_type_sl_2;
	unsigned sad_threshold;
	unsigned sad_coeff_threshold;
	unsigned quant_reduced_deadzone;

	bool no_enhancement_temporal_layer;
	UserDataMethod user_data_method;

	unsigned base_qp;
};

} // namespace lctm
