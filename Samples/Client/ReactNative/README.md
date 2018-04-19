# React Native client

An iOS and Android sample client written in React Native. It supports mono rendering with input handlers for moving/rotating a 3D camera. :iphone: :cloud:

## Installing dependencies

The React Native sample consumes a universal JavaScript plugin that is downloaded using npm. The source code can be found [here](https://github.com/CatalystCode/js-3dtoolkit). Installation is done using the `npm install` command.

```bash
$ npm install
```
## Config file

To connect to a specific signaling, you need to modify the values inside `config.js`:
```  
export default Config = {
   'serverUrl': 'http://localhost:3001',
   'peerConnectionConfig': {
      'iceServers': [{
          'urls': 'stun:stun.l.google.com:19302'
      }]
  } 
}
```

In case your scenario requires VPN/Proxy networks, you need to uncomment and specify a signaling and turn server inside 'config.js':
```  
export default Config = {
    'serverUrl': 'http://localhost:3001',
    'peerConnectionConfig': {
      'iceServers': [{
          'urls': 'turn:turnserveruri:5349',
          'username': 'user',
          'credential': 'password',
          'credentialType': 'password'
      },
      {
          'urls': 'stun:stun.l.google.com:19302'
      }],
      'iceTransportPolicy': 'relay'
  }
}
```

For more information read the [TURN-Service wiki](https://github.com/CatalystCode/3dtoolkit/wiki/TURN-Service) and [sample implementation configuration files](https://github.com/CatalystCode/3dtoolkit/wiki/JSON-Config-Files).

## Deploying the app

For deploying the app to iOS or Android, refer to the official [React Native documentation](https://facebook.github.io/react-native/docs/getting-started.html). Please note, the Android emulator does not support H264 decoding. We recommend testing on a physical device. 

## How it works

Once the config settings are changed, the client will require a server to connect to. In this toolkit, we have a few [Sample Servers](https://github.com/CatalystCode/3dtoolkit/tree/master/Samples/Server). 

When both the client and server are connected to the same signaling server, they will appear in the peer list. 

You can initiate the connection using the client or server, simply select the peer from the list and press join. That will start the video streaming and you can use one finger to rotate the camera or both fingers to translate around the scene.
