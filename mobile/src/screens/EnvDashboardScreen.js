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
import { tb } from '../api';

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


const BottomTab = () => (
    <View style={styles.bottomBar}>
        <View style={styles.tabItem}>
            <FontAwesome6 name="chart-line" size={18} color={colors.text} />
            <Text style={styles.tabText}>Charts</Text>
        </View>
        <View style={styles.tabItem}>
            <FontAwesome6 name="bell" size={18} color={colors.text} />
            <Text style={styles.tabText}>Alerts</Text>
        </View>
        <View style={styles.tabItem}>
            <FontAwesome6 name="gear" size={18} color={colors.text} />
            <Text style={styles.tabText}>Settings</Text>
        </View>
    </View>
);

const EnvDashboardScreen = () => {

    const [city, setCity] = useState('Ha Noi');
    const [pickerVisible, setPickerVisible] = useState(false);
    const [cityNameFSearch, setCityNameFSearch] = useState('Hanoi');
    const [searchText, setSearchText] = useState("");

    const filteredProvinces = PROVINCES_EN.filter((item) =>
        item.toLowerCase().includes(searchText.toLowerCase())
    );

    const [weather, setWeather] = useState({})
    // useEffect(() => {
    //     api.WeatherAPI.fetchWeatherForecast({ cityName: cityNameFSearch, days: '7' }).then(data => {
    //         setWeather(data)
    //     })
    // }, []);

    if (weather) {
        var { current, location, forecast } = weather;
    }

    useEffect(() => {
        (async () => {
            try {

                // TEST 2: Lấy toàn bộ (tự phân trang)
                const all = await tb.getAllDevices({ pageSize: 200 });
                console.log("✅ Total devices:", all.length);
                const allEnriched = await tb.getAllDevicesEnriched({
                    attrKeys: [],                     // để rỗng = lấy tất cả attributes hiện có
                    tsKeys: ["temperature", "pm25"],   // chọn telemetry cần; để [] nếu muốn tất cả key có sẵn
                });
                console.log("Total enriched:", allEnriched.length);
                console.log("Sample:", allEnriched[1]);
                const hcm = await tb.getDevicesByProvinceEnriched({
                    province: "HoChiMinh",
                    tsKeys: ["temperature", "pm25"],
                });
                console.log("HCM devices:", hcm.length);
                hcm.slice(0, 5).forEach((x, i) => {
                    console.log(`#${i + 1}`, {
                        name: x.device?.name,
                        province: x.attributes?.province,
                        temp: x.latestTelemetry?.temperature?.value,
                        pm25: x.latestTelemetry?.pm25?.value,
                    });
                });

            } catch (err) {
                console.error("❌ ThingsBoard test error:", err);
            }
        })();
    }, []);


    const onSelectProvince = (vnName) => {
        console.log(vnName)
        console.log(PROVINCE_MAPPING[vnName])
        setCity(vnName);
        setPickerVisible(false);
    };
    const navigation = useNavigation();
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


    const SparklinePlus = ({
        data = [],
        height = height * 0.1,
        barWidth = 4,
        gap = 2,
        unit = '',
        showGrid = true,
        gridLevels = 3,
        showMinMax = true,
        showLastValue = true,
        textColor = colors.sub,
        barColor = colors.accent,
    }) => {
        if (!data?.length) return null;
        const min = Math.min(...data);
        const max = Math.max(...data);
        const range = Math.max(1e-6, max - min);

        return (
            <View style={{ position: 'relative' }}>
                {showMinMax && (
                    <View style={{ flexDirection: 'row', justifyContent: 'space-between', marginBottom: height * 0.001 }}>
                        <Text style={{ color: textColor, fontSize: fontSize * 0.3 }}>{`Min: ${min}${unit}`}</Text>
                        <Text style={{ color: textColor, fontSize: fontSize * 0.3 }}>{`Max: ${max}${unit}`}</Text>
                    </View>
                )}

                <View style={{ height, borderRadius: 6, backgroundColor: colors.cardAlt, overflow: 'hidden' }}>
                    {showGrid &&
                        Array.from({ length: gridLevels + 1 }).map((_, i) => (
                            <View
                                key={i}
                                style={{
                                    position: 'absolute',
                                    left: 0,
                                    right: 0,
                                    top: (i / gridLevels) * height,
                                    height: 1,
                                    backgroundColor: colors.line,
                                    opacity: 0.7,
                                }}
                            />
                        ))}

                    <View style={{ flexDirection: 'row', alignItems: 'flex-end', height, paddingHorizontal: width * 0.01 }}>
                        {data.map((v, i) => {
                            const h = Math.max(3, ((v - min) / range) * height);
                            return (
                                <View
                                    key={i}
                                    style={{
                                        width: barWidth,
                                        height: h,
                                        marginRight: gap,
                                        backgroundColor: barColor,
                                        borderRadius: 2,
                                        alignSelf: 'flex-end',
                                    }}
                                />
                            );
                        })}
                    </View>

                    {showLastValue && (
                        (() => {
                            const last = data[data.length - 1];
                            const y = height - Math.max(3, ((last - min) / range) * height);
                            return (
                                <View
                                    style={{
                                        position: 'absolute',
                                        right: width * 0.01,
                                        top: Math.max(0, y - 14),
                                        paddingHorizontal: width * 0.01,
                                        paddingVertical: height * 0.005,
                                        backgroundColor: colors.card,
                                        borderWidth: 1,
                                        borderColor: colors.line,
                                        borderRadius: 6,
                                    }}
                                >
                                    <Text style={{ color: colors.text, fontSize: fontSize * 0.3 }}>{`${last}${unit}`}</Text>
                                </View>
                            );
                        })()
                    )}
                </View>
            </View>
        );
    };
    const aqiCategory = (aqi) => {
        if (aqi <= 50) return { label: 'Tốt', color: '#2ecc71' };
        if (aqi <= 100) return { label: 'Trung bình', color: '#f1c40f' };
        if (aqi <= 150) return { label: 'Nhạy cảm', color: '#e67e22' };
        if (aqi <= 200) return { label: 'Xấu', color: '#e74c3c' };
        if (aqi <= 300) return { label: 'Rất xấu', color: '#8e44ad' };
        return { label: 'Nguy hại', color: '#7f1d1d' };
    };


    const AQIBar = ({ aqi = 95, max = 500, height = 24 }) => {
        const segments = [
            { max: 50, color: '#2ecc71', label: 'Tốt' },
            { max: 100, color: '#f1c40f', label: 'Trung bình' },
            { max: 150, color: '#e67e22', label: 'Nhạy cảm' },
            { max: 200, color: '#e74c3c', label: 'Xấu' },
            { max: 300, color: '#8e44ad', label: 'Rất xấu' },
            { max: 500, color: '#7f1d1d', label: 'Nguy hại' },
        ];

        const percent = Math.min(1, aqi / max);
        const pos = `${percent * 100}%`;

        return (
            <View style={{
                marginVertical: height * 0.2, alignItems: 'center', paddingBottom: height * 0.4
            }}>
                <Text style={{ fontSize: fontSize * 0.6, fontWeight: 'bold', color: '#fff', marginBottom: height * 0.3 }}>
                    AQI
                </Text>

                <View style={{ flexDirection: 'row', width: '90%', height, borderRadius: 12, overflow: 'hidden' }}>
                    {segments.map((seg, i) => {
                        const widthPct = (seg.max - (segments[i - 1]?.max || 0)) / max * 100;
                        return (
                            <View key={i} style={{ flex: widthPct, backgroundColor: seg.color }} />
                        );
                    })}
                </View>

                <View
                    style={{
                        position: 'absolute',
                        left: pos,
                        marginLeft: -width * 0.03,
                        top: height * 1.5,
                        alignItems: 'center'
                    }}
                >
                    <View style={{ width: 2, height: height + 8, backgroundColor: '#fff' }} />
                    <Text style={{
                        color: '#fff', fontSize: fontSize * 0.3, fontWeight: 'bold', marginTop: height * 0.01, marginBottom: height * 0.1
                    }}>
                        {aqi}
                    </Text>
                </View>
            </View>
        );
    };

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
                <TouchableOpacity >
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
                    <TopMetric icon="temperature-full" label="" value="38" unit="°C" sub="Temperature" threshold={30} />
                    <TopMetric icon="water" label="" value="1" unit="a" sub="Humidity" />
                    <TopMetric icon="c" label="" value="0.09" unit="ppm" sub="CO" />
                </View>
                <View style={styles.topRow}>
                    <TopMetric icon="temperature-full" label="" value="38" unit="°C" sub="UV" />
                    <TopMetric icon="water" label="" value="1" unit="a" sub="PM 2.5" />
                    <TopMetric icon="c" label="" value="0.09" unit="ppm" sub="PM 10" />
                </View>
                <AQIBar aqi={225} />

                {/* Big timeline-like graph mock */}
                <View style={styles.timelineCard}>
                    <Text style={styles.timelineTitle}>Last 12h • Temp</Text>
                    <SparklinePlus
                        data={[22, 23, 24, 26, 25, 27, 29, 28, 26, 25, 24, 23]}
                        height={56}
                        unit="°C"
                        showGrid
                        gridLevels={4}
                    />
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Temperature trend</Text>
                    </View>
                </View>

                <View style={styles.timelineCard}>
                    <Text style={styles.timelineTitle}>Last 12h • Humidity</Text>
                    <SparklinePlus
                        data={[60, 62, 64, 65, 67, 70, 72, 68, 66, 65, 64, 63]}
                        height={56}
                        unit="%"
                        showGrid
                    />
                    <View style={styles.timelineLegend}>
                        <View style={styles.legendDot} />
                        <Text style={styles.legendText}>Humidity trend</Text>
                    </View>
                </View>




            </ScrollView>

            {/* Bottom bar */}
            <BottomTab />
        </View>
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
        paddingHorizontal: width * 0.02,

        paddingVertical: height * 0.02,
        backgroundColor: colors.cardAlt,
        borderRadius: 12,
        borderWidth: 1,
        borderColor: colors.line,
    },
    timelineTitle: { color: colors.sub, fontSize: fontSize * 0.3, marginBottom: height * 0.01 },
    sparkline: { flexDirection: 'row', alignItems: 'flex-end' },
    timelineLegend: { flexDirection: 'row', alignItems: 'center', marginTop: height * 0.01 },
    legendDot: { width: width * 0.02, height: height * 0.012, borderRadius: 5, backgroundColor: colors.accent, marginRight: 8 },
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
});

export default EnvDashboardScreen;
