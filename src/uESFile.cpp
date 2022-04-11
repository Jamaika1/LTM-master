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

#include "uESFile.h"
#include "Diagnostics.hpp"

namespace vnova {
namespace utility {

static const uint8_t kNalUnitMarkerSize = 3;
static const uint8_t kNalUnitMarker[kNalUnitMarkerSize] = {0x0, 0x0, 0x1};

	ESFile::ESFile() : m_file(nullptr), m_type(BaseDecoder::None), m_decoder(nullptr),
					   m_poc_highest(0), m_poc_offset(0) {
	mpucBuffer = (unsigned char *)malloc(BITSTREAM_BUFFER_SIZE);
}

ESFile::~ESFile() {
	free(mpucBuffer);
	Close();
}

bool ESFile::Open(const std::string &path, BaseDecoder::Codec type) {
	if (IsOpen())
		Close();

	if ((m_file = fopen(path.c_str(), "rb")) == nullptr)
		return false;

	m_type = type;

	return Reset();
}

bool ESFile::Reset() {
	m_decoder = CreateBaseDecoder(m_type);
	CHECK(!!m_decoder);

	return fseek(m_file, 0, SEEK_SET) == 0;
}

void ESFile::Close() {
	if (m_file != nullptr) {
		fclose(m_file);
		m_file = nullptr;
	}

	m_type = BaseDecoder::None;
}

bool ESFile::IsOpen() const { return m_file != nullptr; }

bool ESFile::IsEof() const { return feof(m_file) == 0; }

BaseDecoder::Codec ESFile::GetType() const { return m_type; }

uint32_t ESFile::GetPictureWidth() const {
	assert(m_decoder);

	return m_decoder->GetPictureWidth();
}

uint32_t ESFile::GetPictureHeight() const {
	assert(m_decoder);

	return m_decoder->GetPictureHeight();
}

uint32_t ESFile::GetBitDepth() const {
	assert(m_decoder);

	CHECK(m_decoder->GetBitDepthLuma() == m_decoder->GetBitDepthLuma());

	return m_decoder->GetBitDepthLuma();
}

uint32_t ESFile::GetChromaFormatIDC() const {
	assert(m_decoder);

	return m_decoder->GetChromaFormatIDC();
}

ESFile::Result ESFile::NextAccessUnit(AccessUnit &out) {
	Result result;

	result = ReadAccessUnit(out);

	if (result != Success)
		return result;

	return Success;
}

ESFile::Result ESFile::ReadAccessUnit(AccessUnit &out) {
	switch (m_decoder->Delimiter()) {
	case NALDelimiterMarker:
		return ReadAccessUnitMarker(out);

	case NALDelimiterU32Length:
		return ReadAccessUnitU32Length(out);
	default:
		CHECK(0);
		return NoFile;
	}
}

ESFile::Result ESFile::ReadAccessUnitMarker(AccessUnit &out) {
	if (m_file == nullptr)
		return NoFile;

	utility::DataBuffer buffer;
	std::vector<NalUnit> nalUnits;
	int32_t size = 0;
	uint32_t temporalId = 0;

	while (true) {
		const int c = fgetc(m_file);

		if (c != EOF)
			buffer.push_back(c);

		bool foundNalStart =
		    buffer.size() > kNalUnitMarkerSize && std::equal(buffer.end() - kNalUnitMarkerSize, buffer.end(), kNalUnitMarker);

		if (foundNalStart || feof(m_file)) {

			uint64_t nalLength = buffer.size();
			/* uint64_t */ int markerSize = 0;

			if (foundNalStart) {
				markerSize = kNalUnitMarkerSize;

				// Handle [0, 0, 0, 1] case
				if (buffer.size() > kNalUnitMarkerSize && buffer[nalLength - markerSize - 1] == 0x0)
					++markerSize;
			}

			if (nalLength > markerSize) {
				nalLength -= markerSize;

				unsigned result = m_decoder->ParseNalUnit(buffer.data(), static_cast<uint32_t>(nalLength));

				if (result == 1) {
					NalUnit unit;

					auto nalEnd = buffer.end() - markerSize;
					unit.m_type = m_decoder->GetNalType();
					temporalId = std::max(temporalId, m_decoder->GetTemporalId());

					// Move the next nal unit's marker into unit's buffer so we can just swap the two
					unit.m_data.insert(unit.m_data.end(), nalEnd, buffer.end());
					buffer.erase(nalEnd, buffer.end());
					std::swap(unit.m_data, buffer);

					nalUnits.push_back(std::move(unit));
					size += (int32_t)(nalLength);

					if (m_decoder->GetBaseNalUnitType() == BaseDecNalUnitType::Slice) {

						out.m_pictureType = m_decoder->GetBasePictureType();
						out.m_poc = GenerateIncreasingPOC();
						out.m_qp = m_decoder->GetQP();
						out.m_temporalId = temporalId;
						out.m_nalUnits = std::move(nalUnits);

						out.m_size = size;

						fseek(m_file, -static_cast<long>(markerSize), SEEK_CUR);

						return Success;
					}
				} else if (result == 2) {
					NalUnit unit;

					auto nalEnd = buffer.end() - markerSize;

					// Move the next nal unit's marker into unit's buffer so we can just swap the two
					unit.m_data.insert(unit.m_data.end(), nalEnd, buffer.end());
					buffer.erase(nalEnd, buffer.end());
					std::swap(unit.m_data, buffer);
					nalUnits.push_back(std::move(unit));
				} else
					return NalParsingError;
			}
		}

		if (c == EOF)
			break;
	}

	return EndOfFile;
}

ESFile::Result ESFile::ReadAccessUnitU32Length(AccessUnit &out) {
	if (m_file == nullptr)
		return NoFile;

	utility::DataBuffer buffer;
	std::vector<NalUnit> nalUnits;
	int32_t size = 0;

	while (true) {
		// Read length prefix
		uint32_t nal_length = 0;
		{
			const size_t result = fread(&nal_length, 1, sizeof(nal_length), m_file);
			if (result == 0)
				return EndOfFile;

			if (result != sizeof(nal_length))
				return NalParsingError;
		}

		// Read body of NAL unit
		buffer.resize(nal_length);
		if (fread(buffer.data(), 1, nal_length, m_file) != nal_length)
			return NalParsingError;

		if (m_decoder->ParseNalUnit(buffer.data(), nal_length)) {
			NalUnit unit;
			unit.m_type = m_decoder->GetNalType();
			unit.m_data.insert(unit.m_data.end(), (uint8_t *)&nal_length, (uint8_t *)&nal_length + sizeof(nal_length));
			unit.m_data.insert(unit.m_data.end(), buffer.begin(), buffer.end());
			nalUnits.push_back(std::move(unit));
			size += (int32_t)(nal_length + 4); // length + length prefix

			if (m_decoder->GetBaseNalUnitType() == BaseDecNalUnitType::Slice) {
				out.m_pictureType = m_decoder->GetBasePictureType();
				out.m_poc = GenerateIncreasingPOC();
				out.m_qp = m_decoder->GetQP();
				out.m_nalUnits = std::move(nalUnits);

				out.m_size = size;

				return Success;
			}

		} else {
			return NalParsingError;
		}
	}
}

// Create a POC that always increases across IDR
//
uint64_t ESFile::GenerateIncreasingPOC() {
	uint64_t decoded_poc = m_decoder->GetPictureOrderCount();

	// If we see the start of an IDR and the POC goes backwards, then
	// offset the generated POC by the highest POC seen so far
	//
	if(m_decoder->GetBasePictureType() == BaseDecPictType::IDR && decoded_poc < m_poc_highest) {
		m_poc_offset = m_poc_highest;
	}

	const uint64_t poc = decoded_poc + m_poc_offset;

	if(poc > m_poc_highest) {
		m_poc_highest = poc + m_decoder->GetPictureOrderCountIncrement();
	}

	return poc;
}

} // namespace utility
} // namespace vnova
