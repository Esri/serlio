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

#include "maya/MPlugArray.h"

#include <iterator>
#include <type_traits>

template <bool IsConst>
class MPlugArrayWrapperItBase {

public:
	typedef std::input_iterator_tag iterator_category;
	typedef std::conditional_t<IsConst, const MPlug, MPlug> value_type;
	typedef void difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;

	typedef std::conditional_t<IsConst, const MPlugArray, MPlugArray> array_type;

	MPlugArrayWrapperItBase(array_type& plugArray, unsigned int currentIdx = 0) noexcept
		: plugArray(&plugArray),
		  currentIdx(currentIdx) {}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<IsConstLazy>>
	MPlugArrayWrapperItBase(const MPlugArrayWrapperItBase<false>& other)
		: plugArray(other.plugArray),
		  currentIdx(other.currentIdx){};

	MPlugArrayWrapperItBase& operator++() noexcept {
		currentIdx++;
		return *this;
	}

	MPlugArrayWrapperItBase operator++(int) noexcept {
		MPlugArrayWrapperItBase tmp(*this);
		++(*this);
		return tmp;
	}

	bool operator!=(MPlugArrayWrapperItBase& other) const noexcept {
		return currentIdx != other.currentIdx;
	}

	reference operator*() const {
		return (*plugArray)[currentIdx];
	}

	pointer operator->() const {
		return &(*plugArray)[currentIdx];
	}

private:
	array_type* plugArray;
	unsigned int currentIdx;

	friend class MPlugArrayWrapperItBase<!IsConst>;
};

typedef MPlugArrayWrapperItBase<false> MPlugArrayWrapperIt;
typedef MPlugArrayWrapperItBase<true> MPlugArrayWrapperConstIt;

template <bool IsConst>
class MPlugArrayWrapperBase {

public:
	typedef typename MPlugArrayWrapperItBase<IsConst>::array_type array_type;

	MPlugArrayWrapperBase(array_type& plugArray)
		: plugArray(plugArray),
		  arrayLength(plugArray.length()) {}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<!IsConstLazy>>
	MPlugArrayWrapperIt begin() noexcept {
		return MPlugArrayWrapperIt(plugArray);
	}

	MPlugArrayWrapperConstIt begin() const noexcept {
		return MPlugArrayWrapperConstIt(plugArray);
	}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<!IsConstLazy>>
	MPlugArrayWrapperIt end() noexcept {
		return MPlugArrayWrapperIt(plugArray, arrayLength);
	}

	MPlugArrayWrapperConstIt end() const noexcept {
		return MPlugArrayWrapperConstIt(plugArray, arrayLength);
	}

private:
	array_type& plugArray;
	unsigned int arrayLength;
};

typedef MPlugArrayWrapperBase<false> MPlugArrayWrapper;
typedef MPlugArrayWrapperBase<true> MPlugArrayConstWrapper;

// Starting with Maya 2019 API, MPlugArray provides STL compliant iterators.

// The helper functions makePlugArrayWrapper and makePlugArrayConstWrapper
// detect this here and only return a wrapped array if needed. Callers are
// expected to let the return types of these functions be deduced, e.g. using
// auto. The whole wrapper can be removed, once Maya 2018 API is not supported
// anymore.

template <typename T>
constexpr auto isIterable() -> decltype(std::declval<T>().begin(), std::declval<T>().end(), bool()) {
	return true;
}

template <typename T, typename... U>
constexpr auto isIterable(U...) {
	return false;
}

template <typename IsConstLazy = MPlugArray, typename = std::enable_if_t<isIterable<IsConstLazy>()>>
MPlugArray& makePlugArrayWrapper(MPlugArray& plugArray) {
	return plugArray;
}

template <typename IsConstLazy = MPlugArray, typename = std::enable_if_t<!isIterable<IsConstLazy>()>>
MPlugArrayWrapper makePlugArrayWrapper(MPlugArray& plugArray) {
	return MPlugArrayWrapper(plugArray);
}

template <typename IsConstLazy = MPlugArray, typename = std::enable_if_t<isIterable<IsConstLazy>()>>
const MPlugArray& makePlugArrayConstWrapper(const MPlugArray& plugArray) {
	return plugArray;
}

template <typename IsConstLazy = MPlugArray, typename = std::enable_if_t<!isIterable<IsConstLazy>()>>
MPlugArrayConstWrapper makePlugArrayConstWrapper(const MPlugArray& plugArray) {
	return MPlugArrayConstWrapper(plugArray);
}
