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

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <string>
#include <vector>

#ifdef _WIN32
#if _MSC_VER <= 1700

#ifndef PRId32
#	define PRId32 "d"
#endif

#ifndef PRIu32
#	define PRIu32 "u"
#endif

#ifndef PRId64
#	define PRId64 "ld"
#endif

#ifndef PRIx64
# define PRIx64 "llx"
#endif

#ifndef PRIu64
#	define PRIu64 "llu"
#endif

#else
#include <inttypes.h>
#endif
#else
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#ifndef PRId32
#	define PRId32 "d"
#endif

#ifndef PRIu32
#	define PRIu32 "u"
#endif

#ifndef PRId64
#	define PRId64 "ld"
#endif

#ifndef PRIx64
# define PRIx64 "llx"
#endif

#ifndef PRIu64
#	define PRIu64 "lu"
#endif
#endif

namespace vnova { namespace utility
{
	template <typename T>
	class Callback
	{
	public:
		Callback() : m_callback(nullptr), m_data(nullptr) {};
		T		m_callback;
		void*	m_data;
	};

	typedef std::vector<uint8_t> DataBuffer;
}}
