/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2022 Esri R&D Center Zurich
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

#include "maya/MTypes.h"

namespace mu {

namespace detail {

// remove in favour of std::void_t once we switch to C++17
template <typename... T>
using void_t = void;

} // namespace detail

} // namespace mu

#if (MAYA_API_VERSION >= 20190000)

#	include "maya/MArrayIteratorTemplate.h"

#	include <iterator>

namespace mu {

namespace detail {

template <typename Iterator, typename Default, typename = void_t<>>
struct MArrayIteratorCategory {
	typedef Default type;
};

template <typename Iterator, typename Default>
struct MArrayIteratorCategory<Iterator, Default, void_t<typename Iterator::iterator_category>> {
	typedef typename Iterator::iterator_category type;
};

template <typename Iterator, typename Default, typename = void_t<>>
struct MArrayIteratorPointer {
	typedef Default type;
};

template <typename Iterator, typename Default>
struct MArrayIteratorPointer<Iterator, Default, void_t<typename Iterator::pointer>> {
	typedef typename Iterator::pointer type;
};

template <typename Iterator, typename Default, typename = void_t<>>
struct MArrayIteratorDifferenceType {
	typedef Default type;
};

template <typename Iterator, typename Default>
struct MArrayIteratorDifferenceType<Iterator, Default, void_t<typename Iterator::difference_type>> {
	typedef typename Iterator::difference_type type;
};

// Iterator traits for MArrayIteratorTemplate and MArrayConstIteratorTemplate

// Note that value_type and reference are already provided by the array iterators. For iterator_category, pointer and
// difference_type, we try to import them from the array iterators as well (in case future version start providing
// them), or fall back to the specified default.
template <typename Iterator>
struct MArrayIteratorTraits {
	typedef typename MArrayIteratorCategory<Iterator, std::bidirectional_iterator_tag>::type iterator_category;
	typedef typename Iterator::value_type value_type;
	typedef typename Iterator::reference reference;
	typedef typename MArrayIteratorPointer<Iterator, void>::type pointer;
	typedef typename MArrayIteratorDifferenceType<Iterator, void>::type difference_type;
};

} // namespace detail

} // namespace mu

// STL compliant iterators are required to define all of the five types "difference_type", "value_type", "pointer",
// "reference" and "iterator_category" either inside the iterator or via a template specialization of
// std::iterator_traits. Using algorithms working on STL compliant iterators with a custom iterator that doesn't define
// all of the five above mentioned types is undefined behavior and therefore may or may not work.

// In C++17 the rules for std::iterator_traits were strictly specified to no longer provide any of the five types if not
// all of them are defined. GCC already implements this specification for C++11.

// MArrayIteratorTemplate and its base class MConstArrayIteratorTemplate don't define the types "difference_type",
// "pointer" and "iterator_category" and don't provide a template specialization of std::iterator_traits either.
// Therefore, using these iterators on STL algorithms doesn't compile anymore on all STL versions implementing the
// strict requirement.

// We provide template specializations of std::iterator_traits for MArrayIteratorTemplate and
// MArrayConstIteratorTemplate as a workaround for this issue.

namespace std {

template <typename ContainerType, typename ObjectTypeContained>
struct iterator_traits<OPENMAYA_MAJOR_NAMESPACE_SCHEMA::MArrayIteratorTemplate<ContainerType, ObjectTypeContained>>
    : mu::detail::MArrayIteratorTraits<
              OPENMAYA_MAJOR_NAMESPACE_SCHEMA::MArrayIteratorTemplate<ContainerType, ObjectTypeContained>> {};

template <typename ContainerType, typename ObjectTypeContained>
struct iterator_traits<OPENMAYA_MAJOR_NAMESPACE_SCHEMA::MArrayConstIteratorTemplate<ContainerType, ObjectTypeContained>>
    : mu::detail::MArrayIteratorTraits<
              OPENMAYA_MAJOR_NAMESPACE_SCHEMA::MArrayConstIteratorTemplate<ContainerType, ObjectTypeContained>> {};

} // namespace std

#endif // (MAYA_API_VERSION >= 20190000)
