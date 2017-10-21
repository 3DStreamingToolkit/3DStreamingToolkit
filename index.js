import {
  StackNavigator,
} from 'react-navigation';

import {
  AppRegistry
} from 'react-native';

import HomeScreen from "./components/homeScreen.js"
import VideoPlaybackScreen from "./components/videoPlaybackScreen.js"

const rn3dtksample = StackNavigator({
  Home: { screen: HomeScreen },
  VideoPlayback: { screen: VideoPlaybackScreen },
});

AppRegistry.registerComponent('rn3dtksample', () => rn3dtksample);