import { Component } from 'react';
import {
  View,
  Button,
  Picker
} from 'react-native';

var WebRTC = require('react-native-webrtc');
var {
  RTCPeerConnection,
  RTCIceCandidate,
  RTCSessionDescription,
} = WebRTC;

const { ThreeDStreamingClient } = require('3dtoolkit');

import Config from '../config';

var streamingClient = new ThreeDStreamingClient(Config,
{
  RTCPeerConnection: window.mozRTCPeerConnection || window.webkitRTCPeerConnection || RTCPeerConnection,
  RTCSessionDescription: window.mozRTCSessionDescription || window.RTCSessionDescription || RTCSessionDescription,
  RTCIceCandidate: window.mozRTCIceCandidate || window.RTCIceCandidate || RTCIceCandidate
});

class HomeScreen extends Component {
  static navigationOptions = {
        title: 'Rn3dtksample',
      };

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

    if (peers) {
      this.setState({currentPeerId: Object.keys(peers)[0]});
    }

    this.setState({otherPeers: streamingClient.otherPeers});
  }

  onRemoteStreamAdded(event) {
    // this.setState({videoURL: event.stream.toURL()});

    this.props.navigation.navigate('VideoPlayback', { videoURL: event.stream.toURL(), sendInputData: streamingClient.sendInputChannelData.bind(streamingClient) });
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

      </View>
    );
  }
  renderPeers() {
    return Object.keys(this.state.otherPeers).map((key, index) => {
      return <Picker.Item key={index} label={this.state.otherPeers[key]} value={key} />
    });
  }
}

export default HomeScreen;
