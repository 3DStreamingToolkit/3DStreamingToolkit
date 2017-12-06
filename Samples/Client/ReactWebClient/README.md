# js-3dtoolkit
Universal JavaScript Client for the 3D Streaming Toolkit - Supports Node.js, Browser and React-Native.

## Installation

This is the Universal JavaScript module for the [3dtoolkit](https://github.com/catalystcode/3dtoolkit) project which allows you to build powerful stereoscopic 3D experiences that run on the cloud and stream to devices. You can use this JavaScript module on Node.js, the browser and React-Native.

To install for Node.js or React-Native, installation is done using the `npm install` command.

```bash
$ npm install js-3dtoolkit
```


## Getting Started on the Browser

To add to your browser, you have a few options.

1. You can either clone the repository and use the existing webpack build config to build a production version.
1. You can require the `ThreeDStreamingClient` into your existing modern web app.
1. You can link to the built version in `dist/` on GitHub (not recommended for production).

``` js
var streamingClient = new ThreeDToolkit.ThreeDStreamingClient({
    'serverUrl': server,
    'peerConnectionConfig': {
        'iceServers': [{
            'urls': 'turn:turnserver.server.com:5349',
            'username': 'user',
            'credential': 'password123',
            'credentialType': 'password'
        }],
        'iceTransportPolicy': 'relay'
    }
}, {
    RTCPeerConnection: window.mozRTCPeerConnection || window.webkitRTCPeerConnection || RTCPeerConnection,
    RTCSessionDescription: window.mozRTCSessionDescription || window.RTCSessionDescription || RTCSessionDescription,
    RTCIceCandidate: window.mozRTCIceCandidate || window.RTCIceCandidate || RTCIceCandidate
});

streamingClient.signIn(localName)
    .then(streamingClient.startHeartbeat.bind(streamingClient))
    .then(streamingClient.pollSignalingServer.bind(streamingClient, true));

// Join a peer
streamingClient.joinPeer(streamingClient.getPeerIdByName(peerName), {
    onaddstream: onRemoteStreamAdded,
    onremovestream: onRemoteStreamRemoved,
    onopen: onSessionOpened,
    onconnecting: onSessionConnecting
});
```

### Getting Started on Node.js and React-Native

For Node.js, simply replace how you import the client class.

```js
const { ThreeDStreamingClient } = require('3dtoolkit');

var streamingClient = new ThreeDStreamingClient(...);
```

## Testing

Testing is done via tape and mocking. To run the tests:

```bash
$ npm run test
```
