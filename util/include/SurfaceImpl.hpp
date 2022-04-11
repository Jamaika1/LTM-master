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
// SurfaceImpl.hpp
//
#pragma once

// Template definitions for SurfaceBuilder<T> amd SurfaceView<T>

template <typename T> SurfaceBuilder<T>::SurfaceBuilder() : surface_(new Surface("", 0, 0, 0, 0, 0, 0)) {}

// From a view into another surface
template <typename T>
SurfaceBuilder<T> &SurfaceBuilder<T>::contents(const SurfaceView<T> &view, unsigned x, unsigned y, unsigned width,
                                               unsigned height) {

	// Pixels must be same size (but can be reinterpreted as different similar sized types)
	assert(view.surface_.bpp_ == sizeof(T));

	*surface_ = view.surface_;

	assert(x <= surface_->width());
	assert(x + width <= surface_->width());
	assert(y <= surface_->height());
	assert(y + height <= surface_->height());

	surface_->offset_ += (y * surface_->stride_) + x * surface_->bpp_;
	surface_->width_ = width;
	surface_->height_ = height;

	return *this;
}

template <typename T>
SurfaceBuilder<T> &SurfaceBuilder<T>::contents(const T *data, unsigned width, unsigned height, unsigned data_stride) {
	surface_->bpp_ = sizeof(T);
	surface_->stride_ = width * sizeof(T);
	surface_->buffer_ = std::shared_ptr<Buffer>(CreateBufferAligned(height * surface_->stride_));
	surface_->offset_ = 0;
	surface_->width_ = width;
	surface_->height_ = height;

	uint8_t *mapped_data = nullptr;
	unsigned mapped_size = 0;

	surface_->buffer_->map_write(0, height * surface_->stride_, mapped_data, mapped_size);

	if (data_stride == 0 || data_stride == surface_->stride_) {
		// Contiguous - use a single memcpy
		CHECK(mapped_size >= height * surface_->stride_);
		memcpy(mapped_data, data, height * surface_->stride_);
	} else {
		// Strided - copy line by line
		const uint8_t *src = (const uint8_t *)data;
		uint8_t *dest = mapped_data;

		for (unsigned y = 0; y < height; ++y) {
			memcpy(dest, src, width * surface_->bpp_);
			dest += surface_->stride_;
			src += data_stride;
		}
	}

	surface_->buffer_->unmap();
	return *this;
}

template <typename T> SurfaceBuilder<T> &SurfaceBuilder<T>::reserve(unsigned width, unsigned height, unsigned stride) {
	surface_->bpp_ = sizeof(T);
	surface_->stride_ = stride ? stride : (width * sizeof(T));
	surface_->buffer_ = std::shared_ptr<Buffer>(CreateBufferAligned(height * surface_->stride_));
	surface_->offset_ = 0;
	surface_->width_ = width;
	surface_->height_ = height;

	surface_->buffer_->map_write(surface_->offset_, height * surface_->stride_, mapped_data_, mapped_size_);
	mapped_stride_ = surface_->stride_;
	return *this;
}

template <typename T>
SurfaceBuilder<T> &SurfaceBuilder<T>::reserve_bpp(unsigned width, unsigned height, unsigned bpp, unsigned stride) {
	surface_->bpp_ = bpp;
	surface_->stride_ = stride ? stride : (width * bpp);
	surface_->buffer_ = std::shared_ptr<Buffer>(CreateBufferAligned(height * surface_->stride_));
	surface_->offset_ = 0;
	surface_->width_ = width;
	surface_->height_ = height;

	surface_->buffer_->map_write(surface_->offset_, height * surface_->stride_, mapped_data_, mapped_size_);
	mapped_stride_ = surface_->stride_;
	return *this;
}

template <typename T> T *SurfaceBuilder<T>::data() const {
	assert(mapped_data_ != nullptr);
	return reinterpret_cast<T *>(mapped_data_);
}

template <typename T> T *SurfaceBuilder<T>::data(unsigned x, unsigned y) const {
	assert(mapped_data_ != nullptr);
	assert(x < width() && y < height());
	return reinterpret_cast<T *>(mapped_data_ + y * mapped_stride_) + x;
}

template <typename T> void SurfaceBuilder<T>::write(unsigned x, unsigned y, T t) { *data(x, y) = t; }

template <typename T> unsigned SurfaceBuilder<T>::width() const { return surface_->width_; }

template <typename T> unsigned SurfaceBuilder<T>::height() const { return surface_->height_; }

template <typename T> unsigned SurfaceBuilder<T>::stride() const {
	assert(mapped_data_ != nullptr);

	return mapped_stride_;
}

template <typename T> SurfaceBuilder<T> &SurfaceBuilder<T>::name(const std::string &name) {
	surface_->name_ = name;
	return *this;
}

template <typename T> Surface SurfaceBuilder<T>::finish() {
	if (mapped_data_) {
		surface_->buffer_->unmap();
		mapped_data_ = nullptr;
		mapped_stride_ = 0;
	}
	return *surface_;
}

// Generate new contents using a function of (x,y)
//
template <typename T>
SurfaceBuilder<T> &SurfaceBuilder<T>::generate(unsigned width, unsigned height, std::function<T(unsigned x, unsigned y)> fn) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y));

	return *this;
}

template <typename T>
template <typename C>
SurfaceBuilder<T> &SurfaceBuilder<T>::generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, const C &c),
                                               const C &c) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y, c));

	return *this;
}

template <typename T>
template <typename A1>
SurfaceBuilder<T> &SurfaceBuilder<T>::generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, A1 a1), A1 a1) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y, a1));

	return *this;
}

template <typename T>
template <typename A1, typename A2>
SurfaceBuilder<T> &SurfaceBuilder<T>::generate(unsigned width, unsigned height, T (*fn)(unsigned x, unsigned y, A1 a1, A2 a2),
                                               A1 a1, A2 a2) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y, a1, a2));

	return *this;
}

template <typename T>
template <typename A1, typename A2, typename A3>
SurfaceBuilder<T> &SurfaceBuilder<T>::generate(unsigned width, unsigned height,
                                               T (*fn)(unsigned x, unsigned y, A1 a1, A2 a2, A3 a3), A1 a1, A2 a2, A3 a3) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y, a1, a2, a3));

	return *this;
}

template <typename T> SurfaceBuilder<T> &SurfaceBuilder<T>::fill(T value, unsigned width, unsigned height) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, value);

	return *this;
}

template <typename T> template <typename F> SurfaceBuilder<T> &SurfaceBuilder<T>::xgenerate(unsigned width, unsigned height, F fn) {
	reserve(width, height);
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x)
			write(x, y, fn(x, y));

	return *this;
}

//// SurfaceView
//
//
template <typename T, unsigned S> SurfaceView<T, S>::SurfaceView(const Surface &surface) : surface_(surface) {
	surface.buffer_->map_read(surface.offset_, surface.height_ * surface_.stride_, mapped_data_, mapped_size_);
	mapped_stride_ = surface_.stride_;
}

template <typename T, unsigned S> SurfaceView<T, S>::~SurfaceView() { surface_.buffer_->unmap(); }

template <typename T, unsigned S> const T *SurfaceView<T, S>::data() const { return reinterpret_cast<const T *>(mapped_data_); }

template <typename T, unsigned S> const T *SurfaceView<T, S>::data(unsigned x, unsigned y) const {
	assert((x >> S) < width() && (y >> S) < height());
	return reinterpret_cast<const T *>(mapped_data_ + (y >> S) * mapped_stride_) + (x >> S);
}

template <typename T, unsigned S> T SurfaceView<T, S>::read(unsigned x, unsigned y) const { return *data(x, y); }

template <typename T, unsigned S> unsigned SurfaceView<T, S>::width() const { return surface_.width_; }

template <typename T, unsigned S> unsigned SurfaceView<T, S>::height() const { return surface_.height_; }

template <typename T, unsigned S> unsigned SurfaceView<T, S>::size() const { return surface_.height_ * surface_.stride_; };

template <typename T, unsigned S> unsigned SurfaceView<T, S>::row_size() const { return surface_.width_ * surface_.bpp_; };

template <typename T, unsigned S> bool SurfaceView<T, S>::rows_are_contiguous() const {
	return mapped_stride_ == surface_.width_ * surface_.bpp_;
}

// Surface
//
template <typename T, unsigned S> SurfaceView<T, S> Surface::view_as() const { return SurfaceView<T, S>(*this); }
