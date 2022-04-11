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
// Diagnostics.hpp
//
// Various error checking and logging mechanisms
//
#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace lctm {

std::string format(const char *fmt, ...);
std::string hex_dump(const uint8_t *data, uint64_t size, unsigned int offset);
std::string join(const std::vector<std::string> &list, const std::string &seperator, bool include_trailing = false);

//// CHECK(expr): Macro that act as expression - passing a value though, and checking it for !=0 on the way through
//
// Unlike assert(), the expression will always be evaluated
//
// NB: The error string formatting and associated expressions are not evaluated if the check passes.

void _check_failed(const char *file, int line, const char *func, const std::string &message);

template <typename T> T _check(T c, const char *file, int line, const char *func, const char *expr) {
	if (!c)
		_check_failed(file, line, func, format("CHECK(%s) failed.", expr));

	return c;
}

#define CHECK(expr) lctm::_check(expr, __FILE__, __LINE__, __FUNCTION__, #expr)

//// Logging
//
//
// DBG(format, ....)  : Developer debugging
// INFO(format, ....) : Informative messages
// WARN(format, ....) : Warnings - user may need to take action
// ERR(format, ....) : Error - Configuration or input problems, something user can fix
// FATAL(format, ....) : Fatal - Internal errors that developer will likely need to fix
//
//
void _log(const char *file, int line, const char *func, const std::string &message);
void _log_report(const std::string &message);
void _raise(const char *file, int line, const char *func, const std::string &message);

#ifdef NDEBUG
#define DBG(fmt, ...) ((void)(0))
#else
#define DBG(fmt, ...) lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__))
#endif

#define INFO(fmt, ...) lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__))
#define WARN(fmt, ...) lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__))
#define REPORT(fmt, ...) lctm::_log_report(lctm::format(fmt, ##__VA_ARGS__))

#define ERR(fmt, ...)                                                                                                              \
	do {                                                                                                                           \
		std::string msg(lctm::format("Error " fmt, ##__VA_ARGS__));                                                                \
		lctm::_log(__FILE__, __LINE__, __FUNCTION__, msg);                                                                         \
		lctm::_raise(__FILE__, __LINE__, __FUNCTION__, msg);                                                                       \
	} while (0)

#define FATAL(fmt, ...)                                                                                                            \
	do {                                                                                                                           \
		std::string msg(lctm::format("Fatal " fmt, ##__VA_ARGS__));                                                                \
		lctm::_log(__FILE__, __LINE__, __FUNCTION__, msg);                                                                         \
		lctm::_raise(__FILE__, __LINE__, __FUNCTION__, msg);                                                                       \
	} while (0)

//// Temporary debugging
//
// D(fmt, ...)
//
#ifdef NO_D_DEBUG
#define D(fmt, ...) ((void)(0))
#else
#define D(fmt, ...) ::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#endif

//// Verbose tracing
//
// TRACE(fmt, ...)
// TRACE_PIXEL(fmt, ...)
// TRACE_SURFACE(surface)
// TRACE_HUFFMAN(fmt, ...)
//
#if VERBOSE_TRACE
#define TRACE(fmt, ...)                                                                                                            \
	{                                                                                                                              \
		lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__));                                            \
		fflush(stderr);                                                                                                            \
	}
#else
#define TRACE(fmt, ...) ((void)(0))
#endif

#if VERBOSE_TRACE_PIXEL
#define TRACE_PIXEL(fmt, ...) lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__))
#else
#define TRACE_PIXEL(fmt, ...) ((void)(0))
#endif

#if VERBOSE_TRACE_SURFACE
#define TRACE_SURFACE(surface)                                                                                                     \
	{                                                                                                                              \
		SurfaceView<uint16_t> oSurfaceView = surface.view_as<uint16_t>();                                                          \
		const uint16_t *ps = oSurfaceView.data();                                                                                  \
		for (unsigned int i = 0; i < oSurfaceView.height(); i++) {                                                                 \
			for (unsigned int j = 0; j < oSurfaceView.width(); j++) {                                                              \
				fprintf(stderr, "%4d ", *ps++);                                                                                    \
			}                                                                                                                      \
			fprintf(stderr, "\n");                                                                                                 \
		}                                                                                                                          \
		fflush(stderr);                                                                                                            \
	}
#else
#define TRACE_SURFACE(surface) ((void)(0))
#endif

#if VERBOSE_TRACE_HUFFMAN
#define TRACE_HUFFMAN(fmt, ...) lctm::_log(__FILE__, __LINE__, __FUNCTION__, lctm::format(fmt, ##__VA_ARGS__))
#else
#define TRACE_HUFFMAN(fmt, ...) ((void)(0))
#endif

} // namespace lctm
