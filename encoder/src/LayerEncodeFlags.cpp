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
//
// LayerEncodeFlags.cpp
//
// Store an array of flags indicating which residual layers should
// be encoded and which should be dropped
//

#include "LayerEncodeFlags.hpp"
#include <cassert>

namespace lctm {

EncodeBits::EncodeBits(EncodingMode enc_mode) : enc_mode_(enc_mode) {}

EncodingMode EncodeBits::encoding_mode() const { return enc_mode_; }

EncodeBits_DD::EncodeBits_DD(EncodingMode enc_mode) : EncodeBits(enc_mode) {
	// Do not allow AA for DD, only for DDS
	assert(enc_mode != EncodingMode::AA);

	encode_flags_.reset();
	switch (enc_mode) {
	case ENCODE_ALL:
		for (size_t i = 0; i < encode_flags_.size(); ++i) {
			encode_flags_.set(i, true);
		}
		break;
	case Ax:
		encode_flags_.set(0, true);
		break;
	case ENCODE_NONE:
		encode_flags_.reset();
		break;
	default:
		break;
	}
}

TransformType EncodeBits_DD::transform_type() const { return TransformType::TransformType_DD; }

bool EncodeBits_DD::encode_residual(unsigned layer) const {
	assert(layer < encode_flags_.size());
	return encode_flags_[layer];
}

size_t EncodeBits_DD::size() const { return encode_flags_.size(); }

EncodeBits_DDS::EncodeBits_DDS(EncodingMode enc_mode) : EncodeBits(enc_mode) {
	encode_flags_.reset();
	switch (enc_mode) {
	case ENCODE_ALL:
		for (size_t i = 0; i < encode_flags_.size(); ++i) {
			encode_flags_[i] = true;
		}
		break;
	case Ax:
		encode_flags_[0] = encode_flags_[1] = encode_flags_[2] = encode_flags_[3] = true;
		break;
	case AA:
		encode_flags_[0] = true;
		break;
	case NA:
		 encode_flags_[1] = encode_flags_[2] = encode_flags_[3] = true;
		break;
	case ENCODE_NONE:
		encode_flags_.reset();
		break;
	default:
		break;
	}
}

TransformType EncodeBits_DDS::transform_type() const { return TransformType::TransformType_DDS; }

bool EncodeBits_DDS::encode_residual(unsigned layer) const {
	assert(layer < encode_flags_.size());
	return encode_flags_[layer];
}

size_t EncodeBits_DDS::size() const { return encode_flags_.size(); }

LayerEncodeFlags::LayerEncodeFlags(TransformType transform_type, EncodingMode enc_mode) {
	assert(transform_type == TransformType::TransformType_DD || transform_type == TransformType::TransformType_DDS);
	if (transform_type == TransformType::TransformType_DD) {
		encode_flags_ = std::unique_ptr<EncodeBits_DD>(new EncodeBits_DD(enc_mode));
	} else if (transform_type == TransformType::TransformType_DDS) {
		encode_flags_ = std::unique_ptr<EncodeBits_DDS>(new EncodeBits_DDS(enc_mode));
	}
}

TransformType LayerEncodeFlags::transform_type() const { return encode_flags_->transform_type(); }

EncodingMode LayerEncodeFlags::encoding_mode() const { return encode_flags_->encoding_mode(); }

bool LayerEncodeFlags::encode_residual(unsigned layer) const { return encode_flags_->encode_residual(layer); }

size_t LayerEncodeFlags::size() const { return encode_flags_->size(); }

} // namespace lctm
