import React from 'react';
import {
    Text, View, Image, Modal, Pressable, FlatList, TouchableOpacity, StyleSheet, Dimensions, TextInput
    , ImageBackground, Linking, ScrollView
} from "react-native"
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import { useState, useEffect } from "react"
import { COLORS, ROUTES, PROVINCE_MAPPING, PROVINCES_EN } from '../../constants';
import { useNavigation } from "@react-navigation/native";
import { SafeAreaView } from "react-native-safe-area-context";
import { tb, api } from '../api';
import messaging from '@react-native-firebase/messaging';
import firestore from '@react-native-firebase/firestore';

import {
    LineChart,
    BarChart,
    PieChart,
    ProgressChart,
    ContributionGraph,
    StackedBarChart
} from "react-native-chart-kit";

const { width, height } = Dimensions.get("window");
const fontSize = Math.min(width * 0.1, height * 0.08)
const colors = {
    bg: '#111418',
    card: '#1a1f26',
    cardAlt: '#171c22',
    text: '#e7eef7',
    sub: '#9fb0c3',
    accent: '#00d1ff',
    accent2: '#ffd166',
    good: '#1abc9c',
    warn: '#e67e22',
    danger: '#e74c3c',
    line: '#2b3340',
};


const ThemedLineChart = ({
    labels = [],
    data = [],
    unitSuffix = "",
    W = width - width * 0.1,
    H = height * 0.2,
    lineColor = "#ffa726"

}) => {

    return (
        <View style={[{ paddingTop: W * 0.01 }]}>
            <LineChart
                data={{
                    labels: labels,
                    datasets: [
                        {
                            data
                        }
                    ]
                }}
                style={{ paddingLeft: 0 }}
                width={W} // from react-native
                height={H}
                yAxisSuffix={unitSuffix}
                yAxisInterval={1}
                chartConfig={{
                    backgroundColor: colors.cardAlt,
                    backgroundGradientFrom: colors.cardAlt,
                    backgroundGradientTo: colors.cardAlt,
                    decimalPlaces: 1,
                    color: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,
                    labelColor: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,

                    style: {
                        borderRadius: 16,
                        paddingRight: 0, marginRight: 0
                    },
                    propsForDots: {
                        r: "1",
                        strokeWidth: "1",
                        stroke: lineColor
                    }
                }}
                bezier

            />
        </View>
    );
}

const TopMetric = ({ icon, label, value, unit, sub, threshold }) => {
    const isDanger = threshold !== undefined && value > threshold;

    return (
        <View
            style={[
                styles.topMetric,
                isDanger && { backgroundColor: "rgba(248, 49, 49, 0.2)" }, // đổi màu khi vượt ngưỡng
            ]}
        >
            <View style={styles.metricRow}>
                <FontAwesome6 name={icon} size={18} color={colors.accent} />
                <Text style={styles.metricLabel}>{label}</Text>
            </View>

            <Text style={styles.topValue}>
                {value}{" "}
                <Text style={{ fontSize: fontSize * 0.4, color: COLORS.white }}>
                    {unit}
                </Text>
            </Text>

            {sub ? <Text style={styles.metricSub}>{sub}</Text> : null}
        </View>
    );
};
const BottomTab = ({ unreadCount = 0, onPressCharts, onPressAlerts }) => (
    <View style={styles.bottomBar}>
        <TouchableOpacity style={styles.tabItem} onPress={onPressCharts}>
            <FontAwesome6 name="chart-line" size={fontSize * 0.4} color={colors.text} />
            <Text style={styles.tabText}>Charts</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.tabItem} onPress={onPressAlerts}>
            <View style={{ alignItems: 'center' }}>
                <FontAwesome6 name="bell" size={fontSize * 0.5} color={colors.text} />
                {unreadCount > 0 && (
                    <View style={styles.badge}>
                        <Text style={styles.badgeText}>
                            {unreadCount > 99 ? '99+' : String(unreadCount)}
                        </Text>
                    </View>
                )}
            </View>
            <Text style={styles.tabText}>Alerts</Text>
        </TouchableOpacity>
    </View>
);

const buildChartFromRaw = (series) => {
    if (!Array.isArray(series) || series.length === 0) {
        return { labels: [], data: [] };
    }
    const labels = [];
    const data = [];

    for (const p of series) {
        const ts = +p?.ts;
        const v = Number(p?.value)
        if (!Number.isFinite(ts) || !Number.isFinite(v)) continue;

        const d = new Date(ts);
        const hh = String(d.getHours()).padStart(2, "0");
        const mm = String(d.getMinutes()).padStart(2, "0");
        labels.push(`${hh}:${mm}`);
        data.push(v);
    }
    return { labels, data };
}
const normalizeLabel = (arr) => {
    if (!arr || arr.length < 2) return arr;

    const first = arr[0];
    const last = arr[arr.length - 1];

    const step = (arr.length - 1) / 3;
    const mid1 = arr[Math.round(step)];
    const mid2 = arr[Math.round(step * 2)];

    const result = Array(10).fill("");
    const positions = [0, 3, 6, 9];
    const labels = [first, mid1, mid2, last];

    positions.forEach((pos, i) => {
        result[pos] = labels[i];
    });

    return result;
}

const EnvDashboardScreen = () => {
    const navigation = useNavigation();

    const [city, setCity] = useState('Ha Noi');
    const [pickerVisible, setPickerVisible] = useState(false);
    const [cityNameFSearch, setCityNameFSearch] = useState('Hanoi');
    const [searchText, setSearchText] = useState("");
    const [telemetry, setTelemetry] = useState({});
    const [weather, setWeather] = useState({})
    const [telemetry12h, setTelemetry12h] = useState({});


    const [deviceToken, setDeviceToken] = useState(null);
    const [alerts, setAlerts] = useState([]);
    const [readsMap, setReadsMap] = useState({});
    const [unreadCount, setUnreadCount] = useState(0);

    // const

    const filteredProvinces = PROVINCES_EN.filter((item) =>
        item.toLowerCase().includes(searchText.toLowerCase())
    );
    useEffect(() => {
        let intervalId;

        async function fetchTelemetry() {
            try {
                const endTs = Date.now();
                const startTs = endTs - 60 * 1000;

                const bundle = await tb.getAssetsTelemetryByProvince(
                    "HoChiMinh",
                    ["temperature", "humidity", "co", "uv", "pm25", "pm10"],
                    { startTs, endTs, interval: 60000, agg: "AVG", limit: 1000 }
                );
                setTelemetry(bundle.length > 0 ? bundle[0].telemetry : {});
            } catch (err) {
                console.error("ThingsBoard test error:", err);
            }
        }

        async function fetchTelemetry12h() {
            try {
                const endTs = Date.now();
                const startTs = endTs - 12 * 60 * 60 * 1000;
                const interval = 30 * 60 * 1000;

                const bundle = await tb.getAssetsTelemetryByProvince(
                    "HoChiMinh",
                    ["temperature", "humidity"],
                    { startTs, endTs, interval, agg: "AVG", limit: 1000 }
                );
                setTelemetry12h(bundle.length > 0 ? bundle[0].telemetry : {});
            } catch (err) {
                console.error("ThingsBoard 12h error:", err);
            }
        }
        fetchTelemetry();
        fetchTelemetry12h();
        api.WeatherAPI.fetchWeatherForecast({ cityName: cityNameFSearch, days: '7' }).then(data => {
            setWeather(data)
        })


        intervalId = setInterval(fetchTelemetry, 60000);

        return () => clearInterval(intervalId);
    }, []);

    useEffect(() => {
        let mounted = true;
        (async () => {
            try {
                const t = await messaging().getToken();
                if (mounted) setDeviceToken(t);
            } catch (e) {
                console.log('Get FCM token error:', e);
            }
        })();
        return () => { mounted = false; };
    }, []);

    // Subscribe ALERTS (newest first)
    useEffect(() => {
        const unsub = firestore()
            .collection('alerts')
            .orderBy('createdAt', 'desc')
            .limit(200)
            .onSnapshot(snap => {
                const arr = [];
                snap.forEach(d => {
                    const v = d.data() || {};
                    arr.push({ id: d.id, title: v.title || 'Alert', body: v.body || '', createdAt: v.createdAt });
                });
                setAlerts(arr);
            }, err => console.log('alerts onSnapshot error:', err));
        return unsub;
    }, []);

    // Subscribe READS của thiết bị hiện tại
    useEffect(() => {
        if (!deviceToken) return;
        const unsub = firestore()
            .collection('fcmTokens').doc(deviceToken)
            .collection('reads')
            .onSnapshot(snap => {
                const map = {};
                snap.forEach(d => { map[d.id] = true; });
                setReadsMap(map);
            }, err => console.log('reads onSnapshot error:', err));
        return unsub;
    }, [deviceToken]);

    // Tính số chưa đọc
    useEffect(() => {
        if (!alerts.length) { setUnreadCount(0); return; }
        let c = 0;
        for (const a of alerts) if (!readsMap[a.id]) c++;
        setUnreadCount(c);
    }, [alerts, readsMap]);

    const onSelectProvince = (vnName) => {
        console.log(vnName)
        console.log(PROVINCE_MAPPING[vnName])
        setCity(vnName);
        setPickerVisible(false);
    };
    console.log("unreadCount:", unreadCount);


    function isEmptyObj(obj) {
        return obj && Object.keys(obj).length === 0 && obj.constructor === Object;
    }
    const LeftWeather = () => (
        <View style={styles.leftWeather}>
            <Image
                source={{ uri: `https:${current?.condition?.icon}` }}
                style={styles.leftWeatherIcon}
                resizeMode="contain"
            />
            <Text style={styles.leftWeatherText}>
                {current?.temp_c}
                {'\u2103'}
            </Text>
        </View>
    );
    const current = weather?.current;
    const temp_array_12h = telemetry12h?.temperature ?? [];
    const humidity_array_12h = telemetry12h?.humidity ?? [];




    const { labels: chartTempLabels, data: chartTempData } = buildChartFromRaw(temp_array_12h);
    const { labels: chartHumLabels, data: chartHumData } = buildChartFromRaw(humidity_array_12h);


    const chartTempLabelsNormalized = normalizeLabel(chartTempLabels)
    const chartHumLabelsNormalized = normalizeLabel(chartHumLabels)

    return (
        <View style={styles.container}>

            {/* Header */}

            <View style={styles.header}>
                <TouchableOpacity onPress={() => navigation.navigate(ROUTES.WHEATHER_SCREEN)}>
                    <LeftWeather />
                </TouchableOpacity>
                <View style={styles.centerBox}>
                    <TouchableOpacity onPress={() => setPickerVisible(true)} style={{ flexDirection: 'row', alignItems: 'center' }}>
                        <Text style={styles.brand}>{city}</Text>
                        <FontAwesome6
                            name="chevron-down"
                            size={fontSize * 0.3}
                            color="#fff"
                            style={{ marginLeft: width * 0.01 }}
                        />
                    </TouchableOpacity>

                    <Text style={styles.location}>
                        {new Date().toLocaleDateString('en-VN', {
                            weekday: 'long',
                            day: '2-digit',
                            month: '2-digit',
                            year: 'numeric',
                        })}
                    </Text>
                </View>
                <TouchableOpacity onPress={() => navigation.navigate(ROUTES.MAP_SCREEN)} >
                    <FontAwesome6 name="map" size={22} color="#fff" style={{ marginRight: 12 }} />
                </TouchableOpacity>

                <Modal visible={pickerVisible} animationType="slide" transparent onRequestClose={() => setPickerVisible(false)}>
                    <Pressable style={{ flex: 1, backgroundColor: 'rgba(0,0,0,0.35)' }} onPress={() => setPickerVisible(false)}>
                        <View style={{
                            marginTop: 'auto',
                            backgroundColor: '#111827',
                            borderTopLeftRadius: 16,
                            borderTopRightRadius: 16,
                            paddingHorizontal: width * 0.02,
                            paddingVertical: height * 0.01,
                            maxHeight: '70%',
                        }}>
                            <Text style={{ color: '#fff', fontSize: fontSize * 0.3, fontWeight: '600', marginBottom: height * 0.01 }}>Choose provinces</Text>
                            <TextInput
                                placeholder="Search..."
                                placeholderTextColor="#9CA3AF"
                                value={searchText}
                                onChangeText={setSearchText}
                                style={{
                                    backgroundColor: "#1f2937",
                                    color: "#fff",
                                    paddingHorizontal: width * 0.01,
                                    paddingVertical: height * 0.01,
                                    borderRadius: 8,
                                    marginBottom: height * 0.012,
                                }}
                            />
                            <FlatList
                                data={filteredProvinces}
                                keyExtractor={(item) => item}
                                renderItem={({ item }) => (
                                    <TouchableOpacity onPress={() => onSelectProvince(item)} style={{ paddingVertical: height * 0.012 }}>
                                        <Text style={{ color: "#fff", fontSize: fontSize * 0.32 }}>{item}</Text>
                                    </TouchableOpacity>
                                )}
                                ItemSeparatorComponent={() => <View style={{ height: 1, backgroundColor: "#1f2937" }} />}
                            />
                        </View>
                    </Pressable>
                </Modal>

            </View>

            <ScrollView contentContainerStyle={{ paddingBottom: height * 0.1 }}>

                {/* Top metrics row */}
                <View style={styles.topRow}>
                    <TopMetric
                        icon="thermometer"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.temperature[0].value).toFixed(1)}
                        unit="°C"
                        sub="Temperature"
                        threshold={30}
                    />
                    <TopMetric
                        icon="droplet"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.humidity[0].value).toFixed(1)}
                        unit="%"
                        sub="Humidity"
                    />
                    <TopMetric
                        icon="cloud"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.co[0].value).toFixed(1)}
                        unit="ppm"
                        sub="CO"
                    />
                </View>
                <View style={styles.topRow}>
                    <TopMetric
                        icon="sun"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.uv[0].value).toFixed(1)}
                        unit=""
                        sub="UV Index"
                    />
                    <TopMetric
                        icon="wind"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.pm25[0].value).toFixed(1)}
                        unit="µg/m³"
                        sub="PM 2.5"
                    />
                    <TopMetric
                        icon="wind"
                        label=""
                        value={isEmptyObj(telemetry) ? NaN : Number(telemetry.pm10[0].value).toFixed(1)}
                        unit="µg/m³"
                        sub="PM 10"
                    />
                </View>

                {/* Big timeline-like graph mock */}
                <View style={styles.timelineCard}>
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Temperature trend</Text>
                    </View>
                    {chartTempData.length > 0 && (
                        <ThemedLineChart
                            labels={chartTempLabelsNormalized}
                            data={chartTempData}
                            unitSuffix="°C"
                            bgColor={colors.cardAlt}
                            lineColor="#f97316" // cam cho temperature
                        />
                    )}


                </View>

                <View style={styles.timelineCard}>
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Humidity trend</Text>
                    </View>
                    {chartHumData.length > 0 && (
                        <ThemedLineChart
                            labels={chartHumLabelsNormalized}
                            data={chartHumData}
                            unitSuffix="%"
                            bgColor={colors.cardAlt}
                            lineColor="#22c55e" // xanh cho humidity
                        />
                    )}

                </View>




            </ScrollView>

            {/* Bottom bar */}
            <BottomTab
                unreadCount={unreadCount}
                onPressCharts={() => {/* đang ở màn này rồi, có thể scroll top hoặc bỏ trống */ }}
                onPressAlerts={() => navigation.navigate(ROUTES.ALERTS_SCREEN)}
            /> </View >
    );
};

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: colors.bg, paddingHorizontal: width * 0.01 },
    header: {
        height: height * 0.08,
        paddingHorizontal: width * 0.02,
        flexDirection: 'row',
        alignItems: 'center',
        justifyContent: 'space-between',
        borderBottomWidth: StyleSheet.hairlineWidth,
        borderBottomColor: colors.line,
    },
    centerBox: {
        position: 'absolute',
        left: 0,
        right: 0,
        alignItems: 'center',
        justifyContent: 'center',
    },
    leftWeather: {
        flexDirection: 'row',
        alignItems: 'center',
    },
    leftWeatherIcon: {
        width: width * 0.08,
        height: height * 0.05,
        marginRight: width * 0.01,
    },
    leftWeatherText: {
        fontSize: fontSize * 0.4,
        color: '#ad5c51ff',
        fontWeight: 'bold',
    },
    brand: { color: colors.text, fontSize: fontSize * 0.4, opacity: 0.85, letterSpacing: 1 },
    location: { color: colors.text, fontSize: fontSize * 0.3, fontWeight: '700' },

    topRow: { flexDirection: 'row', marginTop: height * 0.005, paddingHorizontal: width * 0.03, paddingVertical: height * 0.008, gap: height * 0.01 },
    topMetric: {
        flex: 1,
        backgroundColor: colors.card,
        borderRadius: 12,
        paddingHorizontal: width * 0.03,
        paddingVertical: height * 0.018,
        borderWidth: 1,
        borderColor: colors.line,
    },

    aqiShadow: {
        shadowColor: '#000', shadowOffset: { width: 0, height: 2 },
        shadowOpacity: 0.25, shadowRadius: 3.84, elevation: 6,
    },
    metricRow: { flexDirection: 'row', alignItems: 'center', marginBottom: height * 0.005, gap: height * 0.005 },
    metricLabel: { color: colors.sub, fontSize: fontSize * 0.2 },
    topValue: { color: colors.text, fontSize: fontSize * 0.5, fontWeight: '700' },
    metricSub: { color: colors.sub, fontSize: fontSize * 0.3, marginTop: 2 },

    timelineCard: {
        marginHorizontal: width * 0.02,
        marginTop: height * 0.01,
        paddingHorizontal: width * 0.01,

        paddingVertical: height * 0.01,
        backgroundColor: colors.cardAlt,
        borderRadius: 12,
        borderWidth: 1,
        borderColor: colors.line,
    },
    timelineTitle: { color: colors.sub, fontSize: fontSize * 0.3, marginBottom: height * 0.01 },
    sparkline: { flexDirection: 'row', alignItems: 'flex-end' },
    timelineLegend: { flexDirection: 'row', alignItems: 'center', marginTop: height * 0.01 },
    legendDot: { width: width * 0.02, height: height * 0.012, borderRadius: 5, backgroundColor: colors.accent, marginLeft: width * 0.01, marginRight: width * 0.01 },
    legendText: { color: colors.sub, fontSize: fontSize * 0.3 },



    bottomBar: {
        position: 'absolute',
        left: 0, right: 0, bottom: 0,
        height: height * 0.08,
        backgroundColor: colors.cardAlt,
        borderTopWidth: 1,
        borderTopColor: colors.line,
        flexDirection: 'row',
        justifyContent: 'space-around',
        alignItems: 'center',
    },
    tabItem: { alignItems: 'center', gap: height * 0.005 },
    tabText: { color: colors.text, fontSize: fontSize * 0.3 },
    badge: {
        position: 'absolute',
        top: -height * 0.01,
        right: -width * 0.03,
        minWidth: width * 0.05,
        height: height * 0.02,
        paddingHorizontal: width * 0.01,
        backgroundColor: colors.danger,  // đỏ
        borderRadius: 9,
        alignItems: 'center',
        justifyContent: 'center',
        borderWidth: 1,
        borderColor: colors.cardAlt,
    },
    badgeText: {
        color: '#fff',
        fontSize: fontSize * 0.25,
        fontWeight: '700',
    },
});

export default EnvDashboardScreen;
