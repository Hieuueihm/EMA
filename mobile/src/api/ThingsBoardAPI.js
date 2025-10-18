
const DEFAULT_PAGE_SIZE = 100;


export function createThingsBoardClient({ baseUrl, jwtToken, fetchImpl, defaultPageSize = DEFAULT_PAGE_SIZE }) {
    if (!baseUrl) throw new Error("Missing baseUrl");
    if (!jwtToken) throw new Error("Missing jwtToken");
    const _fetch = fetchImpl || (typeof fetch !== "undefined" ? fetch.bind(globalThis) : null);

    if (!_fetch) throw new Error("No fetch implementation available");

    let _jwt = jwtToken;

    function authHeader() {
        return { "X-Authorization": `Bearer ${_jwt}` };
    }

    async function fetchJson(url, opts = {}) {
        const res = await _fetch(url, {
            ...opts,
            headers: { ...(opts.headers || {}), ...authHeader() },
        });
        if (!res.ok) {
            const text = await safeText(res);
            throw new Error(`HTTP ${res.status} ${res.statusText}: ${text}`);
        }
        const t = await res.text();
        return t?.trim() ? JSON.parse(t) : {};
    }


    async function getDevicesPage({ page = 0, pageSize = defaultPageSize, type, textSearch } = {}) {
        const qs = new URLSearchParams();
        qs.set("pageSize", String(pageSize));
        qs.set("page", String(page));
        if (type) qs.set("type", type);
        if (textSearch) qs.set("textSearch", textSearch);
        return await fetchJson(`${baseUrl}/api/tenant/devices?${qs.toString()}`);
    }


    async function getAllDevices({ pageSize = defaultPageSize, type, textSearch } = {}) {
        const all = [];
        let page = 0;

        for (let guard = 0; guard < 10000; guard++) {
            const pd = await getDevicesPage({ page, pageSize, type, textSearch });
            const data = pd?.data || [];
            all.push(...data);

            const totalPages = Number.isFinite(pd?.totalPages)
                ? pd.totalPages
                : pd?.hasNext
                    ? page + 2
                    : page + 1;

            const hasNext = pd?.hasNext ?? page + 1 < totalPages;
            if (!hasNext || data.length === 0) break;
            page += 1;
        }
        return all;
    }

    async function getDeviceAttributes(deviceId, keys = []) {
        if (!deviceId) throw new Error("deviceId required");
        const qs = keys.length ? `?keys=${keys.join(",")}` : "";
        const arr = await fetchJson(`${baseUrl}/api/plugins/telemetry/DEVICE/${encodeURIComponent(deviceId)}/values/attributes${qs}`);
        const list = Array.isArray(arr) ? arr : Object.values(arr || {}).flat();
        const out = {};
        for (const kv of list) {
            if (kv && typeof kv.key === "string") out[kv.key] = kv.value;
        }
        return out;
    }

    async function getLatestTelemetry(deviceId, keys = []) {
        if (!deviceId) throw new Error("deviceId required");
        const qs = new URLSearchParams();
        if (keys.length) qs.set("keys", keys.join(","));
        const obj = await fetchJson(
            `${baseUrl}/api/plugins/telemetry/DEVICE/${encodeURIComponent(deviceId)}/values/timeseries?${qs}`
        );
        const latest = {};
        for (const [k, arr] of Object.entries(obj || {})) {
            if (Array.isArray(arr) && arr.length) {
                const { ts, value } = arr[arr.length - 1];
                latest[k] = { ts: Number(ts), value };
            }
        }
        return latest;
    }

    async function pMap(array, mapper, { concurrency = 8 } = {}) {
        const ret = [];
        let i = 0;
        const workers = Array.from({ length: Math.max(1, concurrency) }, async () => {
            while (i < array.length) {
                const idx = i++;
                ret[idx] = await mapper(array[idx], idx);
            }
        });
        await Promise.all(workers);
        return ret;
    }

    async function enrichDevice(device, { attrKeys = [], tsKeys = [] } = {}) {
        const id = device?.id?.id;
        if (!id) return { device, attributes: {}, latestTelemetry: {} };
        const [attributes, latestTelemetry] = await Promise.all([
            getDeviceAttributes(id, attrKeys),
            getLatestTelemetry(id, tsKeys),
        ]);
        return { device, attributes, latestTelemetry };
    }

    async function getAllDevicesEnriched({
        pageSize = defaultPageSize,
        type,
        textSearch,
        attrKeys = [],     // ví dụ: ["province", "model"]
        tsKeys = [],       // ví dụ: ["temperature","pm25"]
        concurrency = 8,
    } = {}) {
        const all = await getAllDevices({ pageSize, type, textSearch });
        const enriched = await pMap(
            all,
            (dev) => enrichDevice(dev, { attrKeys, tsKeys }),
            { concurrency }
        );
        return enriched;
    }
    async function getDevicesByProvinceEnriched({
        province = "HoChiMinh",
        pageSize = defaultPageSize,
        attrKey = "province",
        tsKeys = [],
        concurrency = 8,
    } = {}) {
        // Lấy toàn bộ trước
        const all = await getAllDevices({ pageSize });
        // Lấy attributes song song (chỉ key 'province' để nhanh)
        const attrsList = await pMap(
            all,
            (dev) => getDeviceAttributes(dev?.id?.id, [attrKey]),
            { concurrency }
        );
        // Lọc theo tỉnh
        const filtered = all.filter((_, idx) => attrsList[idx]?.[attrKey] === province);
        // Enrich đầy đủ (attributes tất cả key, và latest telemetry theo tsKeys)
        const enriched = await pMap(
            filtered,
            (dev) => enrichDevice(dev, { attrKeys: [], tsKeys }),
            { concurrency }
        );
        return enriched;
    }



    return {
        getDevicesPage,
        getAllDevices,
        getAllDevicesEnriched,
        getDevicesByProvinceEnriched
    };
}
