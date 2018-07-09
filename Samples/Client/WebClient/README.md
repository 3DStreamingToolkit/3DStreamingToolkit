The WebClient sample is a vanilla JavaScript sample that supports mono rendering with input handlers for moving/rotating a 3D camera.

## Installing dependencies

All dependencies are installed using the `npm install` command.

```bash
$ npm install
```

## 3DStreamingToolkit JavaScript Library

The WebClient sample consumes a universal JavaScript plugin that is downloaded using npm. The source code can be found [here](https://github.com/3DStreamingToolkit/js-3dstk). 

This library is responsible for connecting to the signaling server, exposing the data channel, connecting/disconnection to peers and handling the SDP offer between peers. By default, the library will request H264 video encoding when connecting to a peer. This is required for the low latency scenarios enabled by this toolkit.

## Config file

To connect to a specific signaling server, you need to modify the value inside `app.js`:
```  
 var defaultSignalingServerUrl = 'http://localhost:3001'
```

In case your scenario requires VPN/Proxy networks, you have two options to setup the configuration file:
1. The js-3dstk library is capable of retriving the TURN credentials automatically from the server. In order to enable this feature, simply enable the following flag inside `app.js`:
```
  // Use this switch to determing whether Turn
  // server credentials are parsed from sever
  // connection offer, or declared here in config.
  var parseTurnFromOffer = true;
```
2. You can also manually include the TURN server url and credentials inside `app.js`:
<pre>
  var parseTurnFromOffer = <b>false</b>;

  var pcConfigTURNStatic = {
    'iceServers': [{
        'urls': 'turn server goes here',
        'username': 'username goes here',
        'credential': 'password goes here',
        'credentialType': 'password'
    },
    {
      'urls': 'stun:stun.l.google.com:19302'
    }],
    'iceTransportPolicy': 'relay'
  };
</pre>

If no VPN/Proxy networks are required, simply change the line 87 inside `app.js` with the code below:
```
   var pcConfig = pcConfigSTUNStatic;
```

For more information read the [TURN-Service wiki](https://github.com/3DStreamingToolkit/3DStreamingToolkit/wiki/TURN-Service) and [sample implementation configuration files](https://github.com/3DStreamingToolkit/3DStreamingToolkit/wiki/JSON-Config-Files).

## Running the client

After all dependencies are installed, simply run the command:
```bash
$ http-server
```

## How it works

Once the config settings are changed, the client will require a server to connect to. In this toolkit, we have a few [Sample Servers](https://github.com/3DStreamingToolkit/3DStreamingToolkit/tree/master/Samples/Server). 

When both the client and server are connected to the same signaling server, they will appear in the peer list. 
![image](https://user-images.githubusercontent.com/10086264/41976434-229ea2e8-79eb-11e8-9d89-82e7d374702f.png)

You can initiate the connection using the client or server, simply select the peer from the list and press join. That will start the video streaming and you can use Click+Drag to rotate the camera or CTRL+Drag to move the camera. 
![image2](https://user-images.githubusercontent.com/10086264/41976864-0830f1bc-79ec-11e8-884f-456ce0edee36.png)


