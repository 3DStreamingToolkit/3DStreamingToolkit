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
+ WebRTCEyes
+ WebRTCServer
+ WebRTCServerDebug
+ MutableState\PeerListState
+ (optional) Editor\WebRTCEyesEditor

Application `Scripts`:

+ DropdownSelectorButton
+ FractalRoot
+ Fractals
+ PeerListDropdownAdapter
+ (dependency) UnityPackages\MenuStack

More details coming soon, via https://github.com/CatalystCode/3dtoolkit/wiki/Unity-Fractal-Server

## Dependencies

+ Unity >= 5.6.4f1 (tested on 5.6.3f1)
+ Unity-MenuStack (for quick menu engineering)
+ Windows
+ DirectX11

## License

MIT