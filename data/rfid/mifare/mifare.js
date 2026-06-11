const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const blockNumber = document.getElementById("blockNumber");
const blockData = document.getElementById("blockData");
const formPayload = document.getElementById("formPayload");
const messageEditor = document.getElementById("messageEditor");
const formWriter = document.getElementById("formWriter");
const convertStatus = document.getElementById("convertStatus");

function getRFIDMode() {
    return document.querySelector('input[name="rfidMode"]:checked').value;
}

function getWriteSource() {
    return document.querySelector('input[name="writeSource"]:checked').value;
}

function getWriteData() {
    return getWriteSource() === "form" ? formPayload.value : blockData.value;
}

function shouldWriteNdef() {
    return getRFIDMode() === "write" && getWriteSource() === "form";
}

function updateWriteSourceUI() {
    const useForm = getWriteSource() === "form";

    messageEditor.classList.toggle("hidden", useForm);
    formWriter.classList.toggle("hidden", !useForm);
}

async function saveRFIDConfig() {
    await fetch("/api/rfid/config", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            mode: getRFIDMode(),
            format: shouldWriteNdef() ? "ndef" : "raw",
            block: parseInt(blockNumber.value),
            data: getWriteData()
        })
    });
}

async function armNdefConverter() {
    document.querySelector('input[name="rfidMode"][value="write"]').checked = true;
    convertStatus.innerText = "Tap an NDEF-formatted MIFARE Classic card";

    await fetch("/api/rfid/config", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            mode: "convert_ndef_to_mifare",
            format: "raw",
            block: parseInt(blockNumber.value),
            data: ""
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
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success !== false
            ? "Read Successful"
            : "Read Failed: " + (data.error || "unknown");
        blockData.value = data.data;
        document.getElementById("readResult").innerText = data.data;
    }

    if (data.type === "rfid_write") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success
            ? "Write Successful"
            : "Write Failed: " + (data.error || "unknown");

        document.getElementById("readResult").innerText = data.success
            ? "Wrote message:\n" + (data.data || getWriteData())
            : "Write failed: " + (data.error || "unknown");
    }

    if (data.type === "rfid_convert") {
        rfidUid.innerText = data.uid;
        rfidStatus.innerText = data.success
            ? "Conversion Successful"
            : "Conversion Failed: " + (data.error || "unknown");
        convertStatus.innerText = data.success
            ? "Converted"
            : "Failed";

        document.getElementById("readResult").innerText = data.success
            ? "Converted NDEF to Classic blocks:\n" + (data.data || "")
            : "Convert failed: " + (data.error || "unknown");
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

document.querySelectorAll('input[name="writeSource"]').forEach(r => {
    r.addEventListener("change", () => {
        updateWriteSourceUI();
        saveRFIDConfig();
    });
});

blockNumber.addEventListener("change", saveRFIDConfig);
blockData.addEventListener("input", saveRFIDConfig);
document.getElementById("convertNdef").addEventListener("click", armNdefConverter);

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
