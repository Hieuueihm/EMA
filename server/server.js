import express from "express";
import cors from "cors";
import morgan from "morgan";
import dotenv from "dotenv";
import fs from "fs";
import admin from "firebase-admin";
import fetch from "node-fetch";

process.on("unhandledRejection", (err) => {
    console.error("UNHANDLED REJECTION:", err);
});
process.on("uncaughtException", (err) => {
    console.error("UNCAUGHT EXCEPTION:", err);
});

dotenv.config();

const PORT = Number(process.env.PORT || 8080);

const FB_PROJECT_ID = process.env.FB_PROJECT_ID;
const SA_PATH = process.env.SA_PATH || "service-account.json";

const TB_URL = process.env.TB_URL;
const TB_JWT = process.env.TB_JWT;
const TB_POLL_SEC = Number(process.env.TB_POLL_SEC || 5);
const TB_PAGE_SIZE = Number(process.env.TB_PAGE_SIZE || 100);

const ATTR_KEYS = (process.env.ATTR_KEYS || "buzzer,lat,lon")
    .split(",").map(s => s.trim()).filter(Boolean);

const PUSH_TOPIC = process.env.PUSH_TOPIC || "notifications_android";

const GEOCODE_UA = process.env.GEOCODE_USER_AGENT || "ema-fcm-server/1.0";
const GEOCODE_EMAIL = process.env.GEOCODE_EMAIL || "";

if (!FB_PROJECT_ID) throw new Error("Missing FB_PROJECT_ID in .env");
if (!TB_URL || !TB_JWT) throw new Error("Missing TB_URL/TB_JWT in .env");

const serviceAccount = JSON.parse(fs.readFileSync(SA_PATH, "utf8"));
admin.initializeApp({
    credential: admin.credential.cert(serviceAccount),
    projectId: FB_PROJECT_ID,
});
const messaging = admin.messaging();

const app = express();
app.use(cors());
app.use(express.json());
app.use(morgan("dev"));

function tbHeaders() {
    return { "X-Authorization": `Bearer ${TB_JWT}` };
}

async function tbFetchJson(path) {
    const res = await fetch(`${TB_URL}${path}`, { headers: tbHeaders() });
    if (!res.ok) {
        const t = await res.text().catch(() => "");
        throw new Error(`TB ${path} -> ${res.status} ${t}`);
    }
    return res.json();
}

async function tbListAllDevices(pageSize = 100) {
    let page = 0;
    const out = [];
    while (true) {
        const data = await tbFetchJson(
            `/api/tenant/devices?pageSize=${pageSize}&page=${page}`
        );
        const items = data?.data || [];
        for (const it of items) {
            const id = it?.id?.id;
            if (id) out.push({ id, name: it?.name || "", type: it?.type || "" });
        }
        if (!data?.hasNext || page + 1 >= (data?.totalPages || 0)) break;
        page += 1;
    }
    return out;
}

async function tbGetLatestAttributes(deviceId, keys) {
    const qs = keys && keys.length ? `?keys=${encodeURIComponent(keys.join(","))}` : "";
    return tbFetchJson(`/api/plugins/telemetry/DEVICE/${deviceId}/values/attributes${qs}`);
}

function getAttrValue(attrResp, key) {
    if (!attrResp) return undefined;

    if (Array.isArray(attrResp)) {
        let bestTs = -1;
        let bestVal = undefined;
        for (const it of attrResp) {
            if (it && it.key === key) {
                const ts = Number(it.lastUpdateTs || 0);
                if (ts > bestTs) {
                    bestTs = ts;
                    bestVal = it.value;
                }
            }
        }
        return bestVal;
    }

    if (Array.isArray(attrResp[key])) {
        const arr = attrResp[key];
        const last = arr[arr.length - 1];
        return last?.value;
    }
    return attrResp[key]?.value ?? attrResp[key];
}

export async function createAlertDoc({
    title,
    body,
    deviceId,
    deviceName,
    lat,
    lon,
    extra = {}
}) {
    const payload = {
        title,
        body,
        deviceId,
        deviceName: deviceName || "",
        location: (typeof lat === "number" && typeof lon === "number")
            ? { lat, lon }
            : null,
        createdAt: admin.firestore.FieldValue.serverTimestamp(),
        ...extra,
    };

    const ref = await admin.firestore().collection("alerts").add(payload);
    return ref.id;
}
async function reverseGeocode(lat, lon) {
    const url = `https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=${encodeURIComponent(lat)}&lon=${encodeURIComponent(lon)}&zoom=14&addressdetails=1${GEOCODE_EMAIL ? `&email=${encodeURIComponent(GEOCODE_EMAIL)}` : ""}`;
    const res = await fetch(url, { headers: { "User-Agent": GEOCODE_UA } });
    if (!res.ok) throw new Error(`Geocode ${res.status}`);
    const j = await res.json();
    return j.display_name || `${lat}, ${lon}`;
}

const lastAlertAt = new Map()
async function getAllFcmTokens(limit = 500) {
    const snap = await admin.firestore().collection("fcmTokens").limit(limit).get();
    return snap.docs
        .map(d => d.data().token)
        .filter(t => typeof t === "string" && t.length > 0);
}

async function checkDeviceOnce(device) {
    const attrs = await tbGetLatestAttributes(device.id, ATTR_KEYS);

    const buzzer = Number(
        getAttrValue(attrs, "buzzer") ?? 0
    );

    const lat = Number(
        getAttrValue(attrs, "lat") ?? 0
    );
    const lon = Number(
        getAttrValue(attrs, "lon") ?? 0
    );

    if (buzzer !== 1) return;

    const now = Date.now();
    const last = lastAlertAt.get(device.id) || 0;

    let where = "vị trí không xác định";
    if (!Number.isNaN(lat) && !Number.isNaN(lon)) {
        try { where = await reverseGeocode(lat, lon); }
        catch (e) { console.warn(`Geocode fail ${device.name}:`, e.message); }
    }

    const title = "⚠️ Cảnh báo nguy hiểm";
    const body = `${device.name || device.id}: Buzzer kích hoạt • ${where}`;

    const msg = {
        notification: { title, body },
        android: { priority: "high", notification: { channelId: "default" } },
        data: {
            kind: "alert",
            deviceId: device.id,
            name: device.name || "",
            lat: String(Number.isNaN(lat) ? "" : lat),
            lon: String(Number.isNaN(lon) ? "" : lon),
            ts: String(now),
        },
    };

    const tokens = await getAllFcmTokens(500);
    if (!tokens.length) {
        console.log("No tokens found in Firestore");
        return;
    }

    const resp = await messaging.sendEachForMulticast({ ...msg, tokens });

    const alertId = await createAlertDoc({
        title,
        body,
        deviceId: device.id,
        deviceName: device.name,
        lat: Number.isNaN(lat) ? undefined : lat,
        lon: Number.isNaN(lon) ? undefined : lon,
        extra: {
            // lưu thêm nếu thích:
            lastUpdateTs: now,
            source: "thingsboard",
        }
    });
    console.log(`Pushed alert → ${device.name || device.id} @ ${where} | success=${resp.successCount}, fail=${resp.failureCount}, alertCreated=${alertId}`); lastAlertAt.set(device.id, now);
}

async function pollAllDevices() {
    try {
        const devices = await tbListAllDevices(TB_PAGE_SIZE);
        if (!devices.length) {
            console.log("Không tìm thấy device nào trong tenant.");
            return;
        }
        console.log(`Đang quét ${devices.length} devices...`);
        for (const d of devices) {
            await checkDeviceOnce(d);
            await new Promise((r) => setTimeout(r, 300));
        }
    } catch (e) {
        console.error("Polling error:", e.message);
    }
}

app.get("/health", (_req, res) => res.json({ ok: true }));
app.post("/tb-scan-now", async (_req, res) => {
    try {
        await pollAllDevices();
        res.json({ ok: true });
    } catch (e) {
        res.status(500).json({ error: String(e.message || e) });
    }
});

app.listen(PORT, () => {
    console.log(`EMA FCM server listening on http://localhost:${PORT}`);
    pollAllDevices();
    setInterval(pollAllDevices, Math.max(TB_POLL_SEC, 2) * 1000);
});
