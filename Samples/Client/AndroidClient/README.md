# AndroidClient

An Android client written in Java

## Building the client
+ Requires Android Studio 2.3.3
+ Edit the file [Config.java](./AndroidClient/app/src/main/java/microsoft/a3dtoolkitandroid/util/Config.java) to enter your proper server configuration.
+ Build the project

## Controls
+ To pan: Use one finger
+ To move forward/backward: Keep one finger down and slide second finger up/down to move.
+ (TODO): Accelerometer control.

## Configuration

> Note: eventually this configuration will be moved to a [webrtcConfig](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#webrtc-configuration-webrtcconfigjson) file that matches our existing schema.

The current configuration values are as follows:

```
    public static String signalingServer = "http://localhost:3001";
    public static String turnServer = "turn:turn:turnserveruri:5349";
    public static String username = "user";
    public static String credential = "password";
    public static PeerConnection.TlsCertPolicy tlsCertPolicy = PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_SECURE;
```

With the exception of `tlsCertPolicy` these map directly to [webrtcConfig](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#webrtc-configuration-webrtcconfigjson) options, as follows:

+ `signalingServer` => [webrtcConfig#server](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#server) 
+ `turnServer` => [webrtcConfig#turnServer#uri](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#uri)
+ `username` => [webrtcConfig#turnServer#username](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#username)
+ `credential` => [webrtcConfig#turnServer#password](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files#password)

uniquely for `tlsCertPolicy`, we allow the caller to configure how the underlying tls library will verify certificates. This supports
the following values:

+ `PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK` - disables certificate and chain of trust checks
+ `PeerConnection.TlsCertPolicy.TLS_CERT_POLICY_SECURE` - (default) ensures certificate integrity and chain of trust are valid
