import 'babel-polyfill';
import {
  StackNavigator,
} from 'react-navigation';

import {
  AppRegistry
} from 'react-native';

import HomeScreen from "./components/HomeScreen.js"
import VideoPlaybackScreen from "./components/VideoPlaybackScreen.js"

const rn3dtksample = StackNavigator({
  Home: { screen: HomeScreen },
  VideoPlayback: { screen: VideoPlaybackScreen },
},
{
  // Default config for all screens
  headerMode: 'none'
});

AppRegistry.registerComponent('rn3dtksample', () => rn3dtksample);