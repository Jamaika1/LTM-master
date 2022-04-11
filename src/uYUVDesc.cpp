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
#include "uYUVDesc.h"
#include "uEnumMap.h"
#include <assert.h>

namespace vnova {
namespace utility {
// -------------------------------------------------------------------------

namespace {
struct YUVFormatInfo {
	YUVFormatInfo(uint32_t planeCount, uint32_t sizeMult, uint32_t sizeDiv, uint8_t bitDepth, uint32_t horizontalDownsample,
	              uint32_t verticalDownsample)
	    : m_planeCount(planeCount), m_sizeMult(sizeMult), m_sizeDiv(sizeDiv), m_bitDepth(bitDepth),
	      m_horizontalDownsample(horizontalDownsample), m_verticalDownsample(verticalDownsample) {}

	uint32_t m_planeCount;
	uint32_t m_sizeMult;
	uint32_t m_sizeDiv;
	uint8_t m_bitDepth;
	uint32_t m_horizontalDownsample;
	uint32_t m_verticalDownsample;
};

const YUVFormatInfo kFormatInfo[YUVFormat::Count] = {
    YUVFormatInfo(3, 3, 2, 8, 2, 2), // YUV8Planar420
    YUVFormatInfo(3, 2, 1, 8, 2, 1), // YUV8Planar422
    YUVFormatInfo(3, 3, 1, 8, 1, 1), // YUV8Planar444
    YUVFormatInfo(1, 1, 1, 8, 1, 1), // Y8Planar

    YUVFormatInfo(3, 3, 1, 10, 2, 2), // YUV10Planar420
    YUVFormatInfo(3, 4, 1, 10, 2, 1), // YUV10Planar422
    YUVFormatInfo(3, 6, 1, 10, 1, 1), // YUV10Planar444
    YUVFormatInfo(1, 2, 1, 10, 1, 1), // Y10Planar

    YUVFormatInfo(3, 3, 1, 12, 2, 2), // YUV12Planar420
    YUVFormatInfo(3, 4, 1, 12, 2, 1), // YUV12Planar422
    YUVFormatInfo(3, 6, 1, 12, 1, 1), // YUV12Planar444
    YUVFormatInfo(1, 2, 1, 12, 1, 1), // Y12Planar

    YUVFormatInfo(3, 3, 1, 14, 2, 2), // YUV14Planar420
    YUVFormatInfo(3, 4, 1, 14, 2, 1), // YUV14Planar422
    YUVFormatInfo(3, 6, 1, 14, 1, 1), // YUV14Planar444
    YUVFormatInfo(1, 2, 1, 14, 1, 1)  // Y14Planar
};

// clang-format off
static const EnumMap<YUVFormat::Enum> kYUVFormatMap = EnumMap<YUVFormat::Enum>
	(YUVFormat::YUV8Planar420, "yuv420p")
	(YUVFormat::YUV8Planar422, "yuv422p")
	(YUVFormat::YUV8Planar444, "yuv444p")
	(YUVFormat::Y8Planar,      "y8")
	(YUVFormat::YUV10Planar420, "yuv420p10")
	(YUVFormat::YUV10Planar422, "yuv422p10")
	(YUVFormat::YUV10Planar444, "yuv444p10")
	(YUVFormat::Y10Planar,      "y10")
	(YUVFormat::YUV12Planar420, "yuv420p12")
	(YUVFormat::YUV12Planar422, "yuv422p12")
	(YUVFormat::YUV12Planar444, "yuv444p12")
	(YUVFormat::Y12Planar,      "y12")
	(YUVFormat::YUV14Planar420, "yuv420p14")
	(YUVFormat::YUV14Planar422, "yuv422p14")
	(YUVFormat::YUV14Planar444, "yuv444p14")
	(YUVFormat::Y14Planar,      "y14");

static const EnumMap<YUVFormat::Enum> kYUVFormatMapEPI = EnumMap<YUVFormat::Enum>
	(YUVFormat::YUV8Planar420, "yuv8planar420")
	(YUVFormat::YUV8Planar422, "yuv8planar422")
	(YUVFormat::YUV8Planar444, "yuv8planar444")
	(YUVFormat::Y8Planar,      "y8")
	(YUVFormat::YUV10Planar420, "yuv10planar420")
	(YUVFormat::YUV10Planar422, "yuv10planar422")
	(YUVFormat::YUV10Planar444, "yuv10planar444")
	(YUVFormat::Y10Planar,      "y10")
	(YUVFormat::YUV12Planar420, "yuv12planar420")
	(YUVFormat::YUV12Planar422, "yuv12planar422")
	(YUVFormat::YUV12Planar444, "yuv12planar444")
	(YUVFormat::Y12Planar,      "y12")
	(YUVFormat::YUV14Planar420, "yuv14planar420")
	(YUVFormat::YUV14Planar422, "yuv14planar422")
	(YUVFormat::YUV14Planar444, "yuv14planar444")
	(YUVFormat::Y14Planar,      "y14");
// clang-format on

} // namespace

// -------------------------------------------------------------------------

bool YUVFormat::FromString(Enum &res, const std::string &str) { return kYUVFormatMap.FindEnum(res, str, YUVFormat::Invalid); }

YUVFormat::Enum YUVFormat::FromString2(const std::string &str) { return FromString2Helper(FromString, str); }

bool YUVFormat::ToString(const char **res, Enum type) { return kYUVFormatMap.FindName(res, type, "YUVFormat-ERROR"); }

const char *YUVFormat::ToString2(Enum val) { return ToString2Helper(ToString, val); }

// @todo (rob): These functions are currently a necessary evil to translate from the
//				FFmpeg inspired formats to EPI legacy formats. EPI API changes should
//				work to eliminate this.
bool YUVFormat::ToStringEPI(const char **res, Enum type) { return kYUVFormatMapEPI.FindName(res, type, "YUVFormat-ERROR"); }

const char *YUVFormat::ToStringEPI2(Enum val) { return ToString2Helper(ToStringEPI, val); }

// -------------------------------------------------------------------------

YUVDesc::PlaneDesc::PlaneDesc(uint32_t width, uint32_t height, uint32_t stridePixels, uint32_t strideBytes)
    : m_width(width), m_height(height), m_stridePixels(stridePixels), m_strideBytes(strideBytes) {}

// -------------------------------------------------------------------------

YUVDesc::YUVDesc() : m_format(YUVFormat::Invalid), m_planeCount(0), m_byteSize(0), m_bitDepth(0), m_bitDepthContainer(0) {}

void YUVDesc::Initialise(YUVFormat::Enum format, uint32_t width, uint32_t height) {
	const YUVFormatInfo &info = kFormatInfo[format];

	m_format = format;
	m_planeCount = info.m_planeCount;
	assert(m_planeCount <= kMaxNumPlanes);

	m_bitDepth = info.m_bitDepth;
	m_bitDepthContainer = (m_bitDepth + 7) & ~7;

	const uint8_t byteDepth = GetByteDepth();

	for (uint32_t planeIndex = 0; planeIndex < m_planeCount; ++planeIndex) {
		const uint32_t planeWidth =
		    (planeIndex == 0 || (info.m_horizontalDownsample == 1)) ? width : ((width + 1) / info.m_horizontalDownsample);
		const uint32_t planeHeight =
		    (planeIndex == 0 || (info.m_verticalDownsample == 1)) ? height : ((height + 1) / info.m_verticalDownsample);
		m_planeDesc[planeIndex] = PlaneDesc(planeWidth, planeHeight, planeWidth, byteDepth * planeWidth);
	}

	m_byteSize = (width * height * info.m_sizeMult) / info.m_sizeDiv;
}

uint32_t YUVDesc::GetWidth() const { return GetPlaneWidth(0); }

uint32_t YUVDesc::GetHeight() const { return GetPlaneHeight(0); }

uint32_t *YUVDesc::GetWidthPtr() { return GetPlaneWidthPtr(0); }

uint32_t *YUVDesc::GetHeightPtr() { return GetPlaneHeightPtr(0); }

YUVFormat::Enum YUVDesc::GetFormat() const { return m_format; }

uint8_t YUVDesc::GetBitDepth() const { return m_bitDepth; }

uint8_t YUVDesc::GetBitDepthContainer() const { return m_bitDepthContainer; }

uint8_t YUVDesc::GetByteDepth() const { return m_bitDepthContainer >> 3; }

uint32_t YUVDesc::GetMemorySize() const { return m_byteSize; }

uint32_t YUVDesc::GetPlaneCount() const { return m_planeCount; }

uint32_t YUVDesc::GetPlaneWidth(uint32_t planeIndex) const {
	assert(planeIndex < kMaxNumPlanes);
	return m_planeDesc[planeIndex].m_width;
}

uint32_t YUVDesc::GetPlaneHeight(uint32_t planeIndex) const {
	assert(planeIndex < kMaxNumPlanes);
	return m_planeDesc[planeIndex].m_height;
}

uint32_t *YUVDesc::GetPlaneWidthPtr(uint32_t planeIndex) {
	assert(planeIndex < kMaxNumPlanes);
	return &m_planeDesc[planeIndex].m_width;
}

uint32_t *YUVDesc::GetPlaneHeightPtr(uint32_t planeIndex) {
	assert(planeIndex < kMaxNumPlanes);
	return &m_planeDesc[planeIndex].m_height;
}

uint32_t YUVDesc::GetPlaneStrideBytes(uint32_t planeIndex) const {
	assert(planeIndex < kMaxNumPlanes);
	return m_planeDesc[planeIndex].m_strideBytes;
}

uint32_t YUVDesc::GetPlaneStridePixels(uint32_t planeIndex) const {
	assert(planeIndex < kMaxNumPlanes);
	return m_planeDesc[planeIndex].m_stridePixels;
}

uint32_t YUVDesc::GetPlaneMemorySize(uint32_t planeIndex) const {
	assert(planeIndex < kMaxNumPlanes);
	const PlaneDesc &desc = m_planeDesc[planeIndex];
	return (desc.m_strideBytes * desc.m_height);
}

void YUVDesc::GetPlanePointers(uint8_t *memoryPtr, uint8_t **planePtrs, uint32_t *planePixelStrides) const {
	// Assumes that the out arrays are the correct size.
	for (uint32_t planeIndex = 0; planeIndex < m_planeCount; ++planeIndex) {
		planePtrs[planeIndex] = memoryPtr;

		if (planePixelStrides) {
			planePixelStrides[planeIndex] = GetPlaneStridePixels(planeIndex);
		}

		memoryPtr += GetPlaneMemorySize(planeIndex);
	}
}

void YUVDesc::GetPlanePointers(const uint8_t *memoryPtr, const uint8_t **planePtrs, uint32_t *planePixelStrides) const {
	// Assumes that the out arrays are the correct size.
	for (uint32_t planeIndex = 0; planeIndex < m_planeCount; ++planeIndex) {
		planePtrs[planeIndex] = memoryPtr;

		if (planePixelStrides) {
			planePixelStrides[planeIndex] = GetPlaneStridePixels(planeIndex);
		}

		memoryPtr += GetPlaneMemorySize(planeIndex);
	}
}

// -------------------------------------------------------------------------
} // namespace utility
} // namespace vnova
