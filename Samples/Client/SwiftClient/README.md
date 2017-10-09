# SwiftClient

An iOS client written in swift :iphone: :cloud:

## Building the client

> Note: If a linking error occurs, set the flag `Enable Bitcode` to `NO` under the build settings tab for the project.

+ Install the required WebRTC library by executing `pod install` from the directory containing the `Podfile`.
+ Edit the file [Config.swift](./SwiftClient/Config.swift) to enter your proper server configuration.
+ Build the project

## Configuration

> Note: eventually this configuration will be moved to a [webrtcConfig](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#webrtc-configuration-webrtcconfigjson) file that matches our existing schema.

The current configuration values are as follows:

```
static let signalingServer = "YOUR SIGNALING SERVER"
static let turnServer = "YOUR TURN SERVER"
static let username = "YOUR USERNAME"
static let credential = "YOUR CREDENTIAL"
static let tlsCertPolicy = RTCTlsCertPolicy.secure
```

With the exception of `tlsCertPolicy` these map directly to [webrtcConfig](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#webrtc-configuration-webrtcconfigjson) options, as follows:

+ `signalingServer` => [webrtcConfig#server](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#server) 
+ `turnServer` => [webrtcConfig#turnServer#uri](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#uri)
+ `username` => [webrtcConfig#turnServer#username](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#username)
+ `credential` => [webrtcConfig#turnServer#password](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files#password)

uniquely for `tlsCertPolicy`, we allow the caller to configure how the underlying tls library will verify certificates. This supports
the following values:

+ `RTCTlsCertPolicy.insecureNoCheck` - disables certificate and chain of trust checks
+ `RTCTlsCertPolicy.secure` - (default) ensures certificate integrity and chain of trust are valid