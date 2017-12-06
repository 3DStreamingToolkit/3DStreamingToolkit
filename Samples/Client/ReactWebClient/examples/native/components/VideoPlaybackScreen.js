import { Component } from 'react';
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

      let json = { type: 'camera-transform-lookat',
      body: (-matrix[12]) + ',' + (-matrix[13]) + ',' + (-matrix[14]) + ',' +
            (-matrix[0]) + ',' + (-matrix[1]) + ',' + (matrix[2]) + ',' +
            (matrix[4]) + ',' + matrix[5] + ',' + (matrix[6]) };

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
