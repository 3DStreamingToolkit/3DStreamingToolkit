import React, { Component } from 'react';
import {
    AppRegistry,
    StyleSheet,
    Text,
    View,
    Button,
    Picker,
    Dimensions 
  } from 'react-native';

const window = Dimensions.get('window');

var WebRTC = require('react-native-webrtc');
var {
  RTCView
} = WebRTC;

class VideoPlaybackScreen extends React.Component {

    constructor(props) {
        super(props);
        this.videoUrl = props.navigation.state.params.videoURL;
    }

    render() {
      const { navigate } = this.props.navigation;
      return (
        <RTCView streamURL={this.videoUrl} style={styles.remoteView}/>
      );
    }
  }

const styles = StyleSheet.create({
    remoteView: {
      height: window.height,
      width: window.width
    },
  });

export default VideoPlaybackScreen;