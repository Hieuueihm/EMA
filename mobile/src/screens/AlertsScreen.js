// AlertsScreen.js
import React, { useEffect, useMemo, useState, useCallback } from 'react';
import { View, Text, FlatList, TouchableOpacity, ActivityIndicator, StyleSheet, Dimensions } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import firestore from '@react-native-firebase/firestore';
import { onAlertsSnapshot, onReadsSnapshot, getDeviceToken, markAlertRead } from '../../constants/helpers';

const { width, height } = Dimensions.get('window');

export default function AlertsScreen() {
    const navigation = useNavigation();

    const [token, setToken] = useState(null);
    const [alerts, setAlerts] = useState([]);
    const [reads, setReads] = useState({});
    const [loading, setLoading] = useState(true);
    const [markingAll, setMarkingAll] = useState(false);

    // lấy token
    useEffect(() => {
        let mounted = true;
        (async () => {
            const t = await getDeviceToken();
            if (mounted) setToken(t);
        })();
        return () => { mounted = false; };
    }, []);

    // subscribe alerts + reads
    useEffect(() => {
        const unsubA = onAlertsSnapshot(arr => {
            setAlerts(arr);
            setLoading(false);
        });
        let unsubR = null;
        if (token) unsubR = onReadsSnapshot(token, setReads);
        return () => {
            unsubA && unsubA();
            unsubR && unsubR();
        };
    }, [token]);

    // danh sách chưa đọc
    const unread = useMemo(() => alerts.filter(a => !reads[a.id]), [alerts, reads]);

    const onMark = useCallback(async (id) => {
        if (!token) return;
        await markAlertRead(token, id);
    }, [token]);

    // ĐỌC TẤT CẢ (batch write)
    const markAllRead = useCallback(async () => {
        if (!token || unread.length === 0) return;
        try {
            setMarkingAll(true);
            const batch = firestore().batch();
            const base = firestore().collection('fcmTokens').doc(token).collection('reads');
            const ts = firestore.FieldValue.serverTimestamp();
            unread.forEach(it => {
                batch.set(base.doc(it.id), { readAt: ts }, { merge: true });
            });
            await batch.commit();
        } finally {
            setMarkingAll(false);
        }
    }, [token, unread]);

    const renderHeader = () => (
        <View style={styles.header}>
            <TouchableOpacity style={styles.headerBtn} onPress={() => navigation.goBack()}>
                <FontAwesome6 name="chevron-left" size={16} color="#fff" />
                <Text style={styles.headerBtnText}>Back</Text>
            </TouchableOpacity>

            <Text style={styles.headerTitle}>Alerts</Text>

            <TouchableOpacity
                style={[styles.headerBtnRight, unread.length === 0 || markingAll ? styles.btnDisabled : null]}
                onPress={markAllRead}
                disabled={unread.length === 0 || markingAll}
            >
                <FontAwesome6 name="check-double" size={16} color="#fff" />
                <Text style={styles.headerBtnText}>{markingAll ? 'Đang đọc…' : 'Đọc tất cả'}</Text>
            </TouchableOpacity>
        </View>
    );

    const renderItem = ({ item }) => (
        <View style={styles.card}>
            {/* viền trái cảnh báo */}
            <View style={styles.cardStripe} />
            <View style={styles.cardBody}>
                <View style={styles.cardTitleRow}>
                    <FontAwesome6 name="bell" size={16} color="#e74c3c" />
                    <Text style={styles.cardTitle} numberOfLines={2}>{item.title}</Text>
                </View>

                {!!item.body && <Text style={styles.cardBodyText}>{item.body}</Text>}

                {/* metadata nếu server có lưu thêm (deviceName/deviceId/location/createdAt) */}
                <View style={styles.metaRow}>
                    {/* ví dụ đọc thêm khi server đã lưu: deviceName/deviceId/location */}
                    {/* <Text style={styles.metaText}>Thiết bị: {item.deviceName || item.deviceId}</Text> */}
                    {item.createdAt?.toDate && (
                        <Text style={styles.metaText}>
                            {item.createdAt.toDate().toLocaleString('vi-VN')}
                        </Text>
                    )}
                </View>

                <View style={styles.actionsRow}>
                    <TouchableOpacity style={styles.readBtn} onPress={() => onMark(item.id)}>
                        <Text style={styles.readBtnText}>Đọc</Text>
                    </TouchableOpacity>
                </View>
            </View>
        </View>
    );

    if (loading || !token) {
        return (
            <View style={styles.center}>
                <ActivityIndicator />
                <Text style={{ color: '#fff', marginTop: 8 }}>Đang tải Alerts…</Text>
            </View>
        );
    }

    return (
        <View style={styles.container}>
            {renderHeader()}
            <FlatList
                data={unread}
                keyExtractor={(it) => it.id}
                contentContainerStyle={{ paddingBottom: 24 }}
                ListEmptyComponent={
                    <View style={{ padding: 24, alignItems: 'center' }}>
                        <FontAwesome6 name="circle-check" size={20} color="#1abc9c" />
                        <Text style={{ color: '#9fb0c3', marginTop: 8 }}>Không có cảnh báo chưa đọc.</Text>
                    </View>
                }
                renderItem={renderItem}
            />
        </View>
    );
}

const colors = {
    bg: '#111418',
    card: '#1a1f26',
    cardAlt: '#171c22',
    text: '#e7eef7',
    sub: '#9fb0c3',
    accent: '#00d1ff',
    danger: '#e74c3c',
    line: '#2b3340',
};

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: colors.bg },
    center: { flex: 1, alignItems: 'center', justifyContent: 'center', backgroundColor: colors.bg },

    // Header
    header: {
        height: 56,
        paddingHorizontal: 12,
        flexDirection: 'row',
        alignItems: 'center',
        borderBottomWidth: StyleSheet.hairlineWidth,
        borderBottomColor: colors.line,
        justifyContent: 'space-between',
        backgroundColor: colors.cardAlt,
    },
    headerBtn: { flexDirection: 'row', alignItems: 'center', gap: 6, padding: 8 },
    headerBtnRight: { flexDirection: 'row', alignItems: 'center', gap: 6, padding: 8 },
    headerBtnText: { color: '#fff', fontWeight: '600' },
    headerTitle: { color: '#fff', fontWeight: '700', fontSize: 16 },

    btnDisabled: { opacity: 0.5 },

    // Card
    card: {
        flexDirection: 'row',
        marginHorizontal: 12,
        marginTop: 12,
        backgroundColor: colors.card,
        borderRadius: 12,
        borderWidth: 1,
        borderColor: colors.line,
        overflow: 'hidden',
    },
    cardStripe: { width: 6, backgroundColor: colors.danger },
    cardBody: { flex: 1, padding: 12 },
    cardTitleRow: { flexDirection: 'row', alignItems: 'center', gap: 8, marginBottom: 6 },
    cardTitle: { color: colors.text, fontWeight: '700', fontSize: 15, flex: 1 },
    cardBodyText: { color: colors.text, opacity: 0.9, marginBottom: 8 },

    metaRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
    metaText: { color: colors.sub, fontSize: 12 },

    actionsRow: { marginTop: 10, flexDirection: 'row', gap: 12 },
    readBtn: {
        backgroundColor: '#0a84ff',
        paddingHorizontal: 14,
        paddingVertical: 8,
        borderRadius: 10,
    },
    readBtnText: { color: '#fff', fontWeight: '700' },
});
