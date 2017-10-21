import React, { Component } from 'react';
import {
  Button,
  Platform,
  StyleSheet,
  Text,
  View
} from 'react-native';

import {
  createIdentityMatrix
} from './matrixMath';

const LookatMatrixGestureHandler = require('./LookatMatrixGestureHandler');

class LookatExample extends Component {
  state = {
    lookat: createIdentityMatrix(),
    moveGesture: 'pan',
  }

  _onLookatChanged = matrix => {
    this.setState({
      lookat: matrix,
    });

    // TODO: send lookat over data channel
  }

  _format(n) {
    return n < 0 ? n.toFixed(3).substring(0, 6) : ' ' + n.toFixed(3).substring(0, 5);
  }

  render() {
    let _cameraHandler;
    let helpText = 'One-finger pan to look around,\n' + (this.state.moveGesture === 'pan' ? 'two-finger pan to move' : 'pinch to move');
    return (
      <View style={styles.container}>
        <Text style={styles.helpText}>{helpText}</Text>
        <LookatMatrixGestureHandler
          ref={(cameraHandler) => { _cameraHandler = cameraHandler; }}
          moveGesture={this.state.moveGesture}
          onLookatChanged={this._onLookatChanged}>
          <View style={styles.gestureContainer}>
            <Text style={styles.matrixText}>
              <Text>|{this._format(this.state.lookat[0])}, {this._format(this.state.lookat[1])}, {this._format(this.state.lookat[2])}, {this._format(this.state.lookat[3])}|{'\n'}</Text>
              <Text>|{this._format(this.state.lookat[4])}, {this._format(this.state.lookat[5])}, {this._format(this.state.lookat[6])}, {this._format(this.state.lookat[7])}|{'\n'}</Text>
              <Text>|{this._format(this.state.lookat[8])}, {this._format(this.state.lookat[9])}, {this._format(this.state.lookat[10])}, {this._format(this.state.lookat[11])}|{'\n'}</Text>
              <Text>|{this._format(this.state.lookat[12])}, {this._format(this.state.lookat[13])}, {this._format(this.state.lookat[14])}, {this._format(this.state.lookat[15])}|{'\n'}</Text>
            </Text>
          </View>
        </LookatMatrixGestureHandler>
        <View style={styles.buttonPanel}>
          <Button    
            title={'Reset'}
            onPress={() => _cameraHandler.reset()}  />
          <Button 
            title={this.state.moveGesture === 'pan' ? 'Switch to Pinch' : 'Switch to Pan'}
            onPress={() => this.setState({moveGesture: this.state.moveGesture === 'pan' ? 'pinch' : 'pan'})} />
        </View>
      </View>
    );
  }
}

const styles = StyleSheet.create({
  container: {
    ...Platform.select({
      ios: {
        paddingTop: 20,
      },
      android: {}
    }),
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
    alignSelf: 'stretch',    
  },
  gestureContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    alignSelf: 'stretch',
    backgroundColor: '#C3C2C5',
  },
  helpText: {
    fontSize: 20,
    textAlign: 'center',
  },
  matrixText: Platform.select({
    ios: {
      fontSize: 15,
      fontFamily: 'Courier'
    },
    android: {
      fontSize: 20,
      fontFamily: 'monospace',  
    },
  }),
  buttonPanel: Platform.select({
    ios: {},
    android: {
      alignSelf: 'stretch',
      flexDirection: 'row',
      justifyContent: 'space-around',
      margin: 5,
    }
  }),
});

module.exports = LookatExample;
