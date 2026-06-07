const statusLabel =
    document.getElementById("status");

const networkSelect =
    document.getElementById("networks");

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
}

document
    .getElementById("scan")
    .onclick = scan;

document
    .getElementById("connect")
    .onclick = connectWifi;

let socket =
    new WebSocket(
        `ws://${location.host}/ws`);

socket.onmessage = e => {
    let data =
        JSON.parse(e.data);

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

    if (data.connected) {
        statusLabel.innerText =
            "Connected: " +
            data.ssid;
    }
    else {
        statusLabel.innerText =
            "Not Connected";
    }
};