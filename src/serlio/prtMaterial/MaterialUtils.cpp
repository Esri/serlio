#include "prtMaterial/MaterialUtils.h"

#include "util/MArrayWrapper.h"
#include "util/MayaUtilities.h"

#include "maya/MPlugArray.h"

MStatus getMeshName(MString& meshName, const MPlug& plug) {
	MStatus status;
	bool searchEnded = false;

	for (MPlug curPlug = plug; !searchEnded;) {
		searchEnded = true;
		MPlugArray connectedPlugs;
		curPlug.connectedTo(connectedPlugs, false, true, &status);
		MCHECK(status);
		if (!connectedPlugs.length()) {
			return MStatus::kFailure;
		}
		for (const auto& connectedPlug : mu::makeMArrayConstWrapper(connectedPlugs)) {
			MFnDependencyNode connectedDepNode(connectedPlug.node(), &status);
			MCHECK(status);
			MObject connectedDepNodeObj = connectedDepNode.object(&status);
			MCHECK(status);
			if (connectedDepNodeObj.hasFn(MFn::kMesh)) {
				meshName = connectedDepNode.name(&status);
				MCHECK(status);
				break;
			}
			if (connectedDepNodeObj.hasFn(MFn::kGroupParts)) {
				curPlug = connectedDepNode.findPlug("outputGeometry", true, &status);
				MCHECK(status);
				searchEnded = false;
				break;
			}
		}
	}

	return MStatus::kSuccess;
}
