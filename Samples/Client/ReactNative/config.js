export default Config = {
  'serverUrl': 'http://localhost:3001',
  'peerConnectionConfig': {
      'iceServers': [{
          'urls': 'turn:turnserveruri:5349',
          'username': 'user',
          'credential': 'password',
          'credentialType': 'password'
      }],
      'iceTransportPolicy': 'relay'
  }
}