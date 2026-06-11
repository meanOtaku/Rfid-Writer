const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const cardType = document.getElementById("cardType");
const dataType = document.getElementById("dataType");
const readResult = document.getElementById("readResult");

async function saveRFIDConfig() {
    await fetch("/api/rfid/config", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            mode: "read",
            format: "auto",
            block: 4,
            data: ""
        })
    });
}

function formatLabel(format) {
    if (format === "ndef") {
        return "NDEF";
    }

    if (format === "mifare" || format === "raw") {
        return "Classic";
    }

    return "Auto";
}

function cardTypeLabel(type) {
    if (!type) {
        return "Detecting...";
    }

    return "Detecting...";
}

function resetReader() {
    rfidUid.innerText = "No Card";
    cardType.innerText = "Unknown";
    dataType.innerText = "Auto";
    rfidStatus.innerText = "Waiting for card...";
    readResult.innerText = "No data read yet";
}

let socket = new WebSocket(`ws://${location.host}/ws`);

socket.onopen = saveRFIDConfig;

socket.onmessage = e => {
    let data = JSON.parse(e.data);

    updateNetworkStatus(data);

    if (data.type === "rfid") {
        rfidUid.innerText = data.uid;
        cardType.innerText = data.cardType || "Unknown";
        dataType.innerText = cardTypeLabel(data.cardType);
        rfidStatus.innerText = "Card Detected";
    }

    if (data.type === "rfid_read") {
        rfidUid.innerText = data.uid;
        cardType.innerText = data.cardType || "Unknown";
        dataType.innerText = data.format ? formatLabel(data.format) : cardTypeLabel(data.cardType);
        rfidStatus.innerText = data.success !== false
            ? "Read Successful"
            : "Read Failed: " + (data.error || "unknown");
        readResult.innerText = data.success !== false
            ? (data.data || "No readable data")
            : "Read failed: " + (data.error || "unknown");
    }

    if (data.type === "rfid_removed") {
        resetReader();
    }
};

document.addEventListener("visibilitychange", () => {
    if (!document.hidden) {
        saveRFIDConfig();
    }
});

saveRFIDConfig();
