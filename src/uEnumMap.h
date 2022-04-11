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
#include "uString.h"
#include "uTypeTraits.h"
#include <vector>

namespace vnova { namespace utility
{
	template<typename E>
	class EnumMap
	{
		VNStaticAssert(IsEnum<E>::Value);

	public:
		EnumMap(E value, const char* name)
		{
			m_pairs.push_back(EnumStringPair(value, name));
		}

		EnumMap& operator()(E value, const char* name)
		{
			m_pairs.push_back(EnumStringPair(value, name));
			return *this;
		}

		bool FindEnum(E& res, const std::string& name, E failedReturn) const
		{
			for (typename EnumStringVec::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it)
			{
				if(string::IEquals((*it).m_name, name))
				{
					res = (*it).m_value;
					return true;
				}
			}

			res = failedReturn;
			return false;
		}

		bool FindName(const char** res, E value, const char* failedReturn) const
		{
			for (typename EnumStringVec::const_iterator it = m_pairs.begin(); it != m_pairs.end(); ++it)
			{
				if((*it).m_value == value)
				{
					*res = (*it).m_name.c_str();
					return true;
				}
			}

			*res = failedReturn;
			return false;
		}

	private:
		struct EnumStringPair
		{
			EnumStringPair(E value, const char* name) : m_value(value), m_name(name) {}
			E					m_value;
			const std::string	m_name;
		};

		typedef std::vector<EnumStringPair> EnumStringVec;

		EnumStringVec m_pairs;
	};

	template<typename T>
	static const char* ToString2Helper(bool (*ToStringFn)(const char**, T), T val)
	{
		const char* res;
		ToStringFn(&res, val);
		return res;
	}

	template<typename T>
	static T FromString2Helper(bool (*FromStringFn)(T&, const std::string&), const std::string val)
	{
		T res;
		FromStringFn(res, val);
		return res;
	}
}}
