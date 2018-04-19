// Use this config if you require a TURN Server for VPN/Proxy networks. 
// See https://github.com/CatalystCode/3dtoolkit/wiki/TURN-Service  
/* export default Config = {
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
} */

// Use this configuration if no VPN/Proxy networks are needed. (used by default)
export default Config = {
   'serverUrl': 'http://localhost:3001',
   'peerConnectionConfig': {
      'iceServers': [{
          'urls': 'stun:stun.l.google.com:19302'
      }]
  } 
}