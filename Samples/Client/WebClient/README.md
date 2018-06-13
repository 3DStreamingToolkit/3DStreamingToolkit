The WebClient sample is a vanilla JavaScript sample that supports mono rendering with input handlers for moving/rotating a 3D camera.

## Installing dependencies

All dependencies are installed using the `npm install` command.

```bash
$ npm install
```

## 3DStreamingToolkit JavaScript Library

The WebClient sample consumes a universal JavaScript plugin that is downloaded using npm. The source code can be found [here](https://github.com/CatalystCode/js-3dtoolkit). 

This library is responsible for connecting to the signaling server, exposing the data channel, connecting/disconnection to peers and handling the SDP offer between peers. By default, the library will request H264 video encoding when connecting to a peer. This is required for the low latency scenarios enabled by this toolkit.

## Config file

To connect to a specific signaling server, you need to modify the value inside `app.js`:
```  
 var defaultSignalingServerUrl = 'http://localhost:3001'
```

In case your scenario requires VPN/Proxy networks, you need to specify a turn server inside `app.js`:
```  
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
```

For more information read the [TURN-Service wiki](https://github.com/CatalystCode/3DStreamingToolkit/wiki/TURN-Service) and [sample implementation configuration files](https://github.com/CatalystCode/3DStreamingToolkit/wiki/JSON-Config-Files).

## Running the client

After all dependencies are installed, simply run the command:
```bash
$ http-server
```

## How it works

Once the config settings are changed, the client will require a server to connect to. In this toolkit, we have a few [Sample Servers](https://github.com/CatalystCode/3DStreamingToolkit/tree/master/Samples/Server). 

When both the client and server are connected to the same signaling server, they will appear in the peer list. 
![capture2](https://user-images.githubusercontent.com/10086264/33888334-64db4d18-df55-11e7-830a-174b9049cceb.PNG)

You can initiate the connection using the client or server, simply select the peer from the list and press join. That will start the video streaming and you can use Click+Drag to rotate the camera or CTRL+Drag to move the camera. 
![webclient](https://user-images.githubusercontent.com/10086264/33888182-fcf9e236-df54-11e7-9eac-0ba8f50cf0a4.PNG)


