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
// Misc.hpp
//
#pragma once

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Diagnostics.hpp"

#define LCEVC_MIN(a, b) ((a) < (b) ? (a) : (b))
// #define LCEVC_MIN(a, b) std::min((a), (b))
#define LCEVC_MAX(a, b) ((a) > (b) ? (a) : (b))
// #define LCEVC_MAX(a, b) std::max((a), (b))

namespace lctm {

// Return the full path of the program executable.
//
std::string get_program();

// Return the path of the directory where the program executable lives, with given suffix appended
//
std::string get_program_directory(const std::string &suffix);

// Find file size in bytes
//
size_t file_size(int fd);
size_t file_size(const std::string &name);

// Read a named file into a string
//
std::string read_file(const std::string &name);

// Write the contents of a string to a named file
//
bool write_file(const std::string &name, const std::string &contents);

// Return extension part of filename, or ""
//
std::string file_extension(const std::string &filename);

// Convert string to lowervcase
//
std::string lowercase(const char *s);
std::string lowercase(const std::string &s);

// Split a source string in 0 or more tokens, returns number of strings generated
//
unsigned split(std::vector<std::string> &output, const std::string &src, const char *seperators);

// Convenience ptr for holding FILE*
//
struct UniquePtrFileClose {
	void operator()(FILE *f) {
		if (f)
			std::fclose(f);
	}
};
typedef std::unique_ptr<FILE, UniquePtrFileClose> UniquePtrFile;

// Clamp a value to [range]
//
// template <typename T> T clamp(T val, T l, T h) { return std::max(std::min(val, h), l); }
template <typename T> T clamp(T val, T l, T h) { return LCEVC_MAX(LCEVC_MIN(val, h), l); }

// Shift int64 down and clamp to int16
//
static inline int16_t shift_clamp_s16(int64_t v, int32_t shift) {
	const int64_t half = (int64_t)1 << (shift - 1);
	return (int16_t)clamp((v + half) >> shift, (int64_t)-32768, (int64_t)32767);
}

// Clamp to  signed 16 bit iteger range
//
static inline int16_t clamp_int16(int32_t i) { return clamp(i, -32768, 32767); }

// Integer log2
//
static inline unsigned log2(unsigned n) {
	unsigned r = 0;
	while (n >>= 1)
		r++;
	return r;
}

static inline unsigned ceil_log2(unsigned n) {
	if(n == 0)
		return 0;
	return log2(n-1)+1;
}

// Work out scale to make (a << s) >= b
//
static inline unsigned tile_size(unsigned a, unsigned b) {

	unsigned s = 0, t = 1;
	while ((a << s) < b) {
		t *= 2;
		++s;
	}

	return t;
}

// Return CRC64 of a buffer
//
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);

// Return a system-wide timstamp for debugging packet timing (in uSecs)
//
uint64_t system_timestamp();

// Generate a temporary filename given a suffix
std::string make_temporary_filename(const std::string &suffix);

// Wrap a string in a istringstream and extract into type
//
template <typename T> T extract(const std::string &s) {
	T r;
	try {
		std::istringstream ss(s);
		ss >> r;
	} catch (const std::ios_base::failure &) {
		ERR("Cannot extract %s", s.c_str());
	}
	return r;
}

template <typename T> T extract(const std::string &s, T dflt) {
	try {
		std::istringstream ss(s);
		T r;
		ss >> r;
		return r;
	} catch (const std::ios_base::failure &) {
		return dflt;
	}
}

} // namespace lctm

// Number of elements in an array
//
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
