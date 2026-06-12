function updateNetworkStatus(data) {
    const apIp = document.getElementById("networkApIp");
    const staIp = document.getElementById("networkStaIp");
    const staName = document.getElementById("networkStaName");
    const retryStatus = document.getElementById("networkRetryStatus");

    if (!apIp || !staIp || !staName) {
        return;
    }

    const hasNetworkData = "connected" in data
        || "ssid" in data
        || "ap_ip" in data
        || "sta_ip" in data
        || "ip" in data
        || "wifi_retry_active" in data
        || "wifi_retry_paused" in data
        || "wifi_connecting" in data
        || "wifi_manual" in data;

    if (data.type && !hasNetworkData) {
        return;
    }

    apIp.innerText = data.ap_ip || "192.168.4.1";
    staIp.innerText = data.sta_ip || data.ip || "Not connected";
    staName.innerText = data.connected ? (data.ssid || "Connected") : "Not connected";

    const hasSaved = Boolean(data.wifi_has_saved);

    if (retryStatus) {
        if (data.connected) {
            retryStatus.innerText = "Connected";
        } else if (data.wifi_connecting) {
            retryStatus.innerText = "Connecting";
        } else if (hasSaved) {
            retryStatus.innerText = "Manual";
        } else {
            retryStatus.innerText = "No saved WiFi";
        }
    }
}

async function loadNetworkStatus() {
    try {
        const response = await fetch("/api/status");

        if (response.ok) {
            updateNetworkStatus(await response.json());
        }
    } catch (err) {
        updateNetworkStatus({});
    }
}

window.addEventListener("load", loadNetworkStatus);
