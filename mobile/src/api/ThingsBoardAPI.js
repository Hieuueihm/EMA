
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
    function qs(params) {
        const u = new URLSearchParams();
        Object.entries(params).forEach(([k, v]) => {
            if (v === undefined || v === null) return;
            u.set(k, String(v));
        });
        return u.toString();
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

    async function getAssetsByProvince(province, { page = 0, pageSize = defaultPageSize } = {}) {
        if (!province) throw new Error("Missing province");
        const url = `${baseUrl}/api/tenant/assets?pageSize=${pageSize}&page=${page}&textSearch=${encodeURIComponent(province)}`;
        const data = await fetchJson(url);
        const assets = (data.data || []).filter(a => {
            return a?.additionalInfo?.province === province || a?.name?.includes(province);
        });
        return assets;
    }

    async function getTelemetryForEntity(
        { entityType = "ASSET", entityId },
        keys,
        {
            startTs,
            endTs,
            interval,
            limit = 10000,
            agg = "NONE",
            order = "ASC",
        } = {}
    ) {
        if (!entityId) throw new Error("Missing entityId");
        if (!Array.isArray(keys) || keys.length === 0)
            throw new Error("keys must be a non-empty array");
        if (startTs == null || endTs == null)
            throw new Error("Missing startTs or endTs (ms)");

        const url =
            `${baseUrl}/api/plugins/telemetry/${encodeURIComponent(
                entityType
            )}/${encodeURIComponent(entityId)}/values/timeseries?` +
            qs({
                keys: keys.join(","),
                startTs,
                endTs,
                interval,
                limit,
                agg,
                order,
            });

        return await fetchJson(url);
    }

    async function getAssetsTelemetryByProvince(
        province,
        keys,
        timeRange,
        { page = 0, pageSize = defaultPageSize, entityType = "ASSET", ...opts } = {}
    ) {
        const assets = await getAssetsByProvince(province, { page, pageSize });
        const out = [];
        for (const a of assets) {
            const telemetry = await getTelemetryForEntity(
                { entityType, entityId: a.id?.id },
                keys,
                timeRange ? { ...timeRange, ...opts } : { ...opts }
            );
            out.push({ asset: a, telemetry });
        }
        return out;
    }


    async function listDevices({ page = 0, pageSize = defaultPageSize, textSearch } = {}) {
        const q = qs({ page, pageSize, textSearch });
        const url = `${baseUrl}/api/tenant/devices?${q}`;
        return await fetchJson(url);
    }

    async function getAttributesForEntity({ entityType = "DEVICE", entityId, keys = [] } = {}) {
        if (!entityId) throw new Error("Missing entityId");
        const url =
            `${baseUrl}/api/plugins/telemetry/${encodeURIComponent(entityType)}/${encodeURIComponent(entityId)}/values/attributes?` +
            qs({ keys: keys.length ? keys.join(",") : undefined });
        const arr = await fetchJson(url); // [{key,value,lastUpdateTs}, ...]
        const out = {};
        if (Array.isArray(arr)) {
            for (const item of arr) out[item.key] = item; // map theo key
        }
        return out;
    }


    async function getAllDevicesWithAttributes({
        keys = ["lat", "lon", "buzzer"],
        pageSize = defaultPageSize,
        textSearch
    } = {}) {
        let page = 0;
        const out = [];

        while (true) {
            const pageData = await listDevices({ page, pageSize, textSearch });
            const devices = pageData?.data || [];

            // tải song song trong 1 trang cho nhanh
            const pageResults = await Promise.all(devices.map(async (d) => {
                const devId = d?.id?.id;
                if (!devId) return null;

                const attrsMap = await getAttributesForEntity({
                    entityType: "DEVICE",
                    entityId: devId,
                    keys,
                });

                // chuẩn hoá về lat/lon/buzzer
                const pick = (names) => {
                    for (const k of names) if (attrsMap[k]?.value !== undefined) return attrsMap[k].value;
                    return null;
                };
                const toNum = v => (v == null ? null : (typeof v === "number" ? v : Number(v)));
                const toBool = v => (v == null ? null : (typeof v === "boolean" ? v : ["true", "1", 1, true].includes(v) || String(v).toLowerCase() === "true"));

                const attributes = {
                    lat: toNum(pick(["lat"])),
                    lon: toNum(pick(["lon"])),
                    buzzer: toBool(pick(["buzzer"])),
                };

                return { device: d, attributes, attributesRaw: attrsMap };
            }));

            for (const it of pageResults) if (it) out.push(it);

            if (pageData?.hasNext === true) { page += 1; continue; }
            const totalPages = pageData?.totalPages;
            if (typeof totalPages === "number" && page + 1 < totalPages) { page += 1; continue; }
            if (!totalPages && devices.length === pageSize) { page += 1; continue; }
            break;
        }

        return out;
    }

    return {
        getAssetsByProvince,
        getAssetsTelemetryByProvince,

        listDevices,
        getAttributesForEntity,
        getAllDevicesWithAttributes,
    };

}
