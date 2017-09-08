import React, { Component } from 'react';

import {
  PanGestureHandler,
  PinchGestureHandler,
  State,
} from 'react-native-gesture-handler';

import {
  cloneMatrix,
  createIdentityMatrix,
  createTranslationMatrix,
  createRotateYMatrix,
  createRotateZMatrix,
  matrixMultiply,
} from './matrixMath';

const PropTypes = require('prop-types');

class SimpleCameraHandler extends Component {
  state = {
    lookat: createIdentityMatrix(),
    heading: 0.0,
    downHeading: 0.0,
    pitch: 0.0,
    downPitch: 0.0,
    location: [0.0, 0.0, 0.0],
    downLocation: [0.0, 0.0, 0.0]
  }

  static propTypes = {
    onLookatChanged: PropTypes.func,
  }

  _onPan = event => {
    let dx = event.nativeEvent.translationX;
    let dy = 0.0;//event.nativeEvent.translationY;

    // calc delta pitch & heading change
    let deltaPitch = 0.005 * dy;
    let deltaHeading = 0.005 * dx;
    let heading = this.state.downHeading - deltaHeading;
    let pitch = this.state.downPitch + deltaPitch;

    // calc delta rotation matrix
    let locationTransform = createTranslationMatrix(this.state.location)
    let headingTransform = createRotateYMatrix(heading);
    let pitchTransform = createRotateZMatrix(pitch);
    // `lookat` holds the result
    let rotateMatrix = matrixMultiply(headingTransform, pitchTransform);
    let lookat = matrixMultiply(locationTransform, rotateMatrix);

    this.setState({
      lookat: lookat,
      heading: heading,
      pitch: pitch,
    })

    this.props.onLookatChanged && this.props.onLookatChanged(lookat);
  }

  _onPinch = event => {
    let scale = event.nativeEvent.scale || 0.001;
    let dy = scale > 1 ? (scale - 1.0) : -((1.0 / scale) - 1.0);
    dy *= 100;

    // update the navigation origin
    let lookat = cloneMatrix(this.state.lookat);   
    let location = [0.0, 0.0, 0.0]; 
    location[0] = this.state.downLocation[0] + dy * lookat[0];
    location[1] = this.state.downLocation[1] + dy * lookat[1];
    location[2] = this.state.downLocation[2] + dy * lookat[2];

    lookat[12] = location[0];
    lookat[13] = location[1];
    lookat[14] = location[2];

    this.setState({
      lookat: lookat,
      location: location,
    });

    this.props.onLookatChanged && this.props.onLookatChanged(lookat);
  }

  _onStateChange = event => {
    if (event.nativeEvent.state === State.ACTIVE)
    {
      this.setState({
        downHeading: this.state.heading,
        downPitch: this.state.pitch,
        downLocation: this.state.location,
      });
    }
  }

  _format(n) {
    return (n >= 0 ? ' ' : '') + n.toFixed(3);
  }

  reset = () => {
    const lookat = createIdentityMatrix(); 
    this.setState({
      heading: 0.0,
      pitch: 0.0,
      lookat: lookat,
      location: [0.0, 0.0, 0.0]      
    });

    this.props.onLookatChanged && this.props.onLookatChanged(lookat);
  }

  render() {
    return (
      <PanGestureHandler
        onGestureEvent={this._onPan}
        onHandlerStateChange={this._onStateChange}
        minDist={10}
        minPointers={1}
        maxPointers={1}
        avgTouches>
        <PinchGestureHandler
          onGestureEvent={this._onPinch}
          onHandlerStateChange={this._onStateChange}
          avgTouches>
          {this.props.children}
        </PinchGestureHandler>
      </PanGestureHandler>
    );
  }
}

module.exports = SimpleCameraHandler;
