import {
  StackNavigator,
} from 'react-navigation';

import HomeScreen from './components/HomeScreen.js';
import VideoPlaybackScreen from './components/VideoPlaybackScreen.js';

export const rn3dtksample = StackNavigator({
  Home: { screen: HomeScreen },
  VideoPlayback: { screen: VideoPlaybackScreen },
});

