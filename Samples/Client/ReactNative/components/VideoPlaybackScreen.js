import React, { Component } from 'react';
import {
  StyleSheet,
  View,
  Dimensions
} from 'react-native';

import {
  createIdentityMatrix
} from './MatrixMath';

const LookatMatrixGestureHandler = require('./LookatMatrixGestureHandler');
const window = Dimensions.get('window');

var WebRTC = require('react-native-webrtc');
var {
  RTCView
} = WebRTC;

class VideoPlaybackScreen extends Component {
  static navigationOptions = ({ navigation }) => ({
    title: navigation.state.params.peerTitle
  });

    constructor(props) {
        super(props);

        this.state = {
          lookat: createIdentityMatrix(),
          moveGesture: 'pan',
          videoUrl: props.navigation.state.params.videoURL,
          sendInputData:props.navigation.state.params.sendInputData
        };
    }

    _onLookatChanged = matrix => {
      this.setState({
        lookat: matrix,
      });

      let eye = [matrix[12], matrix[13], matrix[14]];
      let lookat = [(matrix[12] + matrix[0]), (matrix[13] + matrix[1]), (matrix[14] + matrix[2])];
      let up = [matrix[4], matrix[5], matrix[6]];

      let json = { type: 'camera-transform-lookat',
      body: eye[0] + ',' + eye[1] + ',' + eye[2] + ',' +
            lookat[0] + ',' + lookat[1] + ',' + lookat[2] + ',' +
            up[0] + ',' + up[1] + ',' + up[2] };

      this.state.sendInputData(json);
    }

    render() {
      let _cameraHandler;
      const { navigate } = this.props.navigation;
      return (
        <LookatMatrixGestureHandler
        ref={(cameraHandler) => { _cameraHandler = cameraHandler; }}
        moveGesture={this.state.moveGesture}
        onLookatChanged={this._onLookatChanged}>
        <View style={styles.gestureContainer}>
          <RTCView streamURL={this.state.videoUrl} style={styles.remoteView}/>
        </View>
      </LookatMatrixGestureHandler>
      );
    }
  }

const styles = StyleSheet.create({
    remoteView: {
      height: window.height,
      width: window.width
    },
    gestureContainer: {
      flex: 1,
      justifyContent: 'center',
      alignItems: 'center',
      alignSelf: 'stretch',
      backgroundColor: '#C3C2C5',
    }
  });

export default VideoPlaybackScreen;
