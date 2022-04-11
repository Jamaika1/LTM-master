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
//// Deserializer.hpp
//
// Read a bitstream packet, and updates the provided configuration with any new settings from packet
//
#pragma once

#include "BitstreamUnpacker.hpp"
#include "Component.hpp"
#include "Packet.hpp"
#include "SignaledConfiguration.hpp"
#include "Surface.hpp"

namespace lctm {

class Deserializer : public Component {
public:
	Deserializer(const Packet &packet, SignaledConfiguration &dst_configuration,
	             Surface (&symbols)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]);

	bool has_more() const;
	unsigned parse_block();

	void parse_sequence_config(SequenceConfiguration &sequence_configuration, BitstreamUnpacker &b);
	void parse_global_config(GlobalConfiguration &global_configuration, BitstreamUnpacker &b);
	void parse_picture_config(PictureConfiguration &picture_configuration, BitstreamUnpacker &b, unsigned num_residual_layers,
	                          bool temporal_enabled);
	void parse_encoded_data(SignaledConfiguration &dst_configuration, BitstreamUnpacker &b, unsigned num_planes,
	                        Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]);
	void parse_encoded_data_tiled(SignaledConfiguration &dst_configuration, BitstreamUnpacker &b, unsigned num_planes,
	                              Surface symbols[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS]);
	void parse_filler(BitstreamUnpacker &b);
	void parse_additional_info(AdditionalInfo &additional_info, BitstreamUnpacker &b);

private:
	PacketView view_;
	BitstreamUnpacker b_;
	SignaledConfiguration &dst_configuration_;
	Surface (&symbols_)[MAX_NUM_PLANES][MAX_NUM_LOQS][MAX_NUM_LAYERS];
};

} // namespace lctm
