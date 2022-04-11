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
#pragma once

#include "uPlatform.h"
#include "uTypes.h"

namespace vnova { namespace utility
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// IntegralConstant - used to generate a compile time value constant that can be used for
	// conditions checking.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<class T, T v> struct IntegralConstant { static const T Value = v; };
	typedef IntegralConstant<bool, true> TrueType;
	typedef IntegralConstant<bool, false> FalseType;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// IsIntegral - Compile time helper that indicates if a type is integral. These have been 
	// limited to the explicit types we utilise.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename T> struct IsIntegral						: public FalseType {};
	template<typename T> struct IsIntegral<const T>				: public IsIntegral<T> {};	// Helper to strip const from T.
	template<typename T> struct IsIntegral<volatile T>			: public IsIntegral<T> {};	// Helper to strip volatile from T.
	template<typename T> struct IsIntegral<volatile const T>	: public IsIntegral<T> {};	// Helper to strip volatile const from T.
	template<> struct IsIntegral<uint8_t>						: public TrueType {};
	template<> struct IsIntegral<uint16_t>						: public TrueType {};
	template<> struct IsIntegral<uint32_t>						: public TrueType {};
	template<> struct IsIntegral<uint64_t>						: public TrueType {};
	template<> struct IsIntegral<int8_t>						: public TrueType {};
	template<> struct IsIntegral<int16_t>						: public TrueType {};
	template<> struct IsIntegral<int32_t>						: public TrueType {};
	template<> struct IsIntegral<int64_t>						: public TrueType {};
	template<> struct IsIntegral<bool>							: public TrueType {};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// IsFloat - Compile time helper that indicates if a type is floating pointer. These have been 
	// limited to the explicit types we utilise.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename T> struct IsFloat							: public FalseType {};
	template<typename T> struct IsFloat<const T>				: public IsFloat<T> {};		// Helper to strip const from T.
	template<typename T> struct IsFloat<volatile T>				: public IsFloat<T> {};		// Helper to strip volatile from T.
	template<typename T> struct IsFloat<volatile const T>		: public IsFloat<T> {};		// Helper to strip volatile const from T.
	template<> struct IsFloat<float>							: public TrueType {};
	template<> struct IsFloat<double>							: public TrueType {};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// IsSigned - Compile time helper that indicates if a type is signed. These have been 
	// limited to the explicit types we utilise.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename T> struct IsSigned						: public FalseType {};
	template<typename T> struct IsSigned<const T>				: public IsSigned<T> {};	// Helper to strip const from T.
	template<typename T> struct IsSigned<volatile T>			: public IsSigned<T> {};	// Helper to strip volatile from T.
	template<typename T> struct IsSigned<volatile const T>		: public IsSigned<T> {};	// Helper to strip volatile const from T.
	template<> struct IsSigned<uint8_t>							: public FalseType {};
	template<> struct IsSigned<uint16_t>						: public FalseType {};
	template<> struct IsSigned<uint32_t>						: public FalseType {};
	template<> struct IsSigned<uint64_t>						: public FalseType {};
	template<> struct IsSigned<int8_t>							: public TrueType {};
	template<> struct IsSigned<int16_t>							: public TrueType {};
	template<> struct IsSigned<int32_t>							: public TrueType {};
	template<> struct IsSigned<int64_t>							: public TrueType {};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// IsEnum - Compile time helper that indicates if a type is an enum.
	// @note: Uses C++11 feature std::is_enum, disable if on an unsupporting compiler.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename T> struct IsEnum							: public IntegralConstant<typename std::is_enum<T>::value_type, std::is_enum<T>::value> {};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Low bit mask - Generates an integer with the lowest N contiguous bits set of the supplied
	// integer type.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename INTEGER_TYPE, uint32_t Bits>
	struct LowBitMask
	{
		VNStaticAssert(Bits <= (sizeof(INTEGER_TYPE) * 8));
		static const INTEGER_TYPE Value = (1 << Bits) - 1;
	};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// MinValue - Generates the minimum value represented by the lowest N contiguous bits, 
	// accounting for a signed bit when INTEGER_TYPE is signed.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename INTEGER_TYPE, uint32_t Bits>
	struct MinValue
	{
		static const INTEGER_TYPE Value = IsSigned<INTEGER_TYPE>::Value ? ~(LowBitMask<INTEGER_TYPE, Bits>::Value) : 0;
	};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// MaxValue - Generates the maximum value represented by the lowest N contiguous bits.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<typename INTEGER_TYPE, uint32_t Bits>
	struct MaxValue
	{
		static const INTEGER_TYPE Value = LowBitMask<INTEGER_TYPE, Bits>::Value;
	};
}}
