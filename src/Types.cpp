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
// Types.cpp
//
#include "Types.hpp"

#include "Diagnostics.hpp"
#include "Misc.hpp"

namespace lctm {

std::istream &operator>>(std::istream &in, Upsample &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "nearest")
		v = Upsample_Nearest;
	else if (s == "linear")
		v = Upsample_Linear;
	else if (s == "cubic")
		v = Upsample_Cubic;
	else if (s == "modifiedcubic")
		v = Upsample_ModifiedCubic;
	else if (s == "adaptivecubic")
		v = Upsample_AdaptiveCubic;
	else
		ERR("not an upsampler: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, Downsample &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "area")
		v = Downsample_Area;
	else if (s == "lanczos")
		v = Downsample_Lanczos;
	else if (s == "lanczos3")
		v = Downsample_Lanczos3;
	else
		ERR("not a downsapler: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, ScalingMode &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "none")
		v = ScalingMode_None;
	else if (s == "1d")
		v = ScalingMode_1D;
	else if (s == "2d")
		v = ScalingMode_2D;
	else
		ERR("not a scaling mode: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, DitheringType &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "none")
		v = Dithering_None;
	else if (s == "uniform")
		v = Dithering_Uniform;
	else if (s == "uniform_fixed")
		v = Dithering_UniformFixed;
	else
		ERR("not a dither type: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, DequantOffset &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "default")
		v = DequantOffset_Default;
	else if (s == "const_offset")
		v = DequantOffset_ConstOffset;
	else
		ERR("not a dequantization offset mode: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, TransformType &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "dd")
		v = TransformType_DD;
	else if (s == "dds")
		v = TransformType_DDS;
	else
		ERR("not a transform: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, BaseCoding &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "avc")
		v = BaseCoding_AVC;
	else if (s == "h264")
		v = BaseCoding_AVC;
	else if (s == "baseyuv")
		v = BaseCoding_AVC;
	else if (s == "baseyuv_avc")
		v = BaseCoding_AVC;
	else if (s == "hevc")
		v = BaseCoding_HEVC;
	else if (s == "h265")
		v = BaseCoding_HEVC;
	else if (s == "baseyuv_hevc")
		v = BaseCoding_HEVC;
	else if (s == "vvc")
		v = BaseCoding_VVC;
	else if (s == "baseyuv_vvc")
		v = BaseCoding_VVC;
	else if (s == "evc")
		v = BaseCoding_EVC;
	else if (s == "baseyuv_evc")
		v = BaseCoding_EVC;
	else if (s == "yuv")
		v = BaseCoding_YUV;
	else if (s == "none")
		v = BaseCoding_YUV;
	else if (s == "baseyuv_x265")
		v = BaseCoding_X265;
	else if (s == "x265")
		v = BaseCoding_X265;
	else
		ERR("not a base coding: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, Encapsulation &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "none")
		v = Encapsulation_None;
	else if (s == "sei" || s == "sei_unreg")
		v = Encapsulation_SEI_Unregistered;
	else if (s == "sei_reg")
		v = Encapsulation_SEI_Registered;
	else if (s == "nal")
		v = Encapsulation_NAL;
	else
		ERR("not an encapsulation: %s", s.c_str());

	return in;
}

std::istream &operator>>(std::istream &in, QuantMatrix &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "previous")
		v = QuantMatrix_BothPrevious;
	else if (s == "default")
		v = QuantMatrix_BothDefault;
	else if (s == "custom")
		v = QuantMatrix_SameAndCustom;
	else if (s == "custom_default")
		v = QuantMatrix_Level2CustomLevel1Default;
	else if (s == "default_custom")
		v = QuantMatrix_Level2DefaultLevel1Custom;
	else if (s == "custom_custom")
		v = QuantMatrix_DifferentAndCustom;
	else
		ERR("not a quantmatrix type: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, CompressionType &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "none")
		v = CompressionType_None;
	else if (s == "prefix")
		v = CompressionType_Prefix;
	else if (s == "prefix_diff")
		v = CompressionType_Prefix_OnDiff;
	else
		ERR("not a compression type: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, UserDataMode &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "none")
		v = UserData_None;
	else if (s == "2bits")
		v = UserData_2bits;
	else if (s == "6bits")
		v = UserData_6bits;
	else
		ERR("not a user data mode: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, UserDataMethod &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "zeros")
		v = UserDataMethod_Zeros;
	else if (s == "ones")
		v = UserDataMethod_Ones;
	else if (s == "random")
		v = UserDataMethod_Random;
	else if (s == "fixed_random")
		v = UserDataMethod_FixedRandom;
	else
		ERR("not a user data method: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, PictureType &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "frame")
		v = PictureType_Frame;
	else if (s == "field")
		v = PictureType_Field;
	else
		ERR("not a picture type: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, Profile &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "main")
		v = Profile_Main;
	else if (s == "main444")
		v = Profile_Main444;
	else if (s == "auto")
		v = Profile_Auto;
	else
		ERR("not a profile: %s", s.c_str());
	return in;
}

std::istream &operator>>(std::istream &in, ParameterConfig &v) {
	std::string s;
	in >> s;
	s = lowercase(s);

	if (s == "default")
		v = ParameterConfig_Default;
	else if (s == "conformance")
		v = ParameterConfig_Conformance;
	else
		ERR("not a parameter configuration: %s", s.c_str());

	return in;
}

} // namespace lctm
