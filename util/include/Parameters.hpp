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
// Parameters.hpp
//
#pragma once

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "Diagnostics.hpp"
#include "Misc.hpp"

namespace lctm {

class ParametersBuilder;
class ParameterBase;
class ParameterRef;

// Parameters
//
//
//
class Parameters {
public:
	static ParametersBuilder build();
	const ParameterRef operator[](const std::string &name) const;

private:
	friend class ParametersBuilder;
	typedef std::map<std::string, std::shared_ptr<ParameterBase>> ParametersMap;

	Parameters(const ParametersMap &parameters);

	const ParametersMap parameters_;
};

// Parameters Builder
//
class ParametersBuilder {
public:
	ParametersBuilder &set_json(const std::string &str);
	template <typename T> ParametersBuilder &set(const std::string &name, const T &value);
	Parameters finish();

private:
	Parameters::ParametersMap parameters_;
};

// All parameter types derive from this
//
class ParameterBase {
public:
	ParameterBase(const std::string &name) : name_(name) {}
	virtual ~ParameterBase() = 0;

protected:
	friend ParameterRef;
	const std::string name_;
};

// Per type template derived from ParameterBase
//
template <typename T> class Parameter : public ParameterBase {
public:
	Parameter(const std::string &name, const T &value) : ParameterBase(name), value_(value) {}

private:
	friend ParameterRef;
	T value_;
};

template <typename T> ParametersBuilder &ParametersBuilder::set(const std::string &name, const T &value) {
	Parameters::ParametersMap::iterator i = parameters_.find(name);
	if (i == parameters_.end())
		parameters_.insert(Parameters::ParametersMap::value_type(name, new Parameter<T>(name, value)));
	else {
		i->second = std::shared_ptr<ParameterBase>(new Parameter<T>(name, value));
	}
	return *this;
}

// Adapter type returned by Paramters::operator[] - wraps a ParamterBase pointer.
//
class ParameterRef {
public:
	ParameterRef(const std::string &name, const ParameterBase *base) : name_(name), base_(base) {}

	// Does parameter exist?
	bool empty() const { return base_ == nullptr; }

	// Is parameter of a particular type?
	template <typename T> bool is() const {
		if (!base_)
			return false;

		const Parameter<T> *p = dynamic_cast<const Parameter<T> *>(base_);
		return p != nullptr;
	}

	// Get value as a particular type
	template <typename T> T get(T dflt = T()) const {
		if (!base_)
			return dflt;

		const Parameter<T> *p = dynamic_cast<const Parameter<T> *>(base_);
		if (p) {
			return p->value_;
		} else {
			ERR("Parameter type mismatch: %s", name_.c_str());
			return dflt;
		}
	}

	// Get value as a particular type
	template <typename T> T get(const Parameters &defaults) const {
		if (!base_)
			return defaults[name_].get<T>();

		const Parameter<T> *p = dynamic_cast<const Parameter<T> *>(base_);
		if (p) {
			return p->value_;
		} else {
			ERR("Parameter type mismatch: %s", name_.c_str());
			return defaults[name_].get<T>();
		}
	}

	// Get a string value as an enum
	// Expects 'std::istream & operator >> (std::istream &in, T &v)' to exist via extract<T>()
	//
	template <typename T> T get_enum(T dflt) const {
		if (!base_)
			return dflt;

		const Parameter<std::string> *p = dynamic_cast<const Parameter<std::string> *>(base_);
		if (p) {
			return extract<T>(p->value_, dflt);
		} else {
			ERR("Parameter type mismatch: %s", name_.c_str());
			return dflt;
		}
	}

	template <typename T> T get_enum(const Parameters &defaults) const {
		T dflt = defaults[name_].get_enum<T>(static_cast<T>(0));
		if (!base_)
			return dflt;

		const Parameter<std::string> *p = dynamic_cast<const Parameter<std::string> *>(base_);
		if (p) {
			return extract<T>(p->value_, dflt);
		} else {
			ERR("Parameter type mismatch: %s", name_.c_str());
			return dflt;
		}
	}

	// Get a string value as a vector
	// Expects 'std::istream & operator >> (std::istream &in, T &v)' to exist via extract<T>()

	template <typename T> T *get_vector(T *dest, unsigned size) const {
		if (!base_)
			return dest;

		const Parameter<std::string> *p = dynamic_cast<const Parameter<std::string> *>(base_);
		if (p) {
			std::istringstream ss(p->value_);
			T r;
			for (unsigned i = 0; i < size; i++) {
				ss >> r;
				dest[i] = r;
			}
			return dest;
		} else {
			ERR("Parameter type mismatch: %s", name_.c_str());
			return dest;
		}
	}

private:
	const std::string name_;
	const ParameterBase *const base_;
};

// Specialization for converting double->unsigned and double->int
//
// A bit sketchy - shuould really have some sort of schema
//
template <> inline unsigned ParameterRef::get(unsigned dflt) const {
	if (is<unsigned>()) {
		// fix assignment
		const Parameter<unsigned> *p = dynamic_cast<const Parameter<unsigned> *>(base_);
		return (unsigned)(p->value_);
	}

	if (is<int>()) {
		// fix assignment
		const Parameter<int> *p = dynamic_cast<const Parameter<int> *>(base_);
		return (unsigned)(p->value_);
	}

	if (is<double>()) {
		// fix assignment
		const Parameter<double> *p = dynamic_cast<const Parameter<double> *>(base_);
		return (unsigned)(double)(p->value_);
	}

	return dflt;
}

template <> inline int ParameterRef::get(int dflt) const {
	if (is<int>())
		return get<int>(dflt);

	if (is<unsigned>())
		return get<unsigned>(dflt);

	if (is<double>())
		return (int)get<double>(dflt);

	return dflt;
}

template <> inline unsigned ParameterRef::get<unsigned>(const Parameters &defaults) const {
	if (!base_)
		return defaults[name_].get<unsigned>();

	if (is<unsigned>()) {
		// fix assignment
		const Parameter<unsigned> *p = dynamic_cast<const Parameter<unsigned> *>(base_);
		return (unsigned)(p->value_);
	}

	if (is<int>()) {
		// fix assignment
		const Parameter<int> *p = dynamic_cast<const Parameter<int> *>(base_);
		return (unsigned)(p->value_);
	}

	if (is<double>()) {
		// fix assignment
		const Parameter<double> *p = dynamic_cast<const Parameter<double> *>(base_);
		return (unsigned)(double)(p->value_);
	}

	return defaults[name_].get<unsigned>();
}

template <> inline int ParameterRef::get<int>(const Parameters &defaults) const {
	if (!base_)
		return defaults[name_].get<int>();

	if (is<unsigned>())
		return get<unsigned>(0);

	if (is<int>())
		return get<int>(0);

	if (is<double>())
		return (int)get<double>(0);

	return defaults[name_].get<int>();
}

} // namespace lctm
