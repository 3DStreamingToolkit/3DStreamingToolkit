# WebRTC iOS framework

[__!!!__] Please report all WebRTC related (not specific to this binary build) bugs and questions to [discussion group](https://groups.google.com/forum/#!forum/discuss-webrtc) or [official bug tracker](https://bugs.chromium.org/p/webrtc/issues/list)

# Contents

- [Installation](#installation)
- [Usage](#usage)
- [Bitcode](#bitcode)
- [Information](#information)
- [Links](#links)

# Installation
Check the [Bitcode](#bitcode) section first to avoid linker errors!

__Cocoapods__ (add to Podfile):

```ruby
pod "WebRTC"
```

__Carthage__ (add to Cartfile):

```
github "Anakros/WebRTC"
```

__Manual__: just download framework from [the latest release](https://github.com/Anakros/WebRTC/releases/latest) and copy it to your project

>You can only use the binary release, because the whole WebRTC repository takes ~12Gb of disk space

# Usage

## Swift
```swift
import WebRTC

let device = UIDevice.string(for: UIDevice.deviceType())

print(device)
print(RTCInitializeSSL())
```

## Objective-C
```objc
@import WebRTC;

NSString *device = [UIDevice stringForDeviceType:[UIDevice deviceType]];

NSLog(@"%@", device);
NSLog(@"%d", RTCInitializeSSL());
```

# Bitcode

Bitcode isn't supported in the upstream for now. So you should disable it:

Go to your project's settings and find the *Build settings* tab, check *All* and search for *bitcode*, then disable it.

# Information

Built from `https://chromium.googlesource.com/external/webrtc/` using `tools_webrtc/ios/build_ios_libs.py` script with following modifications (to enable x86 architecture):

```diff
diff --git a/tools_webrtc/ios/build_ios_libs.py b/tools_webrtc/ios/build_ios_libs.py
index 734f3e216..e6f250c97 100755
--- a/tools_webrtc/ios/build_ios_libs.py
+++ b/tools_webrtc/ios/build_ios_libs.py
@@ -165,8 +165,6 @@ def main():

   # Ignoring x86 except for static libraries for now because of a GN build issue
   # where the generated dynamic framework has the wrong architectures.
-  if 'x86' in architectures and args.build_type != 'static_only':
-    architectures.remove('x86')

   # Build all architectures.
   for arch in architectures:
```

# Links

[WebRTC Homepage](https://webrtc.org/)

[WebRTC discussion group](https://groups.google.com/forum/#!forum/discuss-webrtc)

[CocoaDocs](http://cocoadocs.org/docsets/WebRTC/)

[CocoaPods Page](https://cocoapods.org/pods/WebRTC)

[WebRTC Bug Tracker](https://bugs.chromium.org/p/webrtc/issues/list)
