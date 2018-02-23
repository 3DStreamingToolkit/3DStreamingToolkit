# !IMPORTANT - USING UNITY AS A 3D VIDEO STREAMING SERVER IS AGAINST THE SOFTWARE TERMS OF SERVICE - THIS SAMPLE IS PROVIDED FOR DEMO AND EDUCATIONAL PURPOSES ONLY.  CONTACT UNITY FOR LICENSING IF YOU WANT TO USE THIS IN ANY COMMERCIAL ENVIRONMENT

## Please refer to https://unity3d.com/legal/terms-of-service/software 

> ### Streaming and Cloud Gaming Restrictions

> You may not directly or indirectly distribute Your Project Content by means of streaming or broadcasting where Your Project Content is primarily executed on a server and transmitted as a video stream or via low level graphics render commands over the open Internet to end user devices without a separate license from Unity. This restriction does not prevent end users from remotely accessing Your Project Content from an end user device that is running on another end user device.

## Required command line argument

### Unity Standalone Player

Run your app with **-force-d3d11-no-singlethreaded** command line argument.

### Windows Store Apps

Open **App.xaml.cs/cpp** or **App.cs/cpp** and at the end of constructor add:

```
appCallbacks.AddCommandLineArg("-force-d3d11-no-singlethreaded");
```

## Enable UV flag for DirectX HoloLens client

When using [RenderTexture](https://docs.unity3d.com/ScriptReference/RenderTexture.html), Unity follows OpenGL standard so we need to manually “flip” the screen texture upside down. To do that, enable **UNITY_UV_STARTS_AT_TOP** flag in **VideoRenderer.h**.