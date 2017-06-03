## Required command line argument

### Unity Standalone Player

Run your app with **-force-d3d11-no-singlethreaded** command line argument.

### Windows Store Apps

Open **App.xaml.cs/cpp** or **App.cs/cpp** and at the end of constructor add:

```
appCallbacks.AddCommandLineArg("-force-d3d11-no-singlethreaded");
```
