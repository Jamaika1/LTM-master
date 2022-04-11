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
#include "uString.h"

#include <algorithm>
#include <cctype>

namespace vnova {
namespace utility {
namespace string {
namespace {
static inline size_t PathLastSlash(const std::string &path) {
	size_t p = path.rfind('/');
#if WIN32
	size_t p1 = path.rfind('\\');
	if ((p1 != std::string::npos) && ((p1 > p) || (p == std::string::npos)))
		p = p1;
#endif
	return p;
}
} // namespace

bool IEquals(const std::string &a, const std::string &b) {
	if (a.length() != b.length()) {
		return false;
	}

	const char *aptr = a.c_str();
	const char *bptr = b.c_str();

	while (*aptr && *bptr) {
		if (std::toupper(*aptr) != std::toupper(*bptr)) {
			return false;
		}

		aptr++;
		bptr++;
	}

	return true;
}

const std::string &ToLower(std::string &str) {
	std::transform(str.begin(), str.end(), str.begin(), (int (*)(int))std::tolower);
	return str;
}

std::string ToLowerCopy(const std::string &str) {
	std::string res = str;
	std::transform(res.begin(), res.end(), res.begin(), (int (*)(int))std::tolower);
	return res;
}

const std::string &ToUpper(std::string &str) {
	std::transform(str.begin(), str.end(), str.begin(), (int (*)(int))std::toupper);
	return str;
}

std::string ToUpperCopy(const std::string &str) {
	std::string res = str;
	std::transform(res.begin(), res.end(), res.begin(), (int (*)(int))std::toupper);
	return res;
}

std::string PathDirectory(const std::string &path) {
	size_t p = PathLastSlash(path);

	if (p != std::string::npos)
		return path.substr(0, p + 1);

	return "";
}

std::string PathFile(const std::string &path) {
	size_t p = PathLastSlash(path);

	if (p == std::string::npos)
		return path;

	return path.substr(p + 1, path.length() - p);
}

std::string PathExtension(const std::string &path) {
	size_t p = path.rfind('.');
	std::string res;

	if (p != std::string::npos) {
		res = path.substr(p + 1, res.length() - p);
	}

	return ToLower(res);
}

const std::string &PathNormalise(std::string &path, bool bDirectory) {
	if (path.length() == 0)
		return path;

	// Convert backslashes to forward.
	std::replace(path.begin(), path.end(), '\\', '/');

	// Cap with a slash
	if (bDirectory) {
		if (path[path.length() - 1] != '/') {
			path.append("/");
		}
	}

	return path;
}
} // namespace string
} // namespace utility
} // namespace vnova
