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

import ThreeDStreamingClient from './ThreeDStreamingClient.js';

import Config from './config';

var streamingClient = new ThreeDStreamingClient(Config, 
{
  RTCPeerConnection: window.mozRTCPeerConnection || window.webkitRTCPeerConnection || RTCPeerConnection,
  RTCSessionDescription: window.mozRTCSessionDescription || window.RTCSessionDescription || RTCSessionDescription,
  RTCIceCandidate: window.mozRTCIceCandidate || window.RTCIceCandidate || RTCIceCandidate
});

export default class rn3dtksample extends Component {
  constructor(props) {
    super(props);

  streamingClient.signIn('ReactClient', 
    {
      onaddstream: this.onRemoteStreamAdded.bind(this),
      onremovestream: this.onRemoteStreamRemoved,
      onopen: this.onSessionOpened,
      onconnecting: this.onSessionConnecting,
      onupdatepeers: this.updatePeerList.bind(this)
    })
  .then(streamingClient.startHeartbeat.bind(streamingClient))
  .then(streamingClient.pollSignalingServer.bind(streamingClient, true));

  this.state = {otherPeers: streamingClient.otherPeers, 
                currentPeerId: 0,
                videoURL: null};
  }
  componentWillMount() {

  }
  componentWillUpdate(props, nextState) {
  }

  updatePeerList() {
    let peers = streamingClient.otherPeers;

    if(peers)
    {
      this.setState({currentPeerId: Object.keys(peers)[0]});
    }

    this.setState({otherPeers: streamingClient.otherPeers});
  }

  onRemoteStreamAdded(event) {
    this.setState({videoURL: event.stream.toURL()});
  }

  onRemoteStreamRemoved(event) {
  }

  onSessionOpened(event) {
  }

  onSessionConnecting(event) {
  }

  joinPeer() {
    // Join a peer
    streamingClient.joinPeer(this.state.currentPeerId);
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
        <RTCView streamURL={this.state.videoURL} style={styles.remoteView}/>
      </View>
    );
  }
  renderPeers() {
    return Object.keys(this.state.otherPeers).map((key, index) => {
      return <Picker.Item key={index} label={this.state.otherPeers[key]} value={key} />
    });
  }
}

const styles = StyleSheet.create({
  remoteView: {
    width: 500,
    height: 400,
  },
});

AppRegistry.registerComponent('rn3dtksample', () => rn3dtksample);