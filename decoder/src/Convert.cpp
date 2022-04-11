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
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)
//               Stefano Battista (bautz66@gmail.com)
//
// Convert.cpp
//

#include "Convert.hpp"
#include "Config.hpp"
#include "Misc.hpp"

namespace lctm {

Surface ConvertToU8::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<int16_t> src;
		unsigned shift;
		int16_t half;
	} context = {surface.view_as<int16_t>(), shift, (int16_t)((1 << shift) / 2)};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint8_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const int16_t *__restrict psrc = context.src.data(0, y);
		uint8_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, clamp(((context.src.read(x, y) + 0x4000 + context.half) >> context.shift), 0, 255));
			*pdst++ = clamp(((*psrc++ + 0x4000 + context.half) >> context.shift), 0, 255);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint8_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint8_t {
		        return clamp(((c.src.read(x, y) + 0x4000 + c.half) >> c.shift), 0, 255);
	        },
	        context)
	    .finish();
#endif
}

Surface ConvertFromU8::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint8_t> src;
		unsigned shift;
	} context = {
	    surface.view_as<uint8_t>(),
	    shift,
	};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint8_t *__restrict psrc = context.src.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, ((context.src.read(x, y) << context.shift) - 0x4000));
			*pdst++ = ((*psrc++ << context.shift) - 0x4000);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> int16_t { return (c.src.read(x, y) << c.shift) - 0x4000; }, context)
	    .finish();
#endif
}

Surface ConvertToU16::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<int16_t> src;
		unsigned shift;
		int16_t half;
	} context = {surface.view_as<int16_t>(), shift, (int16_t)((1 << shift) / 2)};
#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const int16_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, clamp(((context.src.read(x, y) + 0x4000 + context.half) >> context.shift), 0, 32767 >> context.shift));
			*pdst++ = clamp((*psrc++ + 0x4000 + context.half) >> context.shift, 0, 32767 >> context.shift);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t {
		        return clamp(((c.src.read(x, y) + 0x4000 + c.half) >> c.shift), 0, 32767 >> c.shift);
	        },
	        context)
	    .finish();
#endif
}

Surface ConvertFromU16::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint16_t> src;
		unsigned shift;
	} context = {
	    surface.view_as<uint16_t>(),
	    shift,
	};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<int16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint16_t *__restrict psrc = context.src.data(0, y);
		int16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, ((context.src.read(x, y) << context.shift) - 0x4000));
			*pdst++ = ((*psrc++ << context.shift) - 0x4000);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<int16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> int16_t { return (c.src.read(x, y) << c.shift) - 0x4000; }, context)
	    .finish();
#endif
}

Surface ConvertDumpU08toU10::process(const Surface &surface) {
	struct Context {
		const SurfaceView<uint8_t> src;
	} context = {surface.view_as<uint8_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint8_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, (context.src.read(x, y) << 2));
			*pdst++ = (*psrc++ << 2);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t { return ((c.src.read(x, y)) << 2); }, context)
	    .finish();
#endif
}

Surface ConvertDumpS15toU10::process(const Surface &surface) {
	struct Context {
		const SurfaceView<int16_t> src;
	} context = {surface.view_as<int16_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const int16_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			// dest.write(x, y, (((uint32_t)context.src.read(x, y) + (1 << 14) + (1 << 4)) >> 5));
			*pdst++ = (((uint32_t)context.src.read(x, y) + (1 << 14) + (1 << 4)) >> 5);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t {
		        int32_t value = ((uint32_t)c.src.read(x, y) + (1 << 14) + (1 << 4)) >> 5;
		        return ((uint16_t)value);
	        },
	        context)
	    .finish();
#endif
}

Surface ConvertToInternal::process(const Surface &surface, unsigned depth) {
	switch (depth) {
	case 8:
		return ConvertFromU8().process(surface, 7);
	case 10:
		return ConvertFromU16().process(surface, 5);
	case 12:
		return ConvertFromU16().process(surface, 3);
	case 14:
		return ConvertFromU16().process(surface, 1);
	case 16:
		return surface;
	default:
		CHECK(0);
		return Surface();
	}
}

Surface ConvertFromInternal::process(const Surface &surface, unsigned depth) {
	switch (depth) {
	case 8:
		return ConvertToU8().process(surface, 7);
	case 10:
		return ConvertToU16().process(surface, 5);
	case 12:
		return ConvertToU16().process(surface, 3);
	case 14:
		return ConvertToU16().process(surface, 1);
	case 16:
		return surface;
	default:
		CHECK(0);
		return Surface();
	}
}

Surface ConvertBitShift::process(const Surface &surface, unsigned depth_src, unsigned depth_dst) {
	if (depth_src == depth_dst)
		return surface;
	else if (depth_dst > depth_src) {
		if (depth_src == 8) {
			return ConvertLeftShiftFromU8().process(surface, depth_dst - depth_src);
		} else {
			return ConvertLeftShiftFromU16().process(surface, depth_dst - depth_src);
		}
	} else { // depth_dst < depth_src
		if (depth_dst == 8) {
			return ConvertRightShiftToU8().process(surface, depth_src - depth_dst);
		} else {
			return ConvertRightShiftToU16().process(surface, depth_src - depth_dst);
		}
	}
}

Surface ConvertLeftShiftFromU8::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint8_t> src;
	} context = {surface.view_as<uint8_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint8_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			*pdst++ = (*psrc++ << shift);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t { return ((c.src.read(x, y)) << shift); }, context)
	    .finish();
#endif
}

Surface ConvertLeftShiftFromU16::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint16_t> src;
	} context = {surface.view_as<uint16_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint16_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			*pdst++ = (*psrc++ << shift);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t { return ((c.src.read(x, y)) << shift); }, context)
	    .finish();
#endif
}

Surface ConvertRightShiftToU8::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint16_t> src;
	} context = {surface.view_as<uint16_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint8_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint16_t *__restrict psrc = context.src.data(0, y);
		uint8_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			*pdst++ = (*psrc++ >> shift);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint8_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint8_t { return ((c.src.read(x, y)) >> shift); }, context)
	    .finish();
#endif
}

Surface ConvertRightShiftToU16::process(const Surface &surface, unsigned shift) {
	struct Context {
		const SurfaceView<uint16_t> src;
	} context = {surface.view_as<uint16_t>()};

#if defined __OPT_MATRIX__
	auto dest = Surface::build_from<uint16_t>();
	dest.reserve(surface.width(), surface.height());
	for (unsigned y = 0; y < surface.height(); ++y) {
		const uint16_t *__restrict psrc = context.src.data(0, y);
		uint16_t *__restrict pdst = dest.data(0, y);
		for (unsigned x = 0; x < surface.width(); ++x) {
			*pdst++ = (*psrc++ >> shift);
		}
	}
	return dest.finish();
#else
	return Surface::build_from<uint16_t>()
	    .generate<Context>(
	        surface.width(), surface.height(),
	        [](unsigned x, unsigned y, const Context &c) -> uint16_t { return ((c.src.read(x, y)) >> shift); }, context)
	    .finish();
#endif
}

} // namespace lctm
