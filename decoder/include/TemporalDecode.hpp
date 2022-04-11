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
//
// Temporal.hpp
//
#pragma once

#include "Component.hpp"
#include "SignaledConfiguration.hpp"
#include "Surface.hpp"

namespace lctm {

class TemporalExtractMask : public Component {
public:
	TemporalExtractMask() : Component("TemporalExtractMask") {}
	Surface process(const Surface &symbols);
};

class TemporalClear : public Component {
public:
	TemporalClear() : Component("TemporalClear") {}
	Surface process(const Surface &symbols);
};

class TemporalUpdate : public Component {
public:
	TemporalUpdate() : Component("TemporalUpdate") {}
	Surface process(const Surface &temporal_plane, const Surface &residual_plane, /* const */ Surface &mask_plane,
	                unsigned transform_block_size, bool refresh, bool use_reduced_signalling);
};

class TemporalTileMap : public Component {
public:
	TemporalTileMap() : Component("TemporalTileMap") {}
	Surface process(Surface intra_symbols[MAX_NUM_LAYERS], Surface inter_symbols[MAX_NUM_LAYERS], unsigned transform_block_size);

private:
	static const std::vector<SurfaceView<int16_t>> collect_surfaces_DD(Surface symbols[MAX_NUM_LAYERS]);
	static const std::vector<SurfaceView<int16_t>> collect_surfaces_DDS(Surface symbols[MAX_NUM_LAYERS]);
};

class TemporalTileIntraSignal : public Component {
public:
	TemporalTileIntraSignal() : Component("TemporalTileIntraSignal") {}
	Surface process(const Surface &temporal_tile_map, const Surface &mask_plane, unsigned transform_block_size);
};

class TemporalTileIntraClear : public Component {
public:
	TemporalTileIntraClear() : Component("TemporalTileIntraClear") {}
	Surface process(Surface &temporal_tile_map, const Surface &temporal_signal_syms, unsigned transform_block_size);
};

class ApplyTemporalMap : public Component {
public:
	ApplyTemporalMap() : Component("ApplyTemporalMap") {}

	Surface process(const Surface &src_plane, Surface &map_plane, unsigned transform_block_size);
};

class UserDataClear : public Component {
public:
	UserDataClear() : Component("UserDataClear") {}
	Surface process(const Surface &symbols, UserDataMode user_data);
};

} // namespace lctm
