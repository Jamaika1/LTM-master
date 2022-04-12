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
//

#include "InverseTransformDDS.hpp"
#include "TransformDDS.hpp"

#include "InverseTransformDDS_1D.hpp"
#include "TransformDDS_1D.hpp"

#include "InverseTransformDD.hpp"
#include "TransformDD.hpp"

#include "InverseTransformDD_1D.hpp"
#include "TransformDD_1D.hpp"

#include "Temporal.hpp"

#include "Misc.hpp"
#include "Surface.hpp"
#include "YUVReader.hpp"
#include "YUVWriter.hpp"

#include "Diagnostics.hpp"

using namespace lctm;

int main(int argc, char **argv) {

	// Reference coefficients
	Surface reference_coefficients[16];
	for (unsigned l = 0; l < 16; ++l) {
		auto reader = CreateYUVReader(format("data/coeffs-P0Q1L%d_480x270_16bit.y", l));
		auto img = reader->read(0);
		reference_coefficients[l] = img.plane(0);
	}

	// Reference residuals
	Surface reference_residuals;
	{
		auto reader = CreateYUVReader("data/residuals-P0Q1L0_1920x1080_16bit.y");
		auto img = reader->read(0);
		reference_residuals = img.plane(0);
	}

	// DDS
	{
		// ceoffs -> residuals
		Surface residuals = InverseTransformDDS().process(1920, 1080, reference_coefficients);
		CHECK(residuals.checksum() == reference_residuals.checksum());

		// residuals -> coeffs
		Surface coefficients[16];

		TransformDDS().process(residuals, coefficients);

		for (unsigned l = 0; l < 16; ++l)
			CHECK(coefficients[l].checksum() == reference_coefficients[l].checksum());
	}
}