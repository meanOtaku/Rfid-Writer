function updateNetworkStatus(data) {
    const apIp = document.getElementById("networkApIp");
    const staIp = document.getElementById("networkStaIp");
    const staName = document.getElementById("networkStaName");

    if (!apIp || !staIp || !staName) {
        return;
    }

    apIp.innerText = data.ap_ip || "192.168.4.1";
    staIp.innerText = data.sta_ip || data.ip || "Not connected";
    staName.innerText = data.connected ? (data.ssid || "Connected") : "Not connected";
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
