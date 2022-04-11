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

#include "uPlatform.h"
#include <string>


namespace vnova { namespace utility
{
	struct YUVFormat
	{
		enum Enum
		{
			YUV8Planar420 = 0,
			YUV8Planar422,
			YUV8Planar444,
			Y8Planar,
			YUV10Planar420,
			YUV10Planar422,
			YUV10Planar444,
			Y10Planar,
			YUV12Planar420,
			YUV12Planar422,
			YUV12Planar444,
			Y12Planar,
			YUV14Planar420,
			YUV14Planar422,
			YUV14Planar444,
			Y14Planar,
			Count,
			Invalid
		};

		static bool FromString(Enum& res, const std::string& str);
		static Enum FromString2(const std::string& str);
		static bool ToString(const char** res, Enum type);
		static const char* ToString2(Enum val);

		static bool ToStringEPI(const char** res, Enum type);
		static const char* ToStringEPI2(Enum val);
	};

	class YUVDesc
	{
	public:
		YUVDesc();

		void Initialise(YUVFormat::Enum format = YUVFormat::Count, uint32_t width = 0, uint32_t height = 0);

		uint32_t		GetWidth() const;
		uint32_t		GetHeight() const;
		uint32_t*		GetWidthPtr();
		uint32_t*		GetHeightPtr();
		YUVFormat::Enum GetFormat() const;
		uint8_t			GetBitDepth() const;
		uint8_t			GetBitDepthContainer() const;
		uint8_t			GetByteDepth() const;
		uint32_t		GetMemorySize() const;
		uint32_t		GetPlaneCount() const;
		uint32_t		GetPlaneWidth(uint32_t planeIndex) const;
		uint32_t		GetPlaneHeight(uint32_t planeIndex) const;
		uint32_t*		GetPlaneWidthPtr(uint32_t planeIndex);
		uint32_t*		GetPlaneHeightPtr(uint32_t planeIndex);
		uint32_t		GetPlaneStrideBytes(uint32_t planeIndex) const;
		uint32_t		GetPlaneStridePixels(uint32_t planeIndex) const;
		uint32_t		GetPlaneMemorySize(uint32_t planeIndex) const;
		void			GetPlanePointers(uint8_t* memoryPtr, uint8_t** planePtrs, uint32_t* planePixelStrides) const;
		void			GetPlanePointers(const uint8_t* memoryPtr, const uint8_t** planePtrs, uint32_t* planePixelStrides) const;

		enum
		{
			kMaxNumPlanes = 3
		};

	private:
		struct PlaneDesc
		{
			PlaneDesc(uint32_t width = 0, uint32_t height= 0, uint32_t stridePixels = 0, uint32_t strideBytes = 0);

			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_stridePixels;	//< Means line size in pixels. (For alignment purposes).
			uint32_t m_strideBytes;		//< Means line size in bytes of stride. (stride * channeldepth)
		};

		YUVFormat::Enum		m_format;
		uint32_t			m_planeCount;
		PlaneDesc			m_planeDesc[kMaxNumPlanes];
		uint32_t			m_byteSize;
		uint8_t				m_bitDepth;
		uint8_t				m_bitDepthContainer;
	};
}}
