# 3D Toolkit

A toolkit for building powerful stereoscopic 3d experiences that run on azure
and stream to devices. :muscle: :eye: :cloud:

> Note: If you are using Visual Studio 2017, ensure you have installed the v140 c++ build tools, and please __do not update our projects when prompted to do so__.

## Grabbing latest prebuilt binaries

[CI Builds](https://3dtoolkitstorage.blob.core.windows.net/builds/index.html) - Not recommended

[Pull Request CI Builds](https://3dtoolkitstorage.blob.core.windows.net/pullrequests/index.html) - Not recommended

[Release/Stable Tagged Builds](https://3dtoolkitstorage.blob.core.windows.net/releases/index.html)

## Prerequisites

+ Windows 10 Anniversary Update / Windows Server 2012 R2 / Windows Server 2016
+ Visual Studio 2015 Update 3
+ Windows 10 SDK - 10.0.14393.795
+ [Windows 10 DDK](https://msdn.microsoft.com/en-us/library/windows/hardware/ff557573(v=vs.85).aspx) - If building WebRTC Library from source

## Installing Prebuilt Libraries for /3dtoolkit-directx

After cloning this repository, run `.\setup.cmd` before opening the Project/Solution structure.

This will install and configure the following:

+ 32bit and 64bit Debug, Release, Exes, Dlls and PDBs from this commit [Chromium m58 release](https://chromium.googlesource.com/chromium/src/+/2b7c19d3)
+ [This patch](.\WebRTCLibs\nvencoder.patch) will be applied to the above
+ 32bit and 64bit Debug and Release libraries for DirectX Toolkit
+ [WebRTC-UWP](https://github.com/webrtc-uwp/webrtc-uwp-sdk) M54 synced release for UWP-based clients (Hololens)

> Note: We can't currently use the [directxtk nuget packages](https://www.nuget.org/packages?q=directxtk) because they don't provide static linking targets for release builds.

Once you see `Libraries retrieved and up to date` you may proceed and open [the solution](.\3dtoolkit-directx\Toolkit3D.sln).

## Build output

Build the solution from Visual Studio, and look in:

+ `\3dtoolkit-directx\Samples\**\Build\**` for server binaries.
+ `\WebRTCNativeClient\Build\**` for the native client binaries.
+ `\WebRTCLibs\**\Exe\**` for the sample signaling, turn and stun server binaries.

## Building HoloLens Unity client 

 > Note: Make sure you have Unity version 5.6.1f1 with UWP tools support. 

You can find the HoloLens Unity client sample in `\Samples\Client\Unity`. The HoloLens client is using a native dll library for video decoding that needs to be built before you open the sample in Unity. Follow these steps:
+ Open `3DStreamingToolKit.sln` from the root folder 
+ Switch the build configuration to Release mode and x86. 
+ Under `Plugins -> UnityClient`, find TexturesUWP project and open TexturesUWP.cpp.
+ Build TexturesUWP and WebRtcWrapper. This will copy the necessary dll's to the Unity project folder. 
+ Open Unity and open the HoloLens client folder `\Samples\Client\Unity` and open StereoSideBySideTexture scene.
+ Open ControlScript.cs under the Scripts folder. (For now) You need to manually set the texture resolution depending on what server you are using. 
+ Modify the constant attributes textureWidth and textureHeight. These are the resolutions for our current server implementations:
    - MultithreadedServer - 1280 x 720
    - MultithreadedServerStereo - 2560 x 720
	- SpinningCubeServer - 640 x 480
	- SpinningCubeServerStereo - 1280 x 480
+ In the Awake() function, uncomment //Azure Host Details to use the Azure signalling server or use the local host address to test on your machine.
+ Go to `File -> Build Settings` and select Windows Store platform. If that is not available, you need to add UWP support to your Unity installation. 
+ Select SDK: Universal 10; Target Device: HoloLens; UWP Build type: D3D and UWP SDK: Latest Installed. 
+ Build the project to an empty folder and open the generated UnityX86.sln.
+ Switch configuration to Release mode and x86. 
+ Deploy to your machine or HoloLens. Make sure the server is running, the app will automatically connect to the ip address that was set in the step above.
+ Open the desired server and connect to the Unity client. 

## JSON Configuration Options for Native Client and Native Servers:

Use [webrtcConfig.json](https://github.com/CatalystCode/3dtoolkit/blob/master/Plugins/NativeServerPlugin/webrtcConfig.json) for TURN relay communication

Use [webrtcConfigStun.json](https://github.com/CatalystCode/3dtoolkit/blob/master/Plugins/NativeServerPlugin/webrtcConfigStun.json) for STUN P2P communication

At a minimum, you will need to deploy an instance of the signaling server below and use the STUN configuration file to test the native client and server.  You can run client, server and signaling server from the same development machine.

[nvEncConfig.json](https://github.com/CatalystCode/3dtoolkit/blob/v0.1.0/Plugins/NativeServerPlugin/nvEncConfig.json) is the nvencode configuration file.  
+ Set "useSoftwareEncoding" to true to use the CPU for video encoding - this will increase latency and should only be used for development on computers without an nvidia video card.
+ Set "serverFrameCaptureFPS": 60 and "fps" to the same value.  Maximum FPS is dependent on card and scene complexity.
+ Set "bitrate" and "minBitrate" to set the max and min bitrates for video streaming - recommended maximum @ 10mbps and min to 5.5mbps for high quality streaming.  Anything over 10mbps will not visibly increase quality (see test runners for validation).  Setting minBitrate to 0 enables webrtc to drop bitrate to whatever it needs to in order to keep video streaming fluidly.

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
