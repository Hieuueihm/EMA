// screens/EnvDashboardScreen.js
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
    Text, View, Image, Modal, Pressable, FlatList, TouchableOpacity, StyleSheet, Dimensions,
    TextInput, ScrollView, ActivityIndicator, Platform
} from 'react-native';
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import { useNavigation, useFocusEffect } from "@react-navigation/native";
import { SafeAreaView } from "react-native-safe-area-context";
import messaging from '@react-native-firebase/messaging';
import firestore from '@react-native-firebase/firestore';

import { COLORS, ROUTES, PROVINCE_MAPPING, PROVINCES_EN } from '../../constants';
import { PROVINCE_MAPPING_WEATHERAPI } from '../../constants/provinces';
import { getItem, storeItem } from '../utils/AsyncStorage';
import { tb, api } from '../api';

import { LineChart } from "react-native-chart-kit";

const { width, height } = Dimensions.get("window");
const fontSize = Math.min(width * 0.1, height * 0.08);

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


const downsample = (arr, maxPoints) => {
    if (!Array.isArray(arr) || arr.length <= maxPoints) return arr || [];
    const step = Math.ceil(arr.length / maxPoints);
    const out = [];
    for (let i = 0; i < arr.length; i += step) out.push(arr[i]);
    return out;
};


const buildChartFromRaw = (series, { limit = 60, decimals = 1 } = {}) => {
    if (!Array.isArray(series)) return { labels: [], data: [] };
    const cleaned = series
        .map(p => ({ ts: +p?.ts, value: Number(p?.value) }))
        .filter(p => Number.isFinite(p.ts) && Number.isFinite(p.value))
        .sort((a, b) => a.ts - b.ts);

    const sliced = downsample(cleaned, limit);

    const labels = [];
    const data = [];
    for (const p of sliced) {
        const d = new Date(p.ts);
        const hh = String(d.getHours()).padStart(2, "0");
        const mm = String(d.getMinutes()).padStart(2, "0");
        labels.push(`${hh}:${mm}`);
        data.push(Number(p.value.toFixed(decimals)));
    }
    return { labels, data };
};

const normalizeLabel = (arr) => {
    if (!arr || arr.length < 2) return arr || [];
    const first = arr[0];
    const last = arr[arr.length - 1];
    const mid1 = arr[Math.round((arr.length - 1) / 3)];
    const mid2 = arr[Math.round((arr.length - 1) * 2 / 3)];
    const result = Array(10).fill("");
    const positions = [0, 3, 6, 9];
    const labels = [first, mid1, mid2, last];
    positions.forEach((pos, i) => { result[pos] = labels[i] || ""; });
    return result;
};

const formatNumber = (v, digits = 1) => {
    const n = Number(v);
    if (!Number.isFinite(n)) return '--';
    return n.toFixed(digits);
};

const ThemedLineChart = React.memo(function ThemedLineChart({
    labels = [],
    data = [],
    unit = "",
    W = width - width * 0.1,
    H = height * 0.22,
    lineColor = "#ffa726",
    decimals = 1,
}) {
    const chartConfig = useMemo(() => ({
        backgroundColor: colors.cardAlt,
        backgroundGradientFrom: colors.cardAlt,
        backgroundGradientTo: colors.cardAlt,
        decimalPlaces: decimals, // lib vẫn dùng, nhưng ta override label
        color: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,
        labelColor: (opacity = 1) => `rgba(255, 255, 255, ${opacity})`,
        style: { borderRadius: 16, paddingRight: 0, marginRight: 0 },
        propsForDots: { r: "1", strokeWidth: "1", stroke: lineColor },
    }), [decimals, lineColor]);

    const formatY = useCallback((val) => {
        const n = Number(val);
        if (!Number.isFinite(n)) return '';
        const fixed = n.toFixed(decimals);
        return unit ? `${fixed} ${unit}` : fixed;
    }, [unit, decimals]);

    return (
        <View style={{ paddingTop: W * 0.01 }}>
            <LineChart
                data={{ labels, datasets: [{ data }] }}
                style={{ paddingLeft: 0 }}
                width={W}
                height={H}
                formatYLabel={formatY}
                withInnerLines={false}
                withOuterLines={true}
                withShadow={false}
                withHorizontalLabels
                withVerticalLabels={labels.length <= 10}
                segments={4}
                chartConfig={chartConfig}
                bezier
            />
        </View>
    );
}, (prev, next) => {
    const sameLen = prev.labels.length === next.labels.length && prev.data.length === next.data.length;
    const sameTail =
        prev.labels[prev.labels.length - 1] === next.labels[next.labels.length - 1] &&
        prev.data[prev.data.length - 1] === next.data[next.data.length - 1];
    return sameLen && sameTail && prev.unit === next.unit && prev.decimals === next.decimals;
});

const TopMetric = React.memo(function TopMetric({ icon, label, value, unit, sub, threshold }) {
    const numeric = Number(value);
    const isDanger = Number.isFinite(numeric) && threshold !== undefined && numeric > threshold;

    return (
        <View style={[styles.topMetric, isDanger && { backgroundColor: "rgba(248, 49, 49, 0.18)" }]}>
            <View style={styles.metricRow}>
                <FontAwesome6 name={icon} size={18} color={colors.accent} />
                {!!label && <Text style={styles.metricLabel}>{label}</Text>}
            </View>

            <Text style={styles.topValue}>
                {Number.isFinite(numeric) ? numeric : '--'}{' '}
                <Text style={{ fontSize: fontSize * 0.4, color: COLORS.white }}>
                    {unit}
                </Text>
            </Text>

            {sub ? <Text style={styles.metricSub}>{sub}</Text> : null}
        </View>
    );
});

const EnvDashboardScreen = () => {
    const navigation = useNavigation();

    const [city, setCity] = useState('');
    const [pickerVisible, setPickerVisible] = useState(false);
    const [searchText, setSearchText] = useState("");

    const [cityNameFSearch, setCityNameFSearch] = useState('');
    const [cityNameFSearchWeatherAPI, setCityNameFSearchWeatherAPI] = useState('');

    const [telemetry, setTelemetry] = useState({});
    const [telemetry12h, setTelemetry12h] = useState({});
    const [weather, setWeather] = useState({});
    const [loadingBlock, setLoadingBlock] = useState(false);

    const [deviceToken, setDeviceToken] = useState(null);
    const [alerts, setAlerts] = useState([]);
    const [readsMap, setReadsMap] = useState({});
    const [unreadCount, setUnreadCount] = useState(0);

    const pollingRef = useRef(null);
    const mountedRef = useRef(true);


    useEffect(() => {
        mountedRef.current = true;
        (async () => {
            try {
                const stored = await getItem('selected_province');
                const vnName = stored || 'Ha Noi';
                setCity(vnName);
                setCityNameFSearch(PROVINCE_MAPPING[vnName] || 'HaNoi');
                setCityNameFSearchWeatherAPI(PROVINCE_MAPPING_WEATHERAPI[vnName] || 'Hanoi');
            } catch {
                setCity('Ha Noi');
                setCityNameFSearch('HaNoi');
                setCityNameFSearchWeatherAPI('Hanoi');
            }
        })();
        return () => { mountedRef.current = false; };
    }, []);

    const fetchTelemetry = useCallback(async (provinceName) => {
        const endTs = Date.now();
        const startTs = endTs - 60 * 1000;
        const keys = ["temperature", "humidity", "co", "uv", "pm25", "pm10"];

        const bundle = await tb.getAssetsTelemetryByProvince(
            provinceName,
            keys,
            { startTs, endTs, interval: 60000, agg: "AVG", limit: 1000 }
        );
        return bundle.length > 0 ? bundle[0].telemetry : {};
    }, []);

    const fetchTelemetry12h = useCallback(async (provinceName) => {
        const endTs = Date.now();
        const startTs = endTs - 12 * 60 * 60 * 1000;
        const interval = 30 * 60 * 1000;

        const bundle = await tb.getAssetsTelemetryByProvince(
            provinceName,
            ["temperature", "humidity"],
            { startTs, endTs, interval, agg: "AVG", limit: 2000 }
        );
        console.log("12h telemetry fetch:", bundle[0].telemetry);
        return bundle.length > 0 ? bundle[0].telemetry : {};
    }, []);

    const fetchWeatherForecastF = useCallback(async (cityName) => {
        if (!cityName) return {};
        const data = await api.WeatherAPI.fetchWeatherForecast({ cityName, days: 1 });
        return data;
    }, []);

    const runFetchAll = useCallback(async (provinceName, weatherCityName) => {
        setLoadingBlock(true);
        try {
            const [tNow, t12h, weatherRes] = await Promise.allSettled([
                fetchTelemetry(provinceName),
                fetchTelemetry12h(provinceName),
                fetchWeatherForecastF(weatherCityName),
            ]);

            if (!mountedRef.current) return;

            if (tNow.status === 'fulfilled') setTelemetry(tNow.value || {});
            if (t12h.status === 'fulfilled') setTelemetry12h(t12h.value || {});
            if (weatherRes.status === 'fulfilled') setWeather(weatherRes.value || {});
        } catch (e) {
        } finally {
            if (mountedRef.current) setLoadingBlock(false);
        }
    }, [fetchTelemetry, fetchTelemetry12h, fetchWeatherForecastF]);

    useEffect(() => {
        if (!cityNameFSearch) return;
        runFetchAll(cityNameFSearch, cityNameFSearchWeatherAPI);

        if (pollingRef.current) {
            clearInterval(pollingRef.current);
            pollingRef.current = null;
        }
        pollingRef.current = setInterval(() => {
            runFetchAll(cityNameFSearch, cityNameFSearchWeatherAPI);
        }, 60000);

        return () => {
            if (pollingRef.current) {
                clearInterval(pollingRef.current);
                pollingRef.current = null;
            }
        };
    }, [cityNameFSearch, cityNameFSearchWeatherAPI, runFetchAll]);

    useFocusEffect(
        useCallback(() => {
            return () => {
                if (pollingRef.current) {
                    clearInterval(pollingRef.current);
                    pollingRef.current = null;
                }
            };
        }, [])
    );

    // FCM token
    useEffect(() => {
        let alive = true;
        (async () => {
            try {
                const t = await messaging().getToken();
                if (alive) setDeviceToken(t);
            } catch { }
        })();
        return () => { alive = false; };
    }, []);

    // Sub alerts
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
            }, () => { });
        return unsub;
    }, []);

    // Sub reads
    useEffect(() => {
        if (!deviceToken) return;
        const unsub = firestore()
            .collection('fcmTokens').doc(deviceToken)
            .collection('reads')
            .onSnapshot(snap => {
                const map = {};
                snap.forEach(d => { map[d.id] = true; });
                setReadsMap(map);
            }, () => { });
        return unsub;
    }, [deviceToken]);

    // unread
    useEffect(() => {
        if (!alerts.length) { setUnreadCount(0); return; }
        let c = 0;
        for (const a of alerts) if (!readsMap[a.id]) c++;
        setUnreadCount(c);
    }, [alerts, readsMap]);

    const filteredProvinces = useMemo(() => {
        const q = (searchText || '').toLowerCase().trim();
        if (!q) return PROVINCES_EN;
        return PROVINCES_EN.filter(item => item.toLowerCase().includes(q));
    }, [searchText]);

    const onSelectProvince = useCallback(async (vnName) => {
        await storeItem('selected_province', vnName);
        setCity(vnName);
        setCityNameFSearch(PROVINCE_MAPPING[vnName] || 'HaNoi');
        setCityNameFSearchWeatherAPI(PROVINCE_MAPPING_WEATHERAPI[vnName] || 'Hanoi');
        setPickerVisible(false);
    }, []);

    const current = weather?.current;
    const safeTemp = telemetry?.temperature?.[0]?.value;
    const safeHum = telemetry?.humidity?.[0]?.value;
    const safeCO = telemetry?.co?.[0]?.value;
    const safeUV = telemetry?.uv?.[0]?.value;
    const safePM25 = telemetry?.pm25?.[0]?.value;
    const safePM10 = telemetry?.pm10?.[0]?.value;

    // build charts ( ưu tiên data thực, fallback sample )
    const tempArray12h = telemetry12h?.temperature ?? [];
    const humArray12h = telemetry12h?.humidity ?? [];

    const tempChart = useMemo(() => buildChartFromRaw(tempArray12h, { limit: 60, decimals: 1 }), [tempArray12h]);
    const humChart = useMemo(() => buildChartFromRaw(humArray12h, { limit: 60, decimals: 1 }), [humArray12h]);

    const tempLabelsNorm = useMemo(() => normalizeLabel(tempChart.labels), [tempChart.labels]);
    const humLabelsNorm = useMemo(() => normalizeLabel(humChart.labels), [humChart.labels]);

    const onChangeSearch = useCallback((t) => setSearchText(t), []);

    return (
        <SafeAreaView style={styles.container}>
            {/* Header */}
            <View style={styles.header}>
                <TouchableOpacity onPress={() => navigation.navigate(ROUTES.WHEATHER_SCREEN)}>
                    <View style={styles.leftWeather}>
                        {current?.condition?.icon ? (
                            <Image source={{ uri: `https:${current?.condition?.icon}` }} style={styles.leftWeatherIcon} resizeMode="contain" />
                        ) : null}
                        <Text style={styles.leftWeatherText}>
                            {Number.isFinite(Number(current?.temp_c)) ? current?.temp_c : '--'}
                            {'\u2103'}
                        </Text>
                    </View>
                </TouchableOpacity>

                <View style={styles.centerBox}>
                    <TouchableOpacity onPress={() => setPickerVisible(true)} style={{ flexDirection: 'row', alignItems: 'center' }}>
                        <Text style={styles.brand}>{city || 'Chọn tỉnh'}</Text>
                        <FontAwesome6 name="chevron-down" size={fontSize * 0.3} color="#fff" style={{ marginLeft: width * 0.01 }} />
                    </TouchableOpacity>

                    <Text style={styles.location}>
                        {new Date().toLocaleDateString('vi-VN', {
                            weekday: 'long', day: '2-digit', month: '2-digit', year: 'numeric',
                        })}
                    </Text>
                </View>

                <TouchableOpacity onPress={() => navigation.navigate(ROUTES.MAP_SCREEN)} >
                    <FontAwesome6 name="map" size={22} color="#fff" style={{ marginRight: 12 }} />
                </TouchableOpacity>

                <Modal visible={pickerVisible} animationType="slide" transparent onRequestClose={() => setPickerVisible(false)}>
                    <Pressable style={{ flex: 1, backgroundColor: 'rgba(0,0,0,0.35)' }} onPress={() => setPickerVisible(false)}>
                        <View style={styles.provinceSheet}>
                            <Text style={styles.sheetTitle}>Choose provinces</Text>
                            <TextInput
                                placeholder="Search..."
                                placeholderTextColor="#9CA3AF"
                                value={searchText}
                                onChangeText={onChangeSearch}
                                style={styles.searchInput}
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
                                initialNumToRender={20}
                                windowSize={8}
                                removeClippedSubviews
                            />
                        </View>
                    </Pressable>
                </Modal>
            </View>

            <ScrollView contentContainerStyle={{ paddingBottom: height * 0.1 }} keyboardShouldPersistTaps="handled">
                {loadingBlock ? (
                    <View style={{ paddingVertical: 8, alignItems: 'center' }}>
                        <ActivityIndicator color="#fff" />
                    </View>
                ) : null}

                {/* Top metrics row */}
                <View style={styles.topRow}>
                    <TopMetric
                        icon="thermometer"
                        label=""
                        value={formatNumber(safeTemp, 1)}
                        unit="°C"
                        sub="Nhiệt độ"
                        threshold={40}
                    />
                    <TopMetric
                        icon="droplet"
                        label=""
                        value={formatNumber(safeHum, 1)}
                        unit="%"
                        sub="Độ ẩm"
                    />
                    <TopMetric
                        icon="cloud"
                        label=""
                        value={formatNumber(safeCO, 1)}
                        unit="ppm"
                        sub="CO"
                        threshold={50}
                    />
                </View>

                <View style={styles.topRow}>
                    <TopMetric
                        icon="sun"
                        label=""
                        value={formatNumber(safeUV, 1)}
                        unit=""
                        sub="Chỉ số UV"
                        threshold={7}
                    />
                    <TopMetric
                        icon="wind"
                        label=""
                        value={formatNumber(safePM25, 0)}
                        unit="µg/m³"
                        sub="PM 2.5"
                    />
                    <TopMetric
                        icon="wind"
                        label=""
                        value={formatNumber(safePM10, 0)}
                        unit="µg/m³"
                        sub="PM 10"
                    />
                </View>

                {/* Temperature chart */}
                <View style={styles.timelineCard}>
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Biểu đồ nhiệt độ</Text>
                    </View>
                    {tempChart.data.length > 0 && (
                        <ThemedLineChart
                            labels={tempLabelsNorm}
                            data={tempChart.data}
                            unit="°C"
                            lineColor="#f97316"
                            decimals={1}
                        />
                    )}
                </View>

                {/* Humidity chart */}
                <View style={styles.timelineCard}>
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Biểu đồ độ ẩm</Text>
                    </View>
                    {humChart.data.length > 0 && (
                        <ThemedLineChart
                            labels={humLabelsNorm}
                            data={humChart.data}
                            unit="%"
                            lineColor="#22c55e"
                            decimals={1}
                        />
                    )}
                </View>
            </ScrollView>

            {/* Bottom bar */}
            <View style={styles.bottomBar}>
                <TouchableOpacity style={styles.tabItem} onPress={() => navigation.navigate(ROUTES.CHARTS_SCREEN)}>
                    <FontAwesome6 name="chart-line" size={fontSize * 0.4} color={colors.text} />
                    <Text style={styles.tabText}>Biểu đồ</Text>
                </TouchableOpacity>

                <TouchableOpacity style={styles.tabItem} onPress={() => navigation.navigate(ROUTES.ALERTS_SCREEN)}>
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
                    <Text style={styles.tabText}>Cảnh báo</Text>
                </TouchableOpacity>
            </View>
        </SafeAreaView>
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
        position: 'absolute', left: 0, right: 0,
        alignItems: 'center', justifyContent: 'center',
    },
    leftWeather: { flexDirection: 'row', alignItems: 'center' },
    leftWeatherIcon: { width: width * 0.08, height: height * 0.05, marginRight: width * 0.01 },
    leftWeatherText: { fontSize: fontSize * 0.4, color: '#ad5c51ff', fontWeight: 'bold' },
    brand: { color: colors.text, fontSize: fontSize * 0.4, opacity: 0.85, letterSpacing: 1 },
    location: { color: colors.text, fontSize: fontSize * 0.3, fontWeight: '700' },

    provinceSheet: {
        marginTop: 'auto',
        backgroundColor: '#111827',
        borderTopLeftRadius: 16,
        borderTopRightRadius: 16,
        paddingHorizontal: width * 0.02,
        paddingVertical: height * 0.01,
        maxHeight: '70%',
    },
    sheetTitle: { color: '#fff', fontSize: fontSize * 0.3, fontWeight: '600', marginBottom: height * 0.01 },
    searchInput: {
        backgroundColor: "#1f2937", color: "#fff",
        paddingHorizontal: width * 0.01, paddingVertical: height * 0.01,
        borderRadius: 8, marginBottom: height * 0.012,
    },

    topRow: {
        flexDirection: 'row',
        marginTop: height * 0.005,
        paddingHorizontal: width * 0.03,
        paddingVertical: height * 0.008,
        gap: height * 0.01
    },
    topMetric: {
        flex: 1,
        backgroundColor: colors.card,
        borderRadius: 12,
        paddingHorizontal: width * 0.03,
        paddingVertical: height * 0.018,
        borderWidth: 1,
        borderColor: colors.line,
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
    timelineLegend: { flexDirection: 'row', alignItems: 'center', marginTop: height * 0.01 },
    legendDot: { width: width * 0.02, height: height * 0.012, borderRadius: 5, backgroundColor: colors.accent, marginLeft: width * 0.01, marginRight: width * 0.01 },
    legendText: { color: colors.sub, fontSize: fontSize * 0.3 },

    bottomBar: {
        position: 'absolute', left: 0, right: 0, bottom: 0,
        height: height * 0.08, backgroundColor: colors.cardAlt,
        borderTopWidth: 1, borderTopColor: colors.line,
        flexDirection: 'row', justifyContent: 'space-around', alignItems: 'center',
    },
    tabItem: { alignItems: 'center', gap: height * 0.005 },
    tabText: { color: colors.text, fontSize: fontSize * 0.3 },
    badge: {
        position: 'absolute', top: -height * 0.01, right: -width * 0.03,
        minWidth: width * 0.05, height: height * 0.02, paddingHorizontal: width * 0.01,
        backgroundColor: colors.danger, borderRadius: 9, alignItems: 'center', justifyContent: 'center',
        borderWidth: 1, borderColor: colors.cardAlt,
    },
    badgeText: { color: '#fff', fontSize: fontSize * 0.25, fontWeight: '700' },
});

export default EnvDashboardScreen;
