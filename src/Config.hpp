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
// Config.hpp
//
// Global debugging flags
//
#pragma once

// activate heavy trace for testing
#define VERBOSE_TRACE 0
#define VERBOSE_TRACE_PIXEL 0
#define VERBOSE_TRACE_SURFACE 0
#define VERBOSE_TRACE_TEMPORAL 0
#define VERBOSE_TRACE_HUFFMAN 0

// activate bitstream trace for testing
#ifndef BITSTREAM_DEBUG
#define BITSTREAM_DEBUG 0
#endif

// activate extraction of user data for testing
#define USER_DATA_EXTRACTION 0

#define __OPT_MATRIX__
#define __OPT_INPLACE__

// Pretty convinced these have no effect - same code generated each way on GCC and MSVC
#define __OPT_DIVISION__
#define __OPT_MODULO__

// using multiply for zero/sign in InverseQuant is NOT improving performance
// #define __QUANT_MULTIPLY__
// bug: decoder fails
// #define __BITSTREAM_BUFFER__
