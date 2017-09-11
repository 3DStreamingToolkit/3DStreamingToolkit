# 3D Toolkit

A toolkit for creating 3D experiences that are traditionally out of reach on low-powered devices. :muscle: :eye: :cloud:

![example](./readme_data/example.gif)

## What is this?

The 3DToolkit project's purpose is to provide an approach for developing 3D server applications that stream frames to other devices over the network. Specifically:

1. Server-side libraries for remotely rendering 3D scenes
2. Client-side libraries for receiving streamed 3D scenes
3. Low-latency audio and video streams using WebRTC
4. High-performance video encoding and decoding using NVEncode

Why? Because the world is becoming increasingly mobile, but the demand for high-fidelity 3D content is only growing. We needed a scalable approach to make this sort of content available on low-powered, low-bandwidth devices.

![high level architecture](./readme_data/hl-arch.png)
Here's a high-level diagram of the components we've built, and how they interact with the underlying WebRTC and NVEncode technologies we've leveraged.

## How to build

> If you don't wish to build the application yourself, you can download our latest build [here](https://github.com/CatalystCode/3dtoolkit/releases/latest).

These steps will ensure your development environment is configured properly, and then they'll walk you through the process of building our code.

### Prerequisites 

> Note: If you are using Visual Studio 2017, ensure you have installed the Visual Studio 2015 Update 3 as well, and please do not update our projects when prompted to do so.

+ Windows 10 Anniversary Update / Windows Server 2012 R2 / Windows Server 2016 (see [which version of Windows you have](https://binged.it/2xgQqRI)) 
+ [Visual Studio 2015 Update 3](https://www.visualstudio.com/en-us/news/releasenotes/vs2015-update3-vs)
+ [Windows 10 SDK - 10.0.14393.795](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive)
+ [Azure SDK 3.0.1](https://www.microsoft.com/web/handlers/webpi.ashx/getinstaller/VWDOrVs2015AzurePack.appids) for Visual Studio 2015 (Optional, but required to use the Server Azure Deployment projects)

### Installing dependencies

> Note: Before running our `setup.cmd` script, please ensure powershell is set to [enable unrestricted script execution](https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies?view=powershell-5.1&viewFallbackFrom=powershell-Microsoft.PowerShell.Core).

Run `.\setup.cmd` from the command line. This will install and configure the following:

+ 32bit and 64bit Debug, Release, Exes, Dlls and PDBs from this commit [Chromium m58 release](https://chromium.googlesource.com/chromium/src/+/2b7c19d3)
+ [This patch](.\WebRTCLibs\nvencoder.patch), which adds nvencode support to webrtc, and will be applied to the above
+ 32bit and 64bit Debug and Release libraries for DirectX Toolkit
+ [WebRTC-UWP](https://github.com/webrtc-uwp/webrtc-uwp-sdk) M54 synced release for UWP-based clients (Hololens)

Once you see `Libraries retrieved and up to date` you may proceed.

### The actual build

> Note: To build the unity client library, you must use `Release` and `x86` for the desired configuration.

+ Open [the 3dtoolkit solution](./3DStreamingToolKit.sln) in Visual Studio
+ Build the solution (Build -> Build Solution) in the desired configuration (Build -> Configuration Manager -> Dropdowns at the top)
+ Done!

If you're seeing errors, check out the [troubleshooting guide](https://github.com/CatalystCode/3dtoolkit/wiki/FAQ) and then [file an issue](https://github.com/catalystcode/3dtoolkit/issues/new).

## Build output

After you've built the solution, you'll likely want to start a sample server implementation, and a sample client implementation. These are applications that we've build to demonstrate the behaviors the toolkit provides.

> Note: We advise you to try `Spinning Cube` and `Win32Client` to begin, as these are the simpliest sample implementations.

Here's a table illustrating where each sample implementation will be built to. To run one, navigate to that location and start the `exe`.

> Note: the following table describes the location under `Build\<Platform>\<Configuration>\` where a sample can be found. To identify what `<Platform>` and `<Configuration>` are, see your desired configuration from [section: the actual build](#the-actual-build).

<table>
<tr>
<th>Sample Type</th>
<th>Sample Name</th>
<th>Location</th>
<th>Exe</th>
</tr>
<tr>
<td>Server</td>
<td>Spinning Cube</td>
<td>SpinningCubeServer</td>
<td>`SpinningCubeServer.exe`</td>
</tr>
<tr>
<td>Server</td>
<td>MultiThreaded</td>
<td>MultiThreadedServer</td>
<td>`MultiThreadedServer.exe`</td>
</tr>
<tr>
<td>Client</td>
<td>Win32Client</td>
<td>Client</td>
<td>`StreamingDirectXClient.exe`</td>
</tr>
</table>

Once you start both a server and client implementation, you should be seeing success! If you're instead seeing errors, check out the [Troubleshooting guide](https://github.com/CatalystCode/3dtoolkit/wiki/FAQ) and then [file an issue](https://github.com/catalystcode/3dtoolkit/issues/new). Additionally, you can see more information about our other sample implementations [here](https://github.com/CatalystCode/3dtoolkit/wiki/Feature-matrices).

## Specifics

These resources will be critical to your success in configuring and scaling applications.

+ [Sample implementation configuration files](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files)
+ [Configuring and deploying services](https://github.com/CatalystCode/3dtoolkit/wiki#services-setup)
+ [Configuring authentication](https://github.com/CatalystCode/3dtoolkit/wiki/Configuring-authentication)

## Learn More

+ [General Wiki](https://github.com/CatalystCode/3dtoolkit/wiki)
+ [Building WebRTC from source](https://github.com/CatalystCode/3dtoolkit/wiki/Building-WebRTC-from-source)
+ [Tyler's talk about our 3DToolkit](#todo-link)
+ [WebRTC Homepage](https://webrtc.org/)
+ [NVEncode Homepage](https://developer.nvidia.com/nvidia-video-codec-sdk)

## License

MIT

### License Considerations

# !IMPORTANT - USING UNITY AS A 3D VIDEO STREAMING SERVER IS AGAINST THE SOFTWARE TERMS OF SERVICE - THE UNITY SERVER SAMPLE IS PROVIDED FOR DEMO AND EDUCATIONAL PURPOSES ONLY.  CONTACT UNITY FOR LICENSING IF YOU WANT TO USE THE UNITY SERVER SAMPLE IN ANY COMMERCIAL ENVIRONMENT

## Please refer to https://unity3d.com/legal/terms-of-service/software 

> ### Streaming and Cloud Gaming Restrictions

> You may not directly or indirectly distribute Your Project Content by means of streaming or broadcasting where Your Project Content is primarily executed on a server and transmitted as a video stream or via low level graphics render commands over the open Internet to end user devices without a separate license from Unity. This restriction does not prevent end users from remotely accessing Your Project Content from an end user device that is running on another end user device.