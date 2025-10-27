// screens/StatsScreen.js
import React, { useEffect, useState } from 'react';
import {
    View,
    Text,
    StyleSheet,
    Dimensions,
    TouchableOpacity,
    Modal,
    FlatList,
    Platform,
    ActivityIndicator,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { LineChart } from 'react-native-chart-kit';
import { SafeAreaView } from 'react-native-safe-area-context';

import { tb } from '../api';
import { ROUTES, PROVINCE_MAPPING } from '../../constants';
import { getItem } from '../utils/AsyncStorage';
const { width, height } = Dimensions.get('window');

const DAY_OPTIONS = [1, 3, 7, 15, 30];

const METRICS = [
    { key: 'co', label: 'CO', unit: 'ppm', tbKey: 'co', decimals: 1 },
    { key: 'uv', label: 'UV', unit: 'index', tbKey: 'uv', decimals: 1 },
    { key: 'temperature', label: 'Nhiệt độ', unit: '°C', tbKey: 'temperature', decimals: 1 },
    { key: 'humidity', label: 'Độ ẩm', unit: '%', tbKey: 'humidity', decimals: 1 },
    { key: 'pm25', label: 'PM2.5', unit: 'µg/m³', tbKey: 'pm25', decimals: 0 },
    { key: 'pm10', label: 'PM10', unit: 'µg/m³', tbKey: 'pm10', decimals: 0 },
];

function fmtLabel(ts, days) {
    const d = new Date(Number(ts));
    if (days <= 3) return `${String(d.getHours()).padStart(2, '0')}h`;
    return `${String(d.getDate()).padStart(2, '0')}/${String(d.getMonth() + 1).padStart(2, '0')}`;
}

function decimate(labels, series, maxLabels = 12) {
    if (labels.length <= maxLabels) return { labels, series };
    const step = Math.ceil(labels.length / maxLabels);
    const L = [], S = [];
    for (let i = 0; i < labels.length; i += step) {
        L.push(labels[i]);
        S.push(series[i]);
    }
    return { labels: L, series: S };
}

export default function ChartsScreen() {
    const navigation = useNavigation();

    const [days, setDays] = useState(7);
    const [metric, setMetric] = useState(METRICS[0]);

    const [showDays, setShowDays] = useState(false);
    const [showMetric, setShowMetric] = useState(false);

    const [provinceName, setProvinceName] = useState(null);
    const [labels, setLabels] = useState([]);
    const [series, setSeries] = useState([]);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState(null);

    // lấy province
    useEffect(() => {
        let cancelled = false;
        (async () => {
            try {
                const stored = await getItem('selected_province');
                const mapped = PROVINCE_MAPPING[stored];
                console.log('mapped province name:', mapped);
                if (!cancelled) setProvinceName(mapped);
            } catch {
                if (!cancelled) setProvinceName('HaNoi');
            }
        })();
        return () => { cancelled = true; };
    }, []);

    useEffect(() => {
        if (!provinceName) return;
        let cancelled = false;

        (async () => {
            setLoading(true);
            setError(null);
            try {
                if (typeof tb?.getProvinceAggregatedTimeseries !== 'function') {
                    throw new Error('tb client missing getProvinceAggregatedTimeseries');
                }

                const res = await tb.getProvinceAggregatedTimeseries(
                    provinceName,
                    metric.tbKey,
                    days
                );
                console.log('[TB JSON REAL]', res);

                const arr = Array.isArray(res?.[metric.tbKey]) ? res[metric.tbKey] : [];

                const sorted = arr
                    .map(p => ({ ts: Number(p?.ts), value: Number(p?.value) }))
                    .filter(p => Number.isFinite(p.ts) && Number.isFinite(p.value))
                    .sort((a, b) => a.ts - b.ts);

                const L = sorted.map(p => fmtLabel(p.ts, days));
                const S = sorted.map(p => p.value);

                const compact = decimate(L, S, 12);

                if (!cancelled) {
                    setLabels(compact.labels);
                    setSeries(compact.series);
                }
            } catch (e) {
                if (!cancelled) {
                    setError(String(e?.message || e));
                    console.log('[TB ERROR]', e);
                    setLabels([]);
                    setSeries([]);
                }
            } finally {
                if (!cancelled) setLoading(false);
            }
        })();

        return () => { cancelled = true; };
    }, [provinceName, metric, days]);

    const decimals = metric.decimals ?? 1;
    const unit = metric.unit || '';

    const chartConfig = {
        backgroundColor: '#1f2937',
        backgroundGradientFrom: '#1f2937',
        backgroundGradientTo: '#1f2937',
        decimalPlaces: decimals,
        color: (opacity = 1) => `rgba(255,255,255,${opacity})`,
        labelColor: (opacity = 1) => `rgba(255,255,255,${opacity})`,
        style: { borderRadius: 16 },
        propsForDots: { r: '2', strokeWidth: '1', stroke: '#fff' },
    };

    const formatY = (val) => {
        const n = Number(val);
        if (!Number.isFinite(n)) return '';
        const fixed = n.toFixed(decimals);
        return unit ? `${fixed} ${unit}` : fixed;
    };

    const avgText = (() => {
        if (!series?.length) return '';
        const sum = series.reduce((a, b) => a + b, 0);
        const avg = sum / series.length;
        return ` Trung bình: ${avg.toFixed(decimals)} ${unit}`;
    })();

    return (
        <SafeAreaView style={styles.container}>
            {/* Header */}
            <View style={styles.headerRow}>
                <TouchableOpacity onPress={() => navigation.navigate(ROUTES.HOME_SCREEN)} style={styles.backBtn}>
                    <Text style={styles.backTxt}>‹</Text>
                </TouchableOpacity>

                <View style={{ flex: 1 }}>
                    <Text style={styles.title}>Thống kê</Text>
                    <Text style={styles.subtitle}>
                        {metric.label} · {days} ngày gần đây{provinceName ? ` · ${provinceName}` : ''}
                    </Text>
                </View>

                <View style={styles.actions}>
                    <TouchableOpacity style={styles.actionBtn} onPress={() => setShowDays(true)}>
                        <Text style={styles.actionTxt}>{days}d ▾</Text>
                    </TouchableOpacity>
                    <TouchableOpacity style={[styles.actionBtn, { marginLeft: 8 }]} onPress={() => setShowMetric(true)}>
                        <Text style={styles.actionTxt}>{metric.label} ▾</Text>
                    </TouchableOpacity>
                </View>
            </View>

            {/* Chart card */}
            <View style={styles.card}>
                <Text style={styles.cardTitle}>
                    {metric.label} ({unit})
                </Text>

                {loading ? (
                    <View style={{ height: height * 0.35, alignItems: 'center', justifyContent: 'center' }}>
                        <ActivityIndicator color="#fff" />
                        <Text style={{ color: '#cfd9ee', marginTop: width * 0.001 }}>Đang tải dữ liệu…</Text>
                    </View>
                ) : (
                    series.length > 0 && (
                        <LineChart
                            data={{ labels, datasets: [{ data: series }] }}
                            width={width - width * 0.1}
                            height={height * 0.35}
                            formatYLabel={formatY}
                            withVerticalLabels={labels.length <= 12}
                            withHorizontalLabels
                            chartConfig={chartConfig}
                            bezier
                            style={styles.chart}
                            fromZero
                        />
                    )
                )}

                {(!loading && series.length === 0) ? (
                    <Text style={{ color: '#cfd9ee', marginTop: 8 }}>Chưa có dữ liệu hiển thị.</Text>
                ) : null}

                {error ? <Text style={{ color: '#ff9f9f', marginTop: 6 }}>Lỗi: {error}</Text> : null}

                <Text style={styles.note}>{avgText}</Text>
            </View>

            {/* Modal chọn ngày */}
            <Modal visible={showDays} transparent animationType="fade" onRequestClose={() => setShowDays(false)}>
                <TouchableOpacity style={styles.modalBackdrop} onPress={() => setShowDays(false)} activeOpacity={1}>
                    <View style={styles.modalCard}>
                        <Text style={styles.modalTitle}>Chọn số ngày</Text>
                        <FlatList
                            data={DAY_OPTIONS}
                            keyExtractor={(item) => String(item)}
                            renderItem={({ item }) => (
                                <TouchableOpacity
                                    style={[styles.modalItem, item === days && styles.modalItemActive]}
                                    onPress={() => { setDays(item); setShowDays(false); }}>
                                    <Text style={[styles.modalItemTxt, item === days && styles.modalItemTxtActive]}>
                                        {item} ngày
                                    </Text>
                                </TouchableOpacity>
                            )}
                        />
                    </View>
                </TouchableOpacity>
            </Modal>

            <Modal visible={showMetric} transparent animationType="fade" onRequestClose={() => setShowMetric(false)}>
                <TouchableOpacity style={styles.modalBackdrop} onPress={() => setShowMetric(false)} activeOpacity={1}>
                    <View style={styles.modalCard}>
                        <Text style={styles.modalTitle}>Chọn loại dữ liệu</Text>
                        <FlatList
                            data={METRICS}
                            keyExtractor={(item) => item.key}
                            renderItem={({ item }) => (
                                <TouchableOpacity
                                    style={[styles.modalItem, item.key === metric.key && styles.modalItemActive]}
                                    onPress={() => { setMetric(item); setShowMetric(false); }}>
                                    <Text style={[styles.modalItemTxt, item.key === metric.key && styles.modalItemTxtActive]}>
                                        {item.label} ({item.unit})
                                    </Text>
                                </TouchableOpacity>
                            )}
                        />
                    </View>
                </TouchableOpacity>
            </Modal>
        </SafeAreaView>
    );
}

const styles = StyleSheet.create({
    container: {
        flex: 1,
        backgroundColor: '#0b1220',
        paddingHorizontal: 16,
        paddingTop: Platform.OS === 'android' ? 8 : 0,
    },
    headerRow: {
        flexDirection: 'row',
        alignItems: 'center',
        marginBottom: 12,
    },
    backBtn: {
        width: 36,
        height: 36,
        borderRadius: 18,
        backgroundColor: '#182235',
        alignItems: 'center',
        justifyContent: 'center',
        marginRight: 12,
    },
    backTxt: { color: '#fff', fontSize: 22, marginTop: -2 },
    title: { color: '#fff', fontSize: 18, fontWeight: '700' },
    subtitle: { color: '#b8c0cc', fontSize: 13, marginTop: 2 },
    actions: { flexDirection: 'row' },
    actionBtn: {
        height: 36,
        paddingHorizontal: 10,
        borderRadius: 10,
        backgroundColor: '#24324a',
        alignItems: 'center',
        justifyContent: 'center',
    },
    actionTxt: { color: '#e6eefc', fontSize: 13, fontWeight: '600' },

    card: {
        backgroundColor: '#162235',
        borderRadius: 16,
        padding: 12,
    },
    cardTitle: { color: '#dbe7ff', fontSize: 15, fontWeight: '700', marginBottom: 8 },
    chart: { borderRadius: 16, marginLeft: -8 },
    note: { color: '#92a2be', fontSize: 12, marginTop: 8, textAlign: 'right' },

    modalBackdrop: {
        flex: 1,
        backgroundColor: 'rgba(0,0,0,0.45)',
        alignItems: 'center',
        justifyContent: 'center',
        paddingHorizontal: 24,
    },
    modalCard: {
        width: '100%',
        maxWidth: 420,
        backgroundColor: '#101a2a',
        borderRadius: 16,
        padding: 12,
    },
    modalTitle: { color: '#eaf2ff', fontSize: 16, fontWeight: '700', marginBottom: 8 },
    modalItem: { paddingVertical: 10, paddingHorizontal: 10, borderRadius: 10 },
    modalItemActive: { backgroundColor: '#20314d' },
    modalItemTxt: { color: '#cfd9ee', fontSize: 14, fontWeight: '600' },
    modalItemTxtActive: { color: '#ffffff' },
});
