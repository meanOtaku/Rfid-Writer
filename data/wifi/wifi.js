const statusLabel =
    document.getElementById("status");

const networkSelect =
    document.getElementById("networks");

const savedNetworks =
    document.getElementById("savedNetworks");

const savedWifiSection =
    document.getElementById("savedWifiSection");

const newWifiSection =
    document.getElementById("newWifiSection");

function selectedWifiMode() {
    const selected =
        document.querySelector("input[name='wifiMode']:checked");

    return selected ? selected.value : "saved";
}

function updateWifiMode() {
    const useSaved =
        selectedWifiMode() === "saved";

    savedWifiSection.classList.toggle("hidden", !useSaved);
    newWifiSection.classList.toggle("hidden", useSaved);
}

function selectedSavedSSID() {
    const selected =
        document.querySelector("input[name='savedWifi']:checked");

    return selected ? selected.value : "";
}

function selectedDeleteSSIDs() {
    return Array.from(
        document.querySelectorAll("input[name='deleteWifi']:checked"))
        .map(input => input.value);
}

function renderSavedNetworks(networks) {
    if (!networks || networks.length === 0) {
        savedNetworks.innerHTML =
            "<p>No saved WiFi networks yet.</p>";

        document.querySelector("input[name='wifiMode'][value='new']").checked = true;
        updateWifiMode();

        return;
    }

    savedNetworks.innerHTML = "";

    networks.forEach((network, index) => {
        const row =
            document.createElement("div");

        row.className =
            "savedNetworkRow";

        const connectLabel =
            document.createElement("label");

        connectLabel.className =
            "savedNetworkChoice";

        const radio =
            document.createElement("input");

        radio.type =
            "radio";

        radio.name =
            "savedWifi";

        radio.value =
            network.ssid;

        radio.checked =
            index === 0;

        connectLabel.appendChild(radio);
        connectLabel.appendChild(document.createTextNode(network.ssid));

        const deleteLabel =
            document.createElement("label");

        deleteLabel.className =
            "savedNetworkDelete";

        const checkbox =
            document.createElement("input");

        checkbox.type =
            "checkbox";

        checkbox.name =
            "deleteWifi";

        checkbox.value =
            network.ssid;

        deleteLabel.appendChild(checkbox);
        deleteLabel.appendChild(document.createTextNode("Delete"));

        row.appendChild(connectLabel);
        row.appendChild(deleteLabel);
        savedNetworks.appendChild(row);
    });
}

async function loadSavedNetworks() {
    try {
        const response =
            await fetch("/api/wifi/saved");

        if (!response.ok) {
            throw new Error("saved networks unavailable");
        }

        const data =
            await response.json();

        renderSavedNetworks(data.networks || []);
    } catch (err) {
        savedNetworks.innerHTML =
            "<p>Saved WiFi networks could not be loaded.</p>";
    }
}

async function scan() {
    networkSelect.innerHTML =
        "<option>Scanning...</option>";

    await fetch("/api/scan/start");
}

async function connectWifi() {
    let ssid =
        networkSelect.value;

    let password =
        document.getElementById(
            "password").value;

    if (!ssid || ssid === "Select Network" || ssid === "Scanning..." || ssid === "No Networks Found") {
        statusLabel.innerText =
            "Select a WiFi network first";

        return;
    }

    statusLabel.innerText =
        "Connecting to " + ssid;

    await fetch(
        "/api/connect",
        {
            method: "POST",
            headers:
            {
                "Content-Type":
                    "application/json"
            },
            body:
                JSON.stringify(
                    {
                        ssid,
                        password
                    })
        });

    await loadSavedNetworks();
}

async function connectSavedWifi() {
    const ssid =
        selectedSavedSSID();

    if (!ssid) {
        statusLabel.innerText =
            "Select a saved WiFi network first";

        return;
    }

    statusLabel.innerText =
        "Connecting to " + ssid;

    await fetch(
        "/api/wifi/connect-saved",
        {
            method: "POST",
            headers:
            {
                "Content-Type":
                    "application/json"
            },
            body:
                JSON.stringify(
                    {
                        ssid
                    })
        });
}

async function deleteSavedWifi() {
    const ssids =
        selectedDeleteSSIDs();

    if (ssids.length === 0) {
        statusLabel.innerText =
            "Select saved WiFi networks to delete";

        return;
    }

    await fetch(
        "/api/wifi/delete-saved",
        {
            method: "POST",
            headers:
            {
                "Content-Type":
                    "application/json"
            },
            body:
                JSON.stringify(
                    {
                        ssids
                    })
        });

    statusLabel.innerText =
        "Deleted selected saved WiFi";

    await loadSavedNetworks();
}

function applyStatus(data) {
    if (data.connected) {
        statusLabel.innerText =
            "Connected: " +
            data.ssid;
    }
    else if (data.wifi_connecting) {
        statusLabel.innerText =
            "Connecting...";
    }
    else {
        statusLabel.innerText =
            "Not Connected";
    }
}

document
    .getElementById("scan")
    .onclick = scan;

document
    .getElementById("connect")
    .onclick = connectWifi;

document
    .getElementById("connectSaved")
    .onclick = connectSavedWifi;

document
    .getElementById("deleteSaved")
    .onclick = deleteSavedWifi;

document
    .querySelectorAll("input[name='wifiMode']")
    .forEach(input => {
        input.onchange = updateWifiMode;
    });

let socket =
    new WebSocket(
        `ws://${location.host}/ws`);

socket.onmessage = e => {
    let data =
        JSON.parse(e.data);

    updateNetworkStatus(data);

    if (data.type === "scan_results") {
        networkSelect.innerHTML = "";
        if (data.networks.length === 0) {
            networkSelect.innerHTML =
                "<option>No Networks Found</option>";

            return;
        }
        data.networks.forEach(net => {
            let option =
                document.createElement(
                    "option");

            option.value =
                net.ssid;

            option.textContent =
                `${net.ssid} (${net.rssi} dBm)`;

            networkSelect.appendChild(
                option);
        });

        return;
    }

    applyStatus(data);
};

window.addEventListener("load", async () => {
    updateWifiMode();
    await loadSavedNetworks();
});
