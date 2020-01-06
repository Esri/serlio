#include "MaterialNameSource.h"

#include "util/MItDependencyNodesWrapper.h"
#include "util/MayaUtilities.h"

#include "maya/MItDependencyNodes.h"

MaterialNameSource::MaterialNameSource() {
	MStatus status;
	MItDependencyNodes itDependencyNodes(MFn::kShadingEngine, &status);
	MCHECK(status);
	for (const auto& dependencyNode : MItDependencyNodesWrapper(itDependencyNodes)) {
		MFnDependencyNode fnDependencyNode(dependencyNode, &status);
		MCHECK(status);
		MString shaderNodeName = fnDependencyNode.name(&status);
		MCHECK(status);
		names.insert(shaderNodeName.asWChar());
	}
}

std::pair<std::wstring, std::wstring> MaterialNameSource::getUniqueName(const std::wstring& baseName) {
	std::wstring shadingEngineName = baseName + L"Sg";

	int shIdx = 0;
	while (names.find(shadingEngineName + std::to_wstring(shIdx)) != names.end()) {
		shIdx++;
	}
	shadingEngineName += std::to_wstring(shIdx);
	names.insert(shadingEngineName);

	const std::wstring shaderName = baseName + L"Sh" + std::to_wstring(shIdx);
	// TODO: we should also check that this shader does not exist

	return {shadingEngineName, shaderName};
}
