# 3D Toolkit

### Installing Prebuilt Libraries for /3dtoolkit-directx

After cloning this repository, browse to /WebRTCLibs and run 
```
.\WebRTCLibs\webrtcInstallLibs.ps1
```
This will install 32bit and 64bit Debug, Release, Exe's and PDBs from this commit
https://chromium.googlesource.com/external/webrtc/+/7fb7bbd1798931a9c6c0b3d29e26466419583e54
<br>with this patch applied:
```
.\WebRTCLibs\0001-Andrei-s-support-for-nvencode-h264-encoder.patch
```
<br>
### Building WebRTC Libraries from Source
If you want to build webrtc yourself, run 
```
.\WebRTCLibs\webrtcSetup.ps1 -WebRTCFolder C:\path\here
```
(If path is omitted, it will install to C:\WebRTCSource)

This will automate all of the following steps<br>
https://webrtc.org/native-code/development/

1) Set environment variables
2) Download and install pre-requisites
3) Create webrtc repo from latest master
4) Create build definitions for Win32/64 Debug and Release
5) Run the builds for each configuration
6) (TODO) Clean the output
7) (TODO) Move to consumable folder structure
8) (TODO) Build to dll