# 3D Toolkit

### Installing Prebuilt Libraries for /3dtoolkit-directx

After cloning this repository, browse to /WebRTCLibs and run 
```
.\WebRTCLibs\webrtcInstallLibs.ps1
```
Run this command before opening the Project/Solution structure.
This will install 32bit and 64bit Debug, Release, Exes, Dlls and PDBs from this commit
Chromium m58 release
<br>with this patch applied:
```
.\WebRTCLibs\nvencoder.patch
```
<br>

### Building WebRTC Libraries from Source
If you want to build webrtc yourself, you will need to install Visual Studio 2015 with Update 3.

[Visual Studio 2015 Update 3 Community Edition ISO](http://download.microsoft.com/download/b/e/d/bedddfc4-55f4-4748-90a8-ffe38a40e89f/vs2015.3.com_enu.iso)

[Visual Studio 2015 Update 3 Professional Edition ISO](http://download.microsoft.com/download/e/b/c/ebc2c43f-3821-4a0b-82b1-d05368af1604/vs2015.3.pro_enu.iso)

[Visual Studio 2015 Update 3 Enterprise Edition ISO](http://download.microsoft.com/download/8/4/3/843ec655-1b67-46c3-a7a4-10a1159cfa84/vs2015.3.ent_enu.iso)

Be sure to install all of the C++ language tools and the Windows Universal Components.

Once finished installing, install the [Windows DDK](https://go.microsoft.com/fwlink/p/?LinkID=845298) as it contains debugging tools needed by WebRTC.

Finally, run 
```
.\WebRTCLibs\webrtcSetup.ps1 -WebRTCFolder C:\path\here
```
(If path is omitted, it will install to C:\WebRTCSource)

This will automate all of the following steps<br>
https://webrtc.org/native-code/development/

1) Set environment variables
2) Download and install pre-requisites
3) Create webrtc repo from m58 Chromium release
4) Cleans repo
4) Create build definitions for Win32/64 Debug and Release
5) Run the builds for each configuration
6) Creates Dist folder structure
7) Moves all packages data to dist
8) (TODO) Auto-zip and upload to blob store

Currently you'll need to zip and upload yourself to match the .\WebRTCLibs\webrtcInstallLibs.ps1 script.
https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_headers.zip
https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_Win32.zip
https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_x64.zip
