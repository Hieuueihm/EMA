/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 */

import { StatusBar, StyleSheet, useColorScheme, View, Text } from 'react-native';
import {
  SafeAreaProvider,
  useSafeAreaInsets,
} from 'react-native-safe-area-context';
import HomeScreen from './src/screens/HomeScreen.js';
import WeatherScreen from './src/screens/WeatherScreen.js';
function App() {
  const isDarkMode = useColorScheme() === 'dark';


  return (
    <SafeAreaProvider>
      <StatusBar barStyle={isDarkMode ? 'light-content' : 'dark-content'} />
      <AppContent />
    </SafeAreaProvider>
  );
}

function AppContent() {
  const safeAreaInsets = useSafeAreaInsets();

  return (
    <View style={styles.container}>
      <WeatherScreen />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  }

});
export default App;
