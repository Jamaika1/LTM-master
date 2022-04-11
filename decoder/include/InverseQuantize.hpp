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
//
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//
// InversrQuantize.hpp
//
#pragma once

#include "Component.hpp"
#include "SignaledConfiguration.hpp"
#include "Surface.hpp"

extern const uint8_t default_qm_coefficient_4x4[][16];
extern const uint8_t default_qm_coefficient_2x2[][4];

namespace lctm {

int32_t find_quant_matrix_coeff(const PictureConfiguration &picture_configuration, unsigned num_layers, bool horizontal_only,
                                unsigned loq, unsigned layer, bool is_idr, int32_t previous);

int32_t find_dirq_step_width(int32_t orig_step_width, int32_t quant_matrix_coeff);

int32_t find_invq_offset(const PictureConfiguration &picture_configuration, int32_t orig_step_width, int32_t dirq_step_width);

int32_t find_invq_step_width(const PictureConfiguration &picture_configuration, int32_t dirq_step_width, int32_t invq_offset);

int32_t find_layer_deadzone(int32_t orig_step_width, int32_t dirq_step_width);

int32_t find_invq_applied_offset(const PictureConfiguration &picture_configuration, int32_t invq_offset, int32_t layer_deadzone);

class InverseQuantize : public Component {
public:
	InverseQuantize() : Component("InverseQuantize") {}
	Surface process(const Surface &src_plane, int32_t invq_step_width, int32_t applied_dequant_offset);
};

class InverseQuantize_SWM : public Component {
public:
	InverseQuantize_SWM() : Component("InverseQuantize_SWM") {}
	Surface process(const Surface &src_plane, unsigned transform_block_size, int32_t *invq_step_width,
	                int32_t *applied_dequant_offset, const Surface &temporal_map);
};

} // namespace lctm
