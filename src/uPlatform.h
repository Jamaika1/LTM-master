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

#include <assert.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include <stdexcept>
#include "uTypes.h"

#if defined(_WIN32)
	#define VNAlign(x) __declspec(align(x))
	#define VNEmptyFile() namespace { char NoEmptyfileDummy; }
	#define VNThreadLocal __declspec(thread)
#else
	#define VNAlign(x) __attribute__((aligned(x)))
	#define VNEmptyFile()
	#define VNThreadLocal __thread
#endif

#define VNUnused(x) (void)(x)

// Use VNStringify to convert a preprocessor defines value to a string.
#define VNStringify(a) VNStringifyHelper(a)
#define VNStringifyHelper(s) #s

// Use VNConcat to concatenate 2 values and make a new value, (i.e unique name generation based on file/line).
#define VNConcatHelper(a, b) a ## b
#define VNConcat(a, b) VNConcatHelper(a, b)

// Use VNAssert to trap runtime assertions.
#define VNAssert(condition)							assert((condition))
#define VNAssertf(condition, format, ...)			assert((condition))		// @todo: Actually print this message on failure.
#define VNFail(format, ...)							assert(false)			// @todo: Actually print this message.


// Use VNStaticAssert for compile time assertions, templates a good place for this.
#if defined(VNPlatformSupportsStaticAssert)
#define VNStaticAssert(condition) static_assert((condition), #condition)
#else
// Emulate static assert by generating invalid code when the condition fails. This will result 
// in a less obvious error message. Than the built in language support.
#ifdef __COUNTER__
#define VNStaticAssert(condition) ;enum { VNConcat(static_assert_counter_, __COUNTER__) = 1 / (int)(!!(condition)) }
#else
#define VNStaticAssert(condition) ;enum { VNConcat(static_assert_line_, __LINE__) = 1/(int)(!!(condition)) }
#endif
#endif

// On MSVC < VS2015, snprintf doesn't exist
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf(DEST, COUNT, FORMAT, ...) sprintf((DEST), (FORMAT),  __VA_ARGS__)
#endif

namespace vnova { namespace utility
{
	namespace os
	{
		std::string GetAppPath();
		std::string GetCwd();
	}

	namespace lib
	{
		void*		Open(const std::string& name, const std::string& version = "", std::string* returnmsg = NULL);
		bool		Close(void* handle);
		void*		GetSymbol(void* handle, const std::string& name);
		std::string GetError();

		template<typename T>
		inline static bool GetFunction(void* libHandle, const char* fnName, T& out, bool bThrowOnFail = false)
		{
			out = (T)GetSymbol(libHandle, fnName);

			if(out == nullptr && bThrowOnFail)
			{
		        printf("Failed to find lib function \n");
		        fflush(stdout);
				throw std::runtime_error("Failed to find lib function \n");
			}

			return (out != nullptr);
		}
	}
	
	namespace file
	{
		uint64_t	Tell(FILE* f);
		void		Seek(FILE* f, long long offset, int32_t origin);
		uint64_t	Size(FILE* f);
		FILE*		OpenFileSearched(const std::string& filename, const char* mode);
		bool		ReadContentsText(const std::string& filename, std::string& output);
		bool		ReadContentsBinary(const std::string& filename, DataBuffer& output);
		bool		Exists(const char* path);
		bool		Exists(const std::string& path);
		uint64_t	GetModifiedTime(const char* path);
	}
}}
