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

import React, { useEffect } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { ROUTES } from './constants/routes.js';
import EnvDashboardScreen from './src/screens/EnvDashboardScreen.js'
import MapScreen from './src/screens/MapScreen.js'
import WeatherScreen from './src/screens/WeatherScreen.js';
import { PermissionsAndroid } from 'react-native';
import messaging from '@react-native-firebase/messaging';
import { Platform } from 'react-native';
import firestore from '@react-native-firebase/firestore';
import AlertsScreen from './src/screens/AlertsScreen.js';
import ChartsScreen from './src/screens/ChartsScreen.js';
const Stack = createNativeStackNavigator();

const notificationsTopic = `notifications_${Platform.OS}`;


async function requestNotificationPermission(): Promise<boolean> {
  if (Platform.OS === 'android') {
    const is13plus = (Platform.Version as number) >= 33;
    if (!is13plus) return true;

    const result = await PermissionsAndroid.request(
      PermissionsAndroid.PERMISSIONS.POST_NOTIFICATIONS
    );
    const granted = result === PermissionsAndroid.RESULTS.GRANTED;
    console.log(granted ? 'Notification permission granted' : 'Notification permission denied');
    return granted;
  }

  const status = await messaging().requestPermission();
  const granted =
    status === messaging.AuthorizationStatus.AUTHORIZED ||
    status === messaging.AuthorizationStatus.PROVISIONAL;
  console.log(granted ? 'Notification permission granted (iOS)' : 'Notification permission denied (iOS)');
  return granted;
}

async function saveTokenToFirestore(token: string) {
  const docId = token;
  await firestore().collection('fcmTokens').doc(docId).set(
    {
      token,
      platform: Platform.OS,
      app: 'EMA-mobile',
      createdAt: firestore.FieldValue.serverTimestamp(),
      lastSeenAt: firestore.FieldValue.serverTimestamp(),
    },
    { merge: true },
  );
  return docId;
}

async function registerToken() {
  const enabled = await requestNotificationPermission();
  if (!enabled) return null;

  try {

    const token = await messaging().getToken();
    // console.log('FCM token:', token);

    await saveTokenToFirestore(token);
    await messaging().subscribeToTopic(notificationsTopic);

    return token;
  } catch (err) {
    console.error('Failed to get/save FCM token:', err);
    return null;
  }
}
function App() {
  const isDarkMode = useColorScheme() === 'dark';



  useEffect(() => {
    let unsubTokenRefresh: (() => void) | undefined;

    (async () => {
      const token = await registerToken();

      // Lắng nghe token refresh
      unsubTokenRefresh = messaging().onTokenRefresh(async (newToken) => {
        console.log('FCM token refreshed:', newToken);
        await saveTokenToFirestore(newToken);
        await messaging().subscribeToTopic(notificationsTopic);
      });
    })();

    return () => {
      if (unsubTokenRefresh) unsubTokenRefresh();
    };
  }, []);

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
    <NavigationContainer>
      <Stack.Navigator screenOptions={{ headerShown: false }}>
        <Stack.Screen name={ROUTES.HOME_SCREEN} component={EnvDashboardScreen} />

        <Stack.Screen name={ROUTES.WHEATHER_SCREEN} component={WeatherScreen} />
        <Stack.Screen name={ROUTES.MAP_SCREEN} component={MapScreen} />
        <Stack.Screen name={ROUTES.ALERTS_SCREEN} component={AlertsScreen} />
        <Stack.Screen name={ROUTES.CHARTS_SCREEN} component={ChartsScreen} />

      </Stack.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  }

});
export default App;
