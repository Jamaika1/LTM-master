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
//// Types.hpp
//
#pragma once

#include <iostream>

namespace lctm {

enum Upsample {
	Upsample_Nearest,
	Upsample_Linear,
	Upsample_Cubic,
	Upsample_ModifiedCubic,
	Upsample_AdaptiveCubic,
};

enum Downsample {
	Downsample_Area,
	Downsample_Lanczos,
	Downsample_Lanczos3,
};

enum ScalingMode {
	ScalingMode_None,
	ScalingMode_1D,
	ScalingMode_2D,
};

enum UserDataMode {
	UserData_None,
	UserData_2bits,
	UserData_6bits,
};

enum UserDataMethod {
	UserDataMethod_Zeros,
	UserDataMethod_Ones,
	UserDataMethod_Random,
	UserDataMethod_FixedRandom,
};

enum TileDimensions {
	TileDimensions_None,
	TileDimensions_512x256,
	TileDimensions_1024x512,
	TileDimensions_Custom,
};

enum QuantMatrix {
	QuantMatrix_BothPrevious,
	QuantMatrix_BothDefault,
	QuantMatrix_SameAndCustom,
	QuantMatrix_Level2CustomLevel1Default,
	QuantMatrix_Level2DefaultLevel1Custom,
	QuantMatrix_DifferentAndCustom,
};

enum CompressionType {
	CompressionType_None,
	CompressionType_Prefix,
	CompressionType_Prefix_OnDiff,
};

enum DitheringType {
	Dithering_None,
	Dithering_Uniform,
	Dithering_UniformFixed,
};

enum DequantOffset {
	DequantOffset_Default,
	DequantOffset_ConstOffset,
};

enum TransformType { TransformType_DD, TransformType_DDS };

enum PictureType { PictureType_Frame, PictureType_Field };

enum FieldType { FieldType_Top, FieldType_Bottom };

enum CodingType { CodingType_IDR, CodingType_NonIDR };

enum NalUnitType { LCEVC_NON_IDR = 28, LCEVC_IDR = 29, LCEVC_RSV = 30 };

enum Profile {
	Profile_Main,
	Profile_Main444,
	Profile_Auto,
};

// XXX Unify with BaseDecoder type
enum BaseCoding {
	BaseCoding_Unknown,
	BaseCoding_AVC,
	BaseCoding_HEVC,
	BaseCoding_VVC,
	BaseCoding_EVC,
	BaseCoding_YUV,
};

enum Encapsulation {
	Encapsulation_None,
	Encapsulation_SEI_Unregistered,
	Encapsulation_SEI_Registered,
	Encapsulation_NAL,
};

enum BaseFrameType { BaseFrame_IDR, BaseFrame_Intra, BaseFrame_Inter, BaseFrame_Pred, BaseFrame_Bidi };

enum Temporal_SWM {
	SWM_Disabled = 0,  // Don't apply temporal step width modifier
	SWM_Active = 1,    // Do apply temporal step width modifier (if conditions fulfilled)
	SWM_Dependent = 2, // Apply temporal step width modifier dependent on tile map
};

// Bitmask for presence of syntax blocks
//
enum SyntaxBlocks {
	SyntaxBlock_Sequence = 1,
	SyntaxBlock_Global = 2,
	SyntaxBlock_Picture = 4,
	SyntaxBlock_EncodedData = 8,
	SyntaxBlock_EncodedData_Tiled = 16,
	SyntaxBlock_Additional_Info = 32,
	SyntaxBlock_Filler = 64 
};

// Configurations with different default parameter settings
enum ParameterConfig {
	ParameterConfig_Default,
	ParameterConfig_Conformance,
};

// Labels for residuals (priority map, residual map)
// low and high values for residual mask (useful for display)
enum ResidualLabel { RESIDUAL_LIVE = 128, RESIDUAL_KILL = 192 };

enum TemporalType { TEMPORAL_PRED = 128, TEMPORAL_INTR = 192 };

// Parsing for parameters
//
std::istream &operator>>(std::istream &in, Upsample &v);
std::istream &operator>>(std::istream &in, Downsample &v);
std::istream &operator>>(std::istream &in, ScalingMode &v);
std::istream &operator>>(std::istream &in, QuantMatrix &v);
std::istream &operator>>(std::istream &in, DitheringType &v);
std::istream &operator>>(std::istream &in, DequantOffset &v);
std::istream &operator>>(std::istream &in, TransformType &v);
std::istream &operator>>(std::istream &in, BaseCoding &v);
std::istream &operator>>(std::istream &in, Encapsulation &v);
std::istream &operator>>(std::istream &in, CompressionType &v);
std::istream &operator>>(std::istream &in, UserDataMode &v);
std::istream &operator>>(std::istream &in, UserDataMethod &v);
std::istream &operator>>(std::istream &in, PictureType &v);
std::istream &operator>>(std::istream &in, Profile &v);
std::istream &operator>>(std::istream &in, ParameterConfig &v);

} // namespace lctm
