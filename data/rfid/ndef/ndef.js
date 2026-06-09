const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const blockData = document.getElementById("blockData");

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

let socket = new WebSocket(`ws://${location.host}/ws`);

socket.onmessage = e => {
    let data = JSON.parse(e.data);

    if (data.type === "rfid") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = "Card Detected";
    }

    if (data.type === "rfid_read") {
        const readText = data.success !== false
            ? (data.data || "Empty NDEF message")
            : "Read failed: " + (data.error || "unknown");

        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success !== false
            ? "NDEF Read Successful"
            : "NDEF Read Failed: " + (data.error || "unknown");

        if (data.success !== false) {
            blockData.value = data.data || "";
        }

        document.getElementById("readResult").innerText = readText;
    }

    if (data.type === "rfid_write") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success
            ? "NDEF Write Successful"
            : "NDEF Write Failed: " + (data.error || "unknown");

        document.getElementById("readResult").innerText = data.success
            ? "Wrote NDEF message:\n" + (data.data || blockData.value)
            : "Write failed: " + (data.error || "unknown");
    }

    if (data.type === "rfid_removed") {
        rfidUid.innerText = "No Card";
        rfidStatus.innerText = "Waiting for card...";
        document.getElementById("readResult").innerText = "No data read yet";

        if (getRFIDMode() === "read") {
            blockData.value = "";
        }
    }

};

document.querySelectorAll('input[name="rfidMode"]').forEach(r => {
    r.addEventListener("change", saveRFIDConfig);
});

blockData.addEventListener("input", saveRFIDConfig);

saveRFIDConfig();
