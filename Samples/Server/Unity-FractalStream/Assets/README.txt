Unity-FractalStream - v2, a 3dtoolkit unity server example supporting mono and stereo rendering

## What is this?

This is a unity server sample application that supports remote clients using the 3dtoolkit for
data transmission. This enables mono or stereo streaming of unity frame data, and receives data
from the connected client for input signaling.

Learn more @ https://github.com/CatalystCode/3dtoolkit/wiki/Unity-Fractal-Server

## Basic Architecture

Unity-FractalStream contains both interop code to leverage 3dtoolkit, as well as some sample
application logic.

Interop `Scripts`:

+ SimpleJSON
+ StreamingUnityServerPlugin
+ WebRTCServer
+ WebRTCServerDebug
+ MutableState\PeerListState

Application `Scripts`:

+ DropdownSelectorButton
+ FractalRoot
+ Fractals
+ PeerListDropdownAdapter
+ (dependency) UnityPackages\MenuStack

More details coming soon, via https://github.com/CatalystCode/3dtoolkit/wiki/Unity-Fractal-Server

## Running

To run this sample, you must build a windows standalone application, and run it with the `-force-d3d11-no-singlethreaded` flag.
For example: `Unity-FractalStream.exe -force-d3d11-no-singlethreaded`. You must also ensure that the display information that
is selected when the application first opens matches exactly the values for `TextureHeight` and `TextureWidth` in the unity client
`ControlScript.cs`. In some of our other server samples it is necessary for those `ControlScript.cs` values to be twice the values
of the server resolution, but with this sample that is not the case.

## Dependencies

+ Unity >= 5.6.4f1 (tested on 5.6.3f1)
+ Unity-MenuStack (for quick menu engineering)
+ Windows
+ DirectX11

## License

MIT