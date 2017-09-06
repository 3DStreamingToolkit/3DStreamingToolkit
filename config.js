export default Config = {
    'serverUrl': 'https://3dtoolkit-signaling-server.azurewebsites.net',
    'peerConnectionConfig': {
        'iceServers': [{
            'urls': 'turn:turnserver3dstreaming.centralus.cloudapp.azure.com:5349',
            'username': 'user',
            'credential': '3Dtoolkit072017',
            'credentialType': 'password'
        }],
        'iceTransportPolicy': 'relay',
        'optional': [
            { 'DtlsSrtpKeyAgreement': true }
        ]
    }
}