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
// Old school sprintf into a std::string
//
#include "Diagnostics.hpp"

#include <cstdarg>
#include <cstdio>
#include <sstream>

namespace lctm {

std::string format(const char *fmt, ...) {
	char tmp[16 * 1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp) - 1, fmt, args);
	va_end(args);

	return tmp;
}

std::string join(const std::vector<std::string> &list, const std::string &seperator, bool include_trailing) {
	std::string r;

	auto i = list.begin();
	while (i != list.end()) {
		r += *i;
		++i;
		if (i != list.end() || include_trailing)
			r += seperator;
	}

	return r;
}

void _check_failed(const char *file, int line, const char *func, const std::string &message) {
	fprintf(stderr, "Error: %s:%d (%s) %s\n", file, line, func, message.c_str());
#ifdef DIAGNOSTICS_CHECK_ABORTS
	abort()
#else
	exit(10);
#endif
}

void _raise(const char *file, int line, const char *func, const std::string &message) {
#ifdef DIAGNOSTICS_ERR_THROWS
	throw std::runtime_error(message.c_str());
#else
	exit(20);
#endif
}

void _log(const char *file, int line, const char *func, const std::string &message) {
	const std::string source_coords = format("%s:%d (%s) ", file, line, func);
	fprintf(stderr, "%s%s\n", source_coords.c_str(), message.c_str());
	// prevent log "concurrency"
	fflush(stderr);
}

void _log_report(const std::string &message) {
	fprintf(stderr, "%s\n", message.c_str());
	// prevent log "concurrency"
	fflush(stderr);
}

std::string hex_dump(const uint8_t *data, uint64_t size, unsigned int offset) {
	const int bytes_per_line = 16;
	std::string r;

	for (uint64_t l = 0; l < size; l += bytes_per_line) {

		r += format(" /* %04x */ ", offset + l);
		// Bytes
		for (uint64_t b = 0; b < bytes_per_line; ++b) {
			if (l + b < size)
				r += format("0x%02x, ", data[l + b]);
			else
				r += format("      ");
		}

		r += format(" // ");

		// Chars
		for (uint64_t b = 0; b < bytes_per_line; ++b) {
			if (l + b < size)
				r += format("%c", isprint(data[l + b]) ? data[l + b] : '.');
		}

		r += format("\n");
	}

	return r;
}

} // namespace lctm
