# 3D Toolkit

A toolkit for building powerful stereoscopic 3d experiences that run on azure
and stream to devices. :muscle: :eye: :cloud:

> Note: If you are using Visual Studio 2017, ensure you have installed the [v140 c++ runtime](https://www.microsoft.com/en-us/download/details.aspx?id=48145), and please __do not update our projects when prompted to do so__.

## Installing Prebuilt Libraries for /3dtoolkit-directx

After cloning this repository, run `.\setup.cmd` before opening the Project/Solution structure.

This will install and configure the following:

+ 32bit and 64bit Debug, Release, Exes, Dlls and PDBs from this commit [Chromium m58 release](https://chromium.googlesource.com/chromium/src/+/2b7c19d3)
+ [This patch](.\WebRTCLibs\nvencoder.patch) will be applied to the above
+ 32bit and 64bit Debug and Release libraries for DirectX Toolkit

> Note: We can't currently use the [directxtk nuget packages](https://www.nuget.org/packages?q=directxtk) because they don't provide static linking targets for release builds.

Once you see `Libraries retrieved and up to date` you may proceed and open [the solution](.\3dtoolkit-directx\Toolkit3D.sln).

## Build output

Build the solution from Visual Studio, and look in:

+ `\3dtoolkit-directx\Samples\**\Build\**` for server binaries.
+ `\WebRTCNativeClient\Build\**` for the native client binaries.
+ `\WebRTCLibs\**\Exe\**` for the sample signaling, turn and stun server binaries.

## Distributing binaries

> Note: Currently for others you'll need to zip and upload yourself to match the `.\WebRTCLibs\webrtcInstallLibs.ps1` script.

https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_headers.zip
https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_Win32.zip
https://3dtoolkitstorage.blob.core.windows.net/libs/m58patch_x64.zip

## Standing up production TURN/STUN/Signaling Servers

See https://github.com/anastasiia-zolochevska/3dstreaming-arm-templates
 
And source repos:
https://github.com/anastasiia-zolochevska/signaling-server-for-webrtc-native-client
https://github.com/anastasiia-zolochevska/turn-server-docker-image

## Building WebRTC Libraries from Source (You don't need to do this unless you are changing the encoder)

> Note: If you want to build webrtc yourself, you will need to install Visual Studio 2015 with Update 3.

[Visual Studio 2015 Update 3 Community Edition ISO](http://download.microsoft.com/download/b/e/d/bedddfc4-55f4-4748-90a8-ffe38a40e89f/vs2015.3.com_enu.iso)

[Visual Studio 2015 Update 3 Professional Edition ISO](http://download.microsoft.com/download/e/b/c/ebc2c43f-3821-4a0b-82b1-d05368af1604/vs2015.3.pro_enu.iso)

[Visual Studio 2015 Update 3 Enterprise Edition ISO](http://download.microsoft.com/download/8/4/3/843ec655-1b67-46c3-a7a4-10a1159cfa84/vs2015.3.ent_enu.iso)

Be sure to install all of the C++ language tools and the Windows Universal Components.

Once finished installing, install the [Windows DDK](https://go.microsoft.com/fwlink/p/?LinkID=845298) as it contains debugging tools needed by WebRTC.

Finally, launch PowerShell with Administrative persmissions and run 
```
.\WebRTCLibs\webrtcSetup.ps1 -WebRTCFolder C:\path\here
```
(If path is omitted, it will install to `C:\WebRTCSource`. Be careful with long path names, depot tools may fail to install.)

This will automate all of the following steps (from [here](https://webrtc.org/native-code/development/)):

1) Set environment variables
2) Download and install pre-requisites
3) Create webrtc repo from m58 Chromium release
4) Cleans repo
4) Create build definitions for Win32/64 Debug and Release
5) Run the builds for each configuration
6) Creates Dist folder structure
7) Moves all packages data to dist
8) (TODO) Auto-zip and upload to blob store

Once finished building, for local use, copy the contents of 
```
C:\<path to source>\webrtv-checkout\dist
```
to the `WebRTCLibs` folder in this repo.
