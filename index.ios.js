import React, { Component } from 'react';
import {
  AppRegistry,
  StyleSheet,
  Text,
  View,
  Button,
  Picker
} from 'react-native';

var WebRTC = require('react-native-webrtc');
var {
  RTCPeerConnection,
  RTCIceCandidate,
  RTCSessionDescription,
  RTCView,
  MediaStream,
  MediaStreamTrack,
  getUserMedia,
} = WebRTC;

import ThreeDStreamingClient from './ThreeDStreamingClient';

import Config from './config';

var streamingClient = new ThreeDStreamingClient({
  'serverUrl': 'https://signaling-server591a.azurewebsites.net',
  'peerConnectionConfig': {
      'iceServers': [{
          'urls': 'turn:turnserver3dstreaming.centralus.cloudapp.azure.com:5349',
          'username': 'user',
          'credential': '3Dtoolkit072017',
          'credentialType': 'password'
      }],
      'iceTransportPolicy': 'relay'
  }
}, {
  RTCPeerConnection: window.mozRTCPeerConnection || window.webkitRTCPeerConnection || RTCPeerConnection,
  RTCSessionDescription: window.mozRTCSessionDescription || window.RTCSessionDescription || RTCSessionDescription,
  RTCIceCandidate: window.mozRTCIceCandidate || window.RTCIceCandidate || RTCIceCandidate
});

export default class rn3dtksample extends Component {
  constructor(props) {
    super(props);

  streamingClient.signIn('ReactClient')
  .then((this.updatePeerList).bind(this))
  .then(streamingClient.startHeartbeat.bind(streamingClient))
  .then(streamingClient.pollSignalingServer.bind(streamingClient, true));


  this.state = {otherPeers: streamingClient.otherPeers, 
                currentPeerId: 0,
                videoURL: ''};
  }
  componentWillMount() {

  }
  componentWillUpdate(props, nextState) {
  }

  updatePeerList() {
    let peers = streamingClient.otherPeers;

    if(peers.count > 0)
    {
      this.setState({currentPeerId: peers[0]});
    }

    this.setState({otherPeers: streamingClient.otherPeers});
  }

  onRemoteStreamAdded(event) {
    this.setState({videoURL: event.stream});
  }

  onRemoteStreamRemoved(event) {
  }

  onSessionOpened(event) {
  }

  onSessionConnecting(event) {
  }

  joinPeer() {
    // Join a peer
    streamingClient.joinPeer(this.state.currentPeerId, {
      onaddstream: this.onRemoteStreamAdded.bind(this),
      onremovestream: this.onRemoteStreamRemoved,
      onopen: this.onSessionOpened,
      onconnecting: this.onSessionConnecting
    });
  }

  render() {
    return (
      <View>
        <Picker
          selectedValue={this.state.currentPeerId}
          onValueChange={(itemValue, itemIndex) => this.setState({currentPeerId: itemValue})}>
            {this.renderPeers()}
        </Picker>
        <Button title="Join peer" onPress={this.joinPeer.bind(this)}>Join Peer</Button>
        <RTCView streamURL={this.state.videoURL}/>
      </View>
    );
  }
  renderPeers() {
    return Object.keys(this.state.otherPeers).map((key, index) => {
      return <Picker.Item key={index} label={this.state.otherPeers[key]} value={key} />
    });
  }
}

AppRegistry.registerComponent('rn3dtksample', () => rn3dtksample);