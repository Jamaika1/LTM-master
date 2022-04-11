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
// Convert.hpp
//
// Convert surfaces int?<->fixed16
//
#pragma once

#include "Component.hpp"
#include "Surface.hpp"

namespace lctm {

// U8 = S?.?
//
class ConvertToU8 : public Component {
public:
	ConvertToU8() : Component("ConvertToU8") {}
	Surface process(const Surface &surface, unsigned shift);
};

// S?.? to U8
//
class ConvertFromU8 : public Component {
public:
	ConvertFromU8() : Component("ConvertFromU8") {}
	Surface process(const Surface &surface, unsigned shift);
};

// U16 = S?.?
//
class ConvertToU16 : public Component {
public:
	ConvertToU16() : Component("ConvertToU16") {}
	Surface process(const Surface &surface, unsigned shift);
};

// S?.? to U16
//
class ConvertFromU16 : public Component {
public:
	ConvertFromU16() : Component("ConvertFromU16") {}
	Surface process(const Surface &surface, unsigned shift);
};

// surface xx bit
// dump internal format as 10 bit (for  8 bit source)
class ConvertDumpU08toU10 : public Component {
public:
	ConvertDumpU08toU10() : Component("ConvertDumpU08toU10") {}
	Surface process(const Surface &surface);
};

// surface xx bit
// dump internal format as 10 bit (for 15 bit source)
class ConvertDumpS15toU10 : public Component {
public:
	ConvertDumpS15toU10() : Component("ConvertDumpS15toU10") {}
	Surface process(const Surface &surface);
};

class ConvertToInternal : public Component {
public:
	ConvertToInternal() : Component("ConvertToInternal") {}
	Surface process(const Surface &surface, unsigned depth);
};
class ConvertFromInternal : public Component {
public:
	ConvertFromInternal() : Component("ConvertFromInternal") {}
	Surface process(const Surface &surface, unsigned depth);
};

// Convert between base and enhancement bit depths
// Wrapper class
class ConvertBitShift : public Component {
public:
	ConvertBitShift() : Component("ConvertBitShift") {}
	Surface process(const Surface &surface, unsigned depth_src, unsigned depth_dst);
};

// Perform Shfiting operation
class ConvertLeftShiftFromU8 : public Component {
public:
	ConvertLeftShiftFromU8() : Component("ConvertLeftShiftFromU8") {}
	Surface process(const Surface &surface, unsigned shift);
};
class ConvertLeftShiftFromU16 : public Component {
public:
	ConvertLeftShiftFromU16() : Component("ConvertLeftShiftFromU16") {}
	Surface process(const Surface &surface, unsigned shift);
};
class ConvertRightShiftToU8 : public Component {
public:
	ConvertRightShiftToU8() : Component("ConvertRightShiftToU8") {}
	Surface process(const Surface &surface, unsigned shift);
};
class ConvertRightShiftToU16 : public Component {
public:
	ConvertRightShiftToU16() : Component("ConvertRightShiftToU16") {}
	Surface process(const Surface &surface, unsigned shift);
};

} // namespace lctm
