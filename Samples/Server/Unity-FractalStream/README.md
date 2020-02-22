## Required command line argument

### Unity Standalone Player

Run your app with **-force-d3d11-no-singlethreaded** command line argument.

### Windows Store Apps

Open **App.xaml.cs/cpp** or **App.cs/cpp** and at the end of constructor add:

```
appCallbacks.AddCommandLineArg("-force-d3d11-no-singlethreaded");
```

## UNITY_UV_STARTS_AT_TOP flag

When using [RenderTexture](https://docs.unity3d.com/ScriptReference/RenderTexture.html), Unity follows OpenGL standard so we need to manually “flip” the screen texture upside down by enabling **UNITY_UV_STARTS_AT_TOP** flag:
- DirectX UWP client: **Samples\Client\DirectxUWP\DirectxClientComponent\Content\VideoRenderer.h** 
- DirectX Win32 client: **Libraries\UserInterface\inc\client_main_window.h**
