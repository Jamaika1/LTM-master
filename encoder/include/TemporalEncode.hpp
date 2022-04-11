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
// TemporalRefresh.hpp
//

#pragma once

#include "Component.hpp"
#include "Dithering.hpp"
#include "Surface.hpp"
#include "Types.hpp"

namespace lctm {

class TemporalCost_2x2 : public Component {
public:
	TemporalCost_2x2() : Component("TemporalCost_2x2") {}
	Surface process(const Surface &src_plane, const Surface &recon_plane, const Surface *symb_plane, unsigned transform_block_size,
	                unsigned scale, bool intra);
};

class TemporalCost_4x4 : public Component {
public:
	TemporalCost_4x4() : Component("TemporalCost_4x4") {}
	Surface process(const Surface &src_plane, const Surface &recon_plane, const Surface *symb_plane, unsigned transform_block_size,
	                unsigned scale, bool intra);
};

class TemporalCost_SAD : public Component {
public:
	TemporalCost_SAD() : Component("TemporalCost_SAD") {}
	Surface process(const Surface &src_plane, const Surface &recon_plane, unsigned transform_block_size);
};

class TemporalSelect : public Component {
public:
	TemporalSelect() : Component("TemporalSelect") {}
	Surface process(const Surface &inter_plane, const Surface &intra_plane, const Surface &mask_plane);
};

class TemporalMask : public Component {
public:
	TemporalMask() : Component("TemporalMask") {}
	Surface process(const Surface &temporal_type, const Surface &temporal_type_next_plane, const Surface &residual_plane,
	                unsigned transform_block_size, bool refresh, bool use_reduced_signalling);
};

class TemporalInsertMask : public Component {
public:
	TemporalInsertMask() : Component("TemporalInsertMask") {}
	Surface process(const Surface &symbols, const Surface &mask, const bool refresh);
};

class UserDataInsert : public Component {
public:
	UserDataInsert() : Component("UserDataInsert") {}
	Surface process(const Surface &symbols, const UserDataMethod method, UserDataMode user_data, FILE *file=NULL);
};

} // namespace lctm
