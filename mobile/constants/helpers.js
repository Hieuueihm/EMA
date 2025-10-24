import firestore from '@react-native-firebase/firestore';
import messaging from '@react-native-firebase/messaging';

export async function getDeviceToken() {
    const t = await messaging().getToken();
    if (!t) throw new Error('No FCM token');
    return t;
}

export function alertsCol() {
    return firestore().collection('alerts');
}
export function readsColForToken(token) {
    return firestore().collection('fcmTokens').doc(token).collection('reads');
}

export async function markAlertRead(token, alertId) {
    await readsColForToken(token)
        .doc(alertId)
        .set({ readAt: firestore.FieldValue.serverTimestamp() }, { merge: true });
}

export async function unmarkAlertRead(token, alertId) {
    await readsColForToken(token).doc(alertId).delete();
}

export function onAlertsSnapshot(cb) {
    return alertsCol()
        .orderBy('createdAt', 'desc')
        .limit(200)
        .onSnapshot(snap => {
            const arr = [];
            snap?.forEach(d => {
                const v = d.data() || {};
                arr?.push({
                    id: d.id,
                    title: v.title || 'Alert',
                    body: v.body || '',
                    createdAt: v.createdAt,
                });
            });
            cb(arr);
        });
}

export function onReadsSnapshot(token, cb) {
    return readsColForToken(token).onSnapshot(snap => {
        const map = {};
        snap?.forEach(d => { map[d.id] = true; });
        cb(map);
    });
}
