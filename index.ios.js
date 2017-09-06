import React, { Component } from 'react';
import {
  AppRegistry,
  StyleSheet,
  Text,
  View,
  Button,
  Picker
} from 'react-native';

import { ThreeDStreamingClient } from '3dtoolkit';

import Config from './config';

export default class rn3dtksample extends Component {
  constructor(props) {
    super(props);
  }
  componentWillMount() {

  }
  componentWillUpdate(props, nextState) {
  }
  render() {
    return (
      <View>
        <Button title='click me' onPress={this.signIn.bind(this)}>Sign In</Button>
        <Picker
          selectedValue={this.state.currentPeerId}
          onValueChange={(itemValue, itemIndex) => this.setState({currentPeerId: itemValue})}>
            {this.renderPeers()}
        </Picker>
        <Button title="join peer" onPress={this.joinPeer.bind(this)}>Join Peer</Button>
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