# 3D Toolkit

A toolkit for building powerful stereoscopic 3d experiences that run on the cloud and stream to devices. :muscle: :eye: :cloud:

> Note: If you are using Visual Studio 2017, ensure you have installed the v140 c++ build tools, and please __do not update our projects when prompted to do so__.

## Grabbing latest prebuilt binaries

> These binaries are prebuilt toolkit binaries - if you're looking to contribute, or build your own, keep reading! 

[CI Builds](https://3dtoolkitstorage.blob.core.windows.net/builds/index.html) - Not recommended

[Pull Request CI Builds](https://3dtoolkitstorage.blob.core.windows.net/pullrequests/index.html) - Not recommended

[Release/Stable Tagged Builds](https://3dtoolkitstorage.blob.core.windows.net/releases/index.html)

## Prerequisite Tooling

+ Windows 10 Anniversary Update / Windows Server 2012 R2 / Windows Server 2016
+ Visual Studio 2015 Update 3
+ Windows 10 SDK - 10.0.14393.795
+ [Windows 10 DDK](https://msdn.microsoft.com/en-us/library/windows/hardware/ff557573(v=vs.85).aspx) - If building WebRTC Library from source (note: this is likely not the case, and is only necessary if you're planning to modify our dependency library, WebRTC)

## Installing Prebuilt Dependencies

> Before running our `setup.cmd` script, please ensure powershell is set to [enable unrestricted script execution](https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies?view=powershell-5.1&viewFallbackFrom=powershell-Microsoft.PowerShell.Core).

After cloning this repository, run `.\setup.cmd` before opening the Project/Solution structure.

This will install and configure the following:

+ 32bit and 64bit Debug, Release, Exes, Dlls and PDBs from this commit [Chromium m58 release](https://chromium.googlesource.com/chromium/src/+/2b7c19d3)
+ [This patch](.\WebRTCLibs\nvencoder.patch) that adds nvencode support to webrtc, and will be applied to the above
+ 32bit and 64bit Debug and Release libraries for DirectX Toolkit
+ [WebRTC-UWP](https://github.com/webrtc-uwp/webrtc-uwp-sdk) M54 synced release for UWP-based clients (Hololens)

> Note: We can't currently use the [directxtk nuget packages](https://www.nuget.org/packages?q=directxtk) because they don't provide static linking targets for release builds.

Once you see `Libraries retrieved and up to date` you may proceed.

## Building the Toolkit

> Note: To build the unity client library, you must use `Release` and `x86` for the desired configuration

+ Open [the 3dtoolkit solution](./3DStreamingToolKit.sln) in Visual Studio
+ Build the solution (Build -> Build Solution) in the desired configuration (Build -> Configuration Manager -> Dropdowns at the top)
+ Done!

## Build output

The solution builds binaries for our pluggable components and our sample implementations. All of these are output to the `Build` directory in the root of the project directory. They'll be in subfolders for the desired build configuration, in folders named according to their project name. For instance, `Client\` contains our native client, and `SpinningCubeServer\` contains our spinning cube example.

## Building HoloLens Unity client 

 > Note: Make sure you have Unity version 5.6.3f1 with UWP tools support. 

These steps are unique, and specific to producing a unity client application for a hololens device.

You can find the HoloLens Unity client sample in `\Samples\Client\Unity`. The HoloLens client is using a native dll library for video decoding and rendering that needs to be built before you open the sample in Unity. Follow these steps:
+ Open `3DStreamingToolKit.sln` from the root folder 
+ Switch the build configuration to Release mode and x86. 
+ Under `Plugins -> UnityClient`, find MediaEngineUWP and WebRtcWrapper projects.
+ Build MediaEngineUWP and WebRtcWrapper. This will copy the necessary dll's to the Unity project folder. 
+ Open Unity and open the HoloLens client folder `\Samples\Client\Unity`
+ Under scenes folder, open 'StereoSideBySideTexture' scene.
+ Under StreamingAssets, you will find 'webrtcConfig.json'. Modify the values to match your signaling server and TURN server (optional, needed for VPN/Proxy networks)
+ In the scene hierarchy, open the Control gameobject and look at the Control Script. 
    - Texture Width and Height: These settings are used to display the stereoscopic texture from the server. The client is expecting a left and right eye side-by-side texture and it will crop it for each eye. Since our servers are sending 720p quality, the default values are 2560 x 720 (1280 x 2 width for each eye). Change this if you are sending different resolutions.
    - Frame Rate: This is the expected frame rate from the server. The native engine will be optimized to maintain this framerate. 30 is the currently recommended value, going above this, can cause loss of quality and corrupt frames. 
 > Note: Make sure you set the same frame rate on the server side!
+ Go to `File -> Build Settings` and select Windows Store platform. If that is not available, you need to add UWP support to your Unity installation. 
+ Select 
    - SDK: Universal 10
    - Target Device: HoloLens
    - UWP Build type: D3D
    - UWP SDK: 10.0.14393
+ Build the project to an empty folder and open the generated UnityX86.sln.
+ Switch configuration to Release mode and x86. 
+ Deploy to your machine or HoloLens. Make sure the server is running, the app will automatically connect to the ip address that was set in the step above.
+ Open the desired server and connect to the Unity client. 

## JSON Configuration:

> These configuration files control how our sample clients and servers communicate and connect. More information can be found [in the wiki](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files).

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

+ Signaling Server (Node.JS) - https://github.com/anastasiia-zolochevska/signaling-server
+ TURN Server (optional, needed for VPN/Proxy networks) - https://github.com/anastasiia-zolochevska/coturn-to-azure-deployment

## Building WebRTC Libraries from Source (You don't need to do this unless you are changing the encoder)

> Note: If you want to build webrtc yourself, you will need to install Visual Studio 2015 with Update 3.

[Visual Studio 2015 Update 3 Community Edition ISO](http://download.microsoft.com/download/b/e/d/bedddfc4-55f4-4748-90a8-ffe38a40e89f/vs2015.3.com_enu.iso)

[Visual Studio 2015 Update 3 Professional Edition ISO](http://download.microsoft.com/download/e/b/c/ebc2c43f-3821-4a0b-82b1-d05368af1604/vs2015.3.pro_enu.iso)

[Visual Studio 2015 Update 3 Enterprise Edition ISO](http://download.microsoft.com/download/8/4/3/843ec655-1b67-46c3-a7a4-10a1159cfa84/vs2015.3.ent_enu.iso)

Be sure to install all of the C++ language tools and the Windows Universal Components.

Once finished installing, install the [Windows DDK version 1607](https://go.microsoft.com/fwlink/p/?LinkId=526733) as it contains debugging tools needed by WebRTC.

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

# !IMPORTANT - USING UNITY AS A 3D VIDEO STREAMING SERVER IS AGAINST THE SOFTWARE TERMS OF SERVICE - THE UNITY SERVER SAMPLE IS PROVIDED FOR DEMO AND EDUCATIONAL PURPOSES ONLY.  CONTACT UNITY FOR LICENSING IF YOU WANT TO USE THE UNITY SERVER SAMPLE IN ANY COMMERCIAL ENVIRONMENT

## Please refer to https://unity3d.com/legal/terms-of-service/software 

> ### Streaming and Cloud Gaming Restrictions

> You may not directly or indirectly distribute Your Project Content by means of streaming or broadcasting where Your Project Content is primarily executed on a server and transmitted as a video stream or via low level graphics render commands over the open Internet to end user devices without a separate license from Unity. This restriction does not prevent end users from remotely accessing Your Project Content from an end user device that is running on another end user device.
