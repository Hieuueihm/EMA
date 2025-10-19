import React, { useRef, useEffect, useState } from 'react';
import { StyleSheet, View, TouchableOpacity, Dimensions, PermissionsAndroid, Platform, Alert, Linking } from 'react-native';
import MapView, { Marker } from 'react-native-maps';
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import { useNavigation } from '@react-navigation/native';
import { COLORS, ROUTES } from '../../constants';
import Geolocation from 'react-native-geolocation-service';
import { tb } from '../api';


const { width, height } = Dimensions.get("window");
const fontSize = Math.min(width * 0.1, height * 0.08)
export default function MapScreen() {
    const mapRef = useRef(null);
    const navigation = useNavigation();
    const [markers, setMarkers] = useState([]);

    useEffect(() => {
        const ensurePermission = async () => {
            if (Platform.OS !== 'android') {
                Geolocation.requestAuthorization?.('whenInUse');
                return 'granted';
            }

            const fine = PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION;
            const coarse = PermissionsAndroid.PERMISSIONS.ACCESS_COARSE_LOCATION;

            const hasFine = await PermissionsAndroid.check(fine);
            const hasCoarse = await PermissionsAndroid.check(coarse);
            if (hasFine || hasCoarse) return hasFine ? 'fine' : 'coarse';
            const res = await PermissionsAndroid.requestMultiple([fine, coarse]);

            const fineRes = res[fine];
            const coarseRes = res[coarse];

            if (
                fineRes === PermissionsAndroid.RESULTS.NEVER_ASK_AGAIN ||
                coarseRes === PermissionsAndroid.RESULTS.NEVER_ASK_AGAIN
            ) {
                Alert.alert(
                    'Cần cấp quyền vị trí',
                    'Bạn đã tắt hỏi lại. Hãy mở phần Cài đặt để cấp quyền vị trí cho ứng dụng.',
                    [
                        { text: 'Huỷ' },
                        { text: 'Mở cài đặt', onPress: () => Linking.openSettings() },
                    ]
                );
                return 'denied';
            }

            if (fineRes === PermissionsAndroid.RESULTS.GRANTED) return 'fine';
            if (coarseRes === PermissionsAndroid.RESULTS.GRANTED) return 'coarse';

            Alert.alert('Quyền vị trí bị từ chối');
            return 'denied';
        };

        let cancelled = false;

        const locate = async () => {
            const level = await ensurePermission();
            if (level === 'denied') return;

            const high = level === 'fine';

            requestAnimationFrame(() => {
                if (cancelled) return;

                Geolocation.getCurrentPosition(
                    pos => {
                        if (cancelled) return;
                        const { latitude, longitude } = pos.coords;
                        mapRef.current?.animateCamera(
                            {
                                center: { latitude, longitude },
                                pitch: 0,
                                heading: 0,
                            },
                            { duration: 800 }
                        );

                    },
                    err => {
                        console.warn('Geo error:', err);
                        Alert.alert('Không lấy được vị trí hiện tại');
                    },
                    {
                        enableHighAccuracy: high,
                        timeout: 15000,
                        maximumAge: 10000,

                        forceLocationManager: true,

                        showLocationDialog: true,
                    }
                );
            });
        };
        const getAllDevicesAttributes = async () => {
            try {
                const devicesSharedOnly = await tb.getAllDevicesWithAttributes();
                const withCoords = devicesSharedOnly.filter(x => x.attributes.lat != null && x.attributes.lon != null);
                // console.log("Devices with attributes:", devicesSharedOnly);

                // console.log("Devices with attributes after filter:", withCoords);
                setMarkers(withCoords)
            } catch (e) {
                console.error("Error fetching device attributes:", e);
            }
        };

        locate();
        getAllDevicesAttributes();

        return () => {
            cancelled = true;
        };
    }, []);

    console.log(markers)

    return (
        <View style={styles.container}>
            <MapView
                ref={mapRef}
                style={StyleSheet.absoluteFill}
                showsUserLocation
                showsMyLocationButton
                initialRegion={{
                    latitude: 16.2,
                    longitude: 107.3,
                    latitudeDelta: 18,
                    longitudeDelta: 11,
                }}
            >
                {markers.map(m => (
                    <Marker
                        key={m?.device?.id?.id}
                        coordinate={{ latitude: m?.attributes?.lat, longitude: m?.attributes?.lon }}
                        title={m?.device?.name}
                        description={`Buzzer: ${m?.attributes?.buzzer == true ? 'ON' : 'OFF'}`}
                        pinColor={m?.attributes?.buzzer == true ? 'red' : 'green'}
                    />
                ))}
            </MapView>

            <TouchableOpacity
                onPress={() => navigation.navigate(ROUTES.HOME_SCREEN)}
                style={{
                    position: 'absolute',
                    top: height * 0.01,
                    left: width * 0.02,
                    padding: 8,
                    borderRadius: 20,
                }}
            >
                <FontAwesome6
                    name="angle-left"
                    style={{
                        fontSize: fontSize * 0.7,
                        color: COLORS.black,
                    }}
                />
            </TouchableOpacity>

        </View>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1 },
    backButton: {
        position: 'absolute',
        top: 40,   // đẩy xuống một chút cho khỏi đụng status bar
        left: 20,
        backgroundColor: 'white',
        padding: 8,
        borderRadius: 20,
        elevation: 3,          // Android shadow
        shadowColor: '#000',   // iOS shadow
        shadowOpacity: 0.3,
        shadowOffset: { width: 0, height: 2 },
        shadowRadius: 4,
    },
});
