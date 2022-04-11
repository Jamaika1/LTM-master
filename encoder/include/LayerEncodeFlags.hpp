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
// LayerEncodeFlags.hpp
//

#pragma once

#include <bitset>
#include <memory>

#include "Types.hpp"

namespace lctm {

enum EncodingMode { ENCODE_ALL = 0, Ax = 1, AA = 2, NA = 3, ENCODE_NONE = 4 };

class EncodeBits {
public:
	EncodeBits(EncodingMode enc_mode);

	EncodingMode encoding_mode() const;

	virtual TransformType transform_type() const = 0;

	virtual bool encode_residual(unsigned layer) const = 0;

	virtual size_t size() const = 0;

	virtual ~EncodeBits() = default;

private:
	const EncodingMode enc_mode_;
};

class EncodeBits_DD : public EncodeBits {
public:
	EncodeBits_DD(EncodingMode enc_mode);

	TransformType transform_type() const override;

	bool encode_residual(unsigned layer) const override;

	size_t size() const override;

	~EncodeBits_DD() override {}

private:
	// Array of flags indicating whether to encode/not encode
	// residuals in each layer
	std::bitset<4> encode_flags_;
};

class EncodeBits_DDS : public EncodeBits {
public:
	EncodeBits_DDS(EncodingMode enc_mode);

	TransformType transform_type() const override;

	bool encode_residual(unsigned layer) const override;

	size_t size() const override;

	~EncodeBits_DDS() override {}

private:
	// Array of flags indicating whether to encode/not encode
	// residuals in each layer
	std::bitset<16> encode_flags_;
};

class LayerEncodeFlags {

public:
	LayerEncodeFlags(TransformType transform_type, EncodingMode enc_mode);

	TransformType transform_type() const;

	EncodingMode encoding_mode() const;

	bool encode_residual(unsigned layer) const;

	size_t size() const;

private:
	std::unique_ptr<EncodeBits> encode_flags_;
};

} // namespace lctm
