/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2019 Esri R&D Center Zurich
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "maya/MObject.h"

#include <iterator>
#include <type_traits>

class MItDependencyNodesWrapperIt {

public:
	typedef std::input_iterator_tag iterator_category;
	typedef const MObject value_type;
	typedef void difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;

	MItDependencyNodesWrapperIt() noexcept : itDepNodes(nullptr) {}

	explicit MItDependencyNodesWrapperIt(MItDependencyNodes& itDepNodes);

	MItDependencyNodesWrapperIt& operator++();

	MItDependencyNodesWrapperIt operator++(int) {
		MItDependencyNodesWrapperIt tmp(*this);
		++(*this);
		return tmp;
	}

	bool operator!=(MItDependencyNodesWrapperIt& other) const noexcept {
		return itDepNodes != other.itDepNodes;
	}

	reference operator*() const noexcept {
		return curObject;
	}

	pointer operator->() const noexcept {
		return &curObject;
	}

private:
	void updateCurrentObject();

	MItDependencyNodes* itDepNodes;
	MObject curObject;
};

class MItDependencyNodesWrapper {

public:
	explicit MItDependencyNodesWrapper(MItDependencyNodes& itDepNodes) noexcept : itDepNodes(itDepNodes) {}

	// don't allow copy or move, as this is only a thin wrapper around MItDependencyNodes
	// and therefore should be recreated instead if needed
	MItDependencyNodesWrapper(const MItDependencyNodesWrapper&) = delete;
	MItDependencyNodesWrapper& operator=(const MItDependencyNodesWrapper&) = delete;

	MItDependencyNodesWrapper(MItDependencyNodesWrapper&&) = delete;
	MItDependencyNodesWrapper& operator=(MItDependencyNodesWrapper&&) = delete;

	MItDependencyNodesWrapperIt begin() const {
		return MItDependencyNodesWrapperIt(itDepNodes);
	}

	MItDependencyNodesWrapperIt end() const noexcept {
		return MItDependencyNodesWrapperIt();
	}

private:
	MItDependencyNodes& itDepNodes;
};
