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

#include "util/MItDependencyNodesWrapper.h"

#include "util/MayaUtilities.h"

#include <maya/MItDependencyNodes.h>

MItDependencyNodesWrapperIt::MItDependencyNodesWrapperIt(MItDependencyNodes& itDepNodes)
	: itDepNodes(&itDepNodes) {
	MStatus status;
	const bool isDone = this->itDepNodes->isDone(&status);
	MCHECK(status);
	if (isDone) {
		this->itDepNodes = nullptr;
		return;
	}
	curObject = this->itDepNodes->thisNode(&status);
	MCHECK(status);
}

MItDependencyNodesWrapperIt& MItDependencyNodesWrapperIt::operator++() {
	MStatus status;
	const bool isDone = itDepNodes->isDone(&status);
	MCHECK(status);
	if (isDone) {
		itDepNodes = nullptr;
	}
	else {
		status = itDepNodes->next();
		MCHECK(status);
		curObject = itDepNodes->thisNode(&status);
		MCHECK(status);
	}
	return *this;
}
