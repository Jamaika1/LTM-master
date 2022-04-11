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

#include "Config.hpp"
#include "uPlatform.h"
#include "uBaseDecoder.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#define BITSTREAM_BUFFER_SIZE (1024 * 1024)

namespace vnova { namespace utility {

class ESFile
{
public:
	enum Result
	{
		Success				=  0,
		NalParsingError		= -1,
		EndOfFile			= -2,
		NoFile				= -3,
	};

	inline static const char* ToString(Result result)
	{
		switch(result)
		{
			case Success:			return "Success";
			case NalParsingError:	return "NAL parsing error";
			case EndOfFile:			return "End of file";
			case NoFile:			return "No file";
			default:				return "(unrecognized error)";
		}
	}

	struct NalUnit
	{
		uint32_t			m_type;
		utility::DataBuffer	m_data;
	};

	struct AccessUnit
	{
		int64_t				  m_poc =0;
		int32_t				  m_qp =0;
		BaseDecPictType::Enum m_pictureType;
		uint32_t              m_temporalId =0;
		std::vector<NalUnit>  m_nalUnits;
		int32_t m_size =0;
	};

	ESFile();
	~ESFile();

	bool Open(const std::string& path, BaseDecoder::Codec type);
	bool Reset();
	void Close();

	bool IsOpen() const;
	bool IsEof() const;
	BaseDecoder::Codec GetType() const;

	Result NextAccessUnit(AccessUnit& out);

	// Query information gathered from SPS
	uint32_t GetPictureWidth() const;
	uint32_t GetPictureHeight() const;
	uint32_t GetBitDepth() const;
	uint32_t GetChromaFormatIDC() const;
	uint32_t GetTemporalId() const;

private:
	Result ReadAccessUnit(AccessUnit& out);
	Result ReadAccessUnitMarker(AccessUnit &out);
	Result ReadAccessUnitU32Length(AccessUnit &out);

	// Create a POC that always increases across IDR 
	uint64_t GenerateIncreasingPOC();

	FILE*	m_file;
	BaseDecoder::Codec	m_type;
	std::unique_ptr<utility::BaseDecoder> m_decoder;

	// Highest POC sen so far in stream
	int64_t m_poc_highest;
	// Offset for POCs from decoder to keep them increasing across IDRs
	int64_t m_poc_offset;

	unsigned char * mpucBuffer;
	int miBufferFullness;
	int miBufferPointer;
};

}} // namespace vnova::utility

