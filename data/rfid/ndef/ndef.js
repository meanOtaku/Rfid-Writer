const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const blockData = document.getElementById("blockData");
const formatButton = document.getElementById("formatButton");

function getRFIDMode() {
    return document.querySelector('input[name="rfidMode"]:checked').value;
}

async function saveRFIDConfig() {
    await fetch("/api/rfid/config", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            mode: getRFIDMode(),
            format: "ndef",
            block: 4, // Dummy block, ignored in NDEF mode
            data: blockData.value
        })
    });
}

async function triggerFormat() {
    rfidStatus.innerText = "Waiting for card to format...";
    await fetch("/api/rfid/format", {
        method: "POST"
    });
}

let socket = new WebSocket(`ws://${location.host}/ws`);

socket.onmessage = e => {
    let data = JSON.parse(e.data);

    if (data.type === "rfid") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = "Card Detected";
    }

    if (data.type === "rfid_read") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success !== false
            ? "NDEF Read Successful"
            : "NDEF Read Failed: " + (data.error || "unknown");
        blockData.value = data.data;
        document.getElementById("readResult").innerText = data.data;
    }

    if (data.type === "rfid_write") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success
            ? "NDEF Write Successful"
            : "NDEF Write Failed: " + (data.error || "unknown");
    }

    if (data.type === "rfid_format") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success
            ? "NDEF Format Successful"
            : "NDEF Format Failed: " + (data.error || "unknown");
    }
};

document.querySelectorAll('input[name="rfidMode"]').forEach(r => {
    r.addEventListener("change", saveRFIDConfig);
});

blockData.addEventListener("input", saveRFIDConfig);
formatButton.addEventListener("click", triggerFormat);

saveRFIDConfig();
