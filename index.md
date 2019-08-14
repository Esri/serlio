---
title: Serlio - CityEngine Plugin for Autodesk Maya
---

<table>
<tr>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="#downloads"><img src="images/cog.png" width="100px" /> Downloads</a></td>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="#examples"><img src="images/bldg3d.png" width="100px" /> Examples</a></td>
	<td style="font-size:22pt;text-align:center;vertical-align:bottom"><a href="https://github.com/esri/serlio/blob/master/README.md#documentation"><img src="images/docs.png" width="100px" /> Documentation</a></td>
</tr>
</table>

<img src="images/serlio_icon_v1_512.png" width="110" align="right" />Serlio is a plugin for [Autodesk Maya](https://www.autodesk.com/maya). It provides a modifier node which enables the execution of [Esri CityEngine](http://www.esri.com/software/cityengine) ‘rules’ within a Maya scene. Therefore, a 3D environment artist does not have to leave their familiar Maya toolset anymore to make use of CityEngine’s procedural modeling power. Complicated export-import pipelines are no longer needed, which also means that the procedural building models do not need to be “baked” anymore. The buildings stay procedural during the entire modeling workflow. Consequently, the 3D environment artist can change the height, style and appearance of buildings easily with a parametric interface at any point during production.

Serlio requires so-called rule packages (RPK) as input, which are authored in CityEngine. An RPK includes assets and a CGA rule file which encodes an architectural style. Comprehensive RPK examples are available below and can be used “out-of-the-box” in Serlio.

Serlio is well suited for managing the procedural generation of architectural 3D content in digital sets. However, Serlio is restricted to the procedural generation of single buildings / objects. Serlio does not include the city layouting and street network editing tools of CityEngine i.e. the rich CityEngine toolset to design a city from scratch (or based on geographic data) is still needed.

*Serlio is free for non-commercial use.* Commercial use requires at least one commercial license of the latest CityEngine version installed in the organization. No redistribution is allowed. Please refer to the licensing section below for more detailed licensing information.


## Downloads

| ![Linux](images/linux.png)   | [Serlio 1.0.0, Maya 2019, PRT 2.0](https://github.com/Esri/serlio/releases/download/v1.0.0/serlio-1.0.0+b2980-rhel7-gcc63-x86_64-rel-opt-maya2019.tar.gz) |
| ![Windows](images/win32.png) | [Serlio 1.0.0, Maya 2019, PRT 2.0](https://github.com/Esri/serlio/releases/download/v1.0.0/serlio-installer-1.0.0+b2980-win10-vc141-x86_64-rel-opt-maya2019.msi) |

More downloads (also for Maya 2018) are available on the [github release page](https://github.com/esri/serlio/releases).


## Examples

Coming soon...


## Licensing

Serlio is under the same license as the included [CityEngine SDK](https://github.com/Esri/esri-cityengine-sdk#licensing).

An exception is the Serlio source code (without CityEngine SDK, binaries, or object code), which is licensed under the Apache License, Version 2.0 (the “License”); you may not use this work except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
