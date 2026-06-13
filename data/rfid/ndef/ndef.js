const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const blockData = document.getElementById("blockData");
const formPayload = document.getElementById("formPayload");
const messageEditor = document.getElementById("messageEditor");
const formWriter = document.getElementById("formWriter");
const writeSourceCard = document.getElementById("writeSourceCard");

function getRFIDMode() {
    return document.querySelector('input[name="rfidMode"]:checked').value;
}

function getWriteSource() {
    return document.querySelector('input[name="writeSource"]:checked').value;
}

function getWriteData() {
    return getWriteSource() === "form" ? formPayload.value : blockData.value;
}

function displayData(value, fallback) {
    return formatRfidData(value || "") || fallback;
}

function updateWriteSourceUI() {
    const isWriteMode = getRFIDMode() === "write";
    const useForm = getWriteSource() === "form";

    writeSourceCard.classList.toggle("hidden", !isWriteMode);
    messageEditor.classList.toggle("hidden", !isWriteMode || useForm);
    formWriter.classList.toggle("hidden", !isWriteMode || !useForm);
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
            data: getWriteData()
        })
    });
}

let socket = new WebSocket(`ws://${location.host}/ws`);

socket.onmessage = e => {
    let data = JSON.parse(e.data);

    updateNetworkStatus(data);

    if (data.type === "rfid") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = "Card Detected";
    }

    if (data.type === "rfid_read") {
        const readText = data.success !== false
            ? displayData(data.data, "Empty NDEF message")
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
            ? "Wrote NDEF message:\n" + displayData(data.data || getWriteData(), "Empty NDEF message")
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
    r.addEventListener("change", () => {
        updateWriteSourceUI();
        saveRFIDConfig();
    });
});

document.querySelectorAll('input[name="writeSource"]').forEach(r => {
    r.addEventListener("change", () => {
        updateWriteSourceUI();
        saveRFIDConfig();
    });
});

blockData.addEventListener("input", saveRFIDConfig);

const formUrlInput = formWriter.querySelector("[data-form-url]");

if (formUrlInput) {
    formUrlInput.value = "";
}

createFormWriter({
    rootId: "formWriter",
    outputId: "formPayload",
    onPayloadChange: payload => {
        if (payload.length > 0) {
            document.querySelector('input[name="writeSource"][value="form"]').checked = true;
            document.querySelector('input[name="rfidMode"][value="write"]').checked = true;
        }

        updateWriteSourceUI();
        saveRFIDConfig();
    }
});

updateWriteSourceUI();
saveRFIDConfig();
