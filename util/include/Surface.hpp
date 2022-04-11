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
// Surface.hpp
//
// A 2D array of pixels
//
#pragma once

#include "Buffer.hpp"
#include "Diagnostics.hpp"

#include <cassert>
#include <cstring>
#include <functional>

namespace lctm {

class Surface;
//// SurfaceView
//
// Scoped view into surface - templated on underlying data type
//
template <typename T, unsigned S = 0> class SurfaceView {
public:
	SurfaceView(const Surface &surface);
	~SurfaceView();

	unsigned width() const;
	unsigned height() const;
	unsigned stride() const;

	const T *data() const;
	const T *data(unsigned x, unsigned y) const;

	unsigned size() const;
	unsigned row_size() const;

	bool rows_are_contiguous() const;

	// Read in raster order
	T read(unsigned x, unsigned y) const;

private:
	const uint8_t *mapped_data_;
	unsigned mapped_size_ = 0;
	unsigned mapped_stride_ = 0;

	const Surface &surface_;
};

//// SurfaceBuilder
//
// Surface building object - templated on underlying data type
//
template <typename T> class SurfaceBuilder {
public:
	SurfaceBuilder();

	// From a view into another surface
	SurfaceBuilder &contents(const SurfaceView<T> &view, unsigned x, unsigned y, unsigned width, unsigned height);

	// From existing data
	SurfaceBuilder &contents(const T *data, unsigned width, unsigned height, unsigned stride = 0);
	SurfaceBuilder &contents(const std::vector<T> data, unsigned width, unsigned height, unsigned stride = 0);

	// From new data
	SurfaceBuilder &reserve(unsigned width, unsigned height, unsigned stride = 0);
	SurfaceBuilder &reserve_bpp(unsigned width, unsigned height, unsigned bpp, unsigned stride = 0);

	// From constant
	SurfaceBuilder &fill(T data, unsigned width, unsigned height);

	// Set metadata
	SurfaceBuilder &name(const std::string &name);

	// Make the surface
	Surface finish();

	unsigned width() const;
	unsigned height() const;

	// Write access to reserved bytes
	T *data() const;
	T *data(unsigned x, unsigned y) const;
	void write(unsigned x, unsigned y, T t);

	unsigned stride() const;

	// Generate new contents using a function of (x,y)
	//
	SurfaceBuilder<T> &generate(unsigned width, unsigned height, std::function<T(unsigned x, unsigned y)> fn);

	template <typename C>
	SurfaceBuilder<T> &generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, const C &c), const C &c);

	template <typename A1>
	SurfaceBuilder<T> &generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, A1 a1), A1 a1);

	template <typename A1, typename A2>
	SurfaceBuilder<T> &generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, A1 a1, A2 a2), A1 a1, A2 a2);

	template <typename A1, typename A2, typename A3>
	SurfaceBuilder<T> &generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, A1 a1, A2 a2, A3 a3), A1 a1, A2 a2,
	                            A3 a3);

	template <typename F> SurfaceBuilder<T> &xgenerate(unsigned width, unsigned height, F fn);

private:
	// Accumulated surface
	std::unique_ptr<Surface> surface_;

	uint8_t *mapped_data_ = nullptr;
	unsigned mapped_size_ = 0;
	unsigned mapped_stride_ = 0;
};

class ImageDescription;
class Surface {
public:
	Surface() = default;
	Surface(const Surface &p) = default;
	Surface &operator=(const Surface &src) = default;

	// Create a new builder
	template <typename T> static SurfaceBuilder<T> build_from() { return SurfaceBuilder<T>(); }

	// Create a new view, with optional constant shift right on coordinates
	template <typename T, unsigned S = 0> SurfaceView<T, S> view_as() const;

	// debugname
	std::string name() const { return name_; }

	// checksum
	uint64_t checksum() const;

	unsigned bpp() const { return bpp_; }

	// dimensions
	unsigned width() const { return width_; }
	unsigned height() const { return height_; }

	unsigned empty() const { return !buffer_; }

	// Debug dumps
	static void set_dump_surfaces(bool b);
	static bool get_dump_surfaces();

	// Append to video as Y8 or Y16
	void dump(const std::string &name) const;

	// Append array of layers/surfaces to video as Y16
	static void dump_layers(const Surface src_surface[], const std::string &name, const unsigned transform_block_size);

	// Append to video as  420P8 YUV with empty chroma planes
	void dump_p420(const std::string &name) const;

	// Append to video as correct ImageFormat YUV concatenating every plane
	void dump_yuv(const std::string &name, Surface (&planes)[3], ImageDescription desc, bool decorate = false) const;

	// Dump single image as Y8 or Y16
	void dump_image(const std::string &name) const;

private:
	Surface(const std::string name, uint64_t checksum, const std::shared_ptr<Buffer> &buffer, unsigned offset, unsigned width,
	        unsigned height, unsigned stride);

	template <typename T, unsigned S> friend class SurfaceView;
	template <typename T> friend class SurfaceBuilder;

	// Metadata
	std::string name_;

	// Lazy checksum
	mutable uint64_t checksum_ = 0;
	mutable bool checksummed_ = false;

	// Underlying shared buffer
	std::shared_ptr<Buffer> buffer_;

	// Bytes for each pel
	unsigned bpp_ = 0;

	// Region within above buffer
	unsigned offset_ = 0;
	unsigned width_ = 0;
	unsigned height_ = 0;
	unsigned stride_ = 0;
};

// Templated definitions
#include "SurfaceImpl.hpp"

} // namespace lctm
