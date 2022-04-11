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
// Parameters.cpp
//
//
#include "Parameters.hpp"
#include "Diagnostics.hpp"

#include <cassert>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace lctm {

Parameters::Parameters(const ParametersMap &parameters) : parameters_(parameters) {}

ParametersBuilder &ParametersBuilder::set_json(const std::string &json_str) {
	json params;
	try {
		params = json::parse(json_str);
	} catch (const std::exception &e) {
		ERR("Cannot parse parameters: %s", e.what());
	}

	if (!params.is_object()) {
		ERR("Parameters should be a json object.");
		return *this;
	}

	for (const auto &p : params.items()) {
		if (p.value().is_number()) {
			set(p.key(), p.value().get<double>());
			REPORT("parameter %s NUM  %8.3f", p.key().c_str(), p.value().get<double>());
		} else if (p.value().is_boolean()) {
			set(p.key(), p.value().get<bool>());
			REPORT("parameter %s BOOL %8d", p.key().c_str(), p.value().get<bool>());
		} else if (p.value().is_string()) {
			set(p.key(), p.value().get<std::string>());
			REPORT("parameter %s STR  %s", p.key().c_str(), p.value().get<std::string>().c_str());
		} else {
			WARN("Parameter %s should be bool, number, or string", p.key().c_str());
		}
	}

	return *this;
}

ParameterBase::~ParameterBase() {}

ParametersBuilder Parameters::build() { return ParametersBuilder(); }

Parameters ParametersBuilder::finish() { return Parameters(parameters_); }

const ParameterRef Parameters::operator[](const std::string &name) const {
	const auto i = parameters_.find(name);

	if (i == parameters_.end())
		// No paramter
		return ParameterRef(name, nullptr);

	return ParameterRef(name, i->second.get());
}

} // namespace lctm
