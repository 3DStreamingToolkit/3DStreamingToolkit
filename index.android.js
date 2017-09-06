/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 * @flow
 */

import React, { Component } from 'react';
import {
  AppRegistry,
  StyleSheet,
  Text,
  TouchableHighlight,
  View
} from 'react-native';

import {
  RTCPeerConnection,
  RTCIceCandidate,
  RTCSessionDescription,
  RTCView,
  MediaStream,
  MediaStreamTrack,
  getUserMedia,
} from 'react-native-webrtc';

const XMLHttpRequest = require('XMLHttpRequest');

var pcConfigStatic = {
  "iceServers": [{
    "urls": "turn:turnserver3dstreaming.centralus.cloudapp.azure.com:5349",
    "username": "user",
    "credential": "3Dtoolkit072017",
    "credentialType": "password"
  }],
  "iceTransportPolicy": "relay"
};

export default class rn3dtksample extends Component {
  render() {
    return (
      <View style={styles.container}>
        <TouchableHighlight onPress={this.login}>
          <Text style={styles.welcome}>
            Login
          </Text>
        </TouchableHighlight>
      </View>
    );
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
  welcome: {
    fontSize: 20,
    textAlign: 'center',
    margin: 10,
  },
  instructions: {
    textAlign: 'center',
    color: '#333333',
    marginBottom: 5,
  },
});

AppRegistry.registerComponent('rn3dtksample', () => rn3dtksample);
