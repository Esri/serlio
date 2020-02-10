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

#include "utils/MArrayIteratorTraits.h"

#include <iterator>
#include <type_traits>

namespace mu {

namespace detail {

// remove in favour of std::is_const_v once we switch to C++17
template <typename T>
constexpr bool is_const_v = std::is_const<T>::value;

// remove in favour of std::is_reference_v once we switch to C++17
template <typename T>
constexpr bool is_reference_v = std::is_reference<T>::value;

// remove in favour of std::bool_constant once we switch to C++17
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

template <typename ElementType>
class MArrayWrapperItProxy {

public:
	explicit MArrayWrapperItProxy(std::add_const_t<ElementType>& element) : element(element) {}

	explicit operator ElementType() const {
		return element;
	}

	ElementType& operator*() const {
		return element;
	}

	ElementType* operator->() const {
		return &element;
	}

private:
	ElementType element;
};

template <typename ArrayType>
class MArrayWrapperItBase {

public:
	static constexpr bool IsConst = is_const_v<ArrayType>;

	typedef decltype(std::declval<ArrayType>()[0]) ElementType;
	typedef std::remove_const_t<std::remove_reference_t<ElementType>> BaseElementType;
	static constexpr bool ElementTypeIsRef = is_reference_v<ElementType>;

	typedef std::input_iterator_tag iterator_category;
	typedef std::conditional_t<IsConst, std::add_const_t<BaseElementType>, BaseElementType> value_type;
	typedef void difference_type;
	typedef std::conditional_t<ElementTypeIsRef, value_type*, MArrayWrapperItProxy<value_type>> pointer;
	typedef std::conditional_t<ElementTypeIsRef, value_type&, value_type> reference;

	explicit MArrayWrapperItBase(ArrayType& array, unsigned int currentIdx = 0) noexcept
	    : array(&array), currentIdx(currentIdx) {}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<IsConstLazy>>
	explicit MArrayWrapperItBase(const MArrayWrapperItBase<std::remove_cv_t<ArrayType>>& other)
	    : array(other.array), currentIdx(other.currentIdx) {}

	MArrayWrapperItBase& operator++() noexcept {
		currentIdx++;
		return *this;
	}

	MArrayWrapperItBase operator++(int) noexcept {
		MArrayWrapperItBase tmp(*this);
		++(*this);
		return tmp;
	}

	bool operator!=(MArrayWrapperItBase& other) const noexcept {
		return currentIdx != other.currentIdx;
	}

	reference operator*() const {
		return (*array)[currentIdx];
	}

	pointer operator->() const {
		return arrowOpImpl(bool_constant<!ElementTypeIsRef>{});
	}

private:
	pointer arrowOpImpl(bool_constant<false>) const {
		return &(*array)[currentIdx];
	}

	pointer arrowOpImpl(bool_constant<true>) const {
		return MArrayWrapperItProxy<value_type>((*array)[currentIdx]);
	}

	ArrayType* array;
	unsigned int currentIdx;

	friend class MArrayWrapperItBase<
	        std::conditional_t<IsConst, std::remove_const_t<ArrayType>, std::add_const_t<ArrayType>>>;
};

template <typename ArrayType>
using MArrayWrapperIt = MArrayWrapperItBase<ArrayType>;

template <typename ArrayType>
using MArrayWrapperConstIt = MArrayWrapperItBase<const ArrayType>;

template <typename ArrayType>
class MArrayWrapperBase {

public:
	static constexpr bool IsConst = is_const_v<ArrayType>;

	explicit MArrayWrapperBase(ArrayType& array) : array(array), arrayLength(array.length()) {}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<!IsConstLazy>>
	MArrayWrapperIt<ArrayType> begin() noexcept {
		return MArrayWrapperIt<ArrayType>(array);
	}

	MArrayWrapperConstIt<ArrayType> begin() const noexcept {
		return MArrayWrapperConstIt<ArrayType>(array);
	}

	template <bool IsConstLazy = IsConst, typename = std::enable_if_t<!IsConstLazy>>
	MArrayWrapperIt<ArrayType> end() noexcept {
		return MArrayWrapperIt<ArrayType>(array, arrayLength);
	}

	MArrayWrapperConstIt<ArrayType> end() const noexcept {
		return MArrayWrapperConstIt<ArrayType>(array, arrayLength);
	}

private:
	ArrayType& array;
	unsigned int arrayLength;
};

template <typename T>
constexpr auto isIterable() -> decltype(std::declval<T>().begin(), std::declval<T>().end(), bool()) {
	return true;
}

template <typename T, typename... U>
constexpr auto isIterable(U...) {
	return false;
}

} // namespace detail

template <typename ArrayType>
class MArraySimpleWrapper {

public:
	explicit MArraySimpleWrapper(ArrayType& array) : array(array){};

	auto begin() {
		return array.begin();
	}

	auto begin() const {
		return array.begin();
	}

	auto end() {
		return array.end();
	}

	auto end() const {
		return array.end();
	}

private:
	ArrayType& array;
};

template <typename ArrayType>
using MArrayWrapper = detail::MArrayWrapperBase<ArrayType>;

template <typename ArrayType>
using MArrayConstWrapper = detail::MArrayWrapperBase<const ArrayType>;

// Starting with Maya 2019 API, various array types such as MFloatPointArray, MIntArray and MPlugArray provide STL
// compliant iterators.

// The helper functions makeMArrayWrapper and makeMArrayConstWrapper detect this here and only return a wrapped array if
// needed. Callers are expected to let the return types of these functions be deduced, e.g. using auto. The whole
// wrapper can be removed, once Maya 2018 API is not supported anymore.

template <typename ArrayType, typename = std::enable_if_t<detail::isIterable<ArrayType>()>>
MArraySimpleWrapper<ArrayType> makeMArrayWrapper(ArrayType& array) {
	return MArraySimpleWrapper<ArrayType>(array);
}

template <typename ArrayType, typename = std::enable_if_t<!detail::isIterable<ArrayType>()>>
MArrayWrapper<ArrayType> makeMArrayWrapper(ArrayType& array) {
	return MArrayWrapper<ArrayType>(array);
}

template <typename ArrayType, typename = std::enable_if_t<detail::isIterable<ArrayType>()>>
MArraySimpleWrapper<const ArrayType> makeMArrayConstWrapper(const ArrayType& array) {
	return MArraySimpleWrapper<const ArrayType>(array);
}

template <typename ArrayType, typename = std::enable_if_t<!detail::isIterable<ArrayType>()>>
MArrayConstWrapper<ArrayType> makeMArrayConstWrapper(const ArrayType& array) {
	return MArrayConstWrapper<ArrayType>(array);
}

} // namespace mu
