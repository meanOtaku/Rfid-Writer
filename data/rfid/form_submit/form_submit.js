const rfidUid = document.getElementById("rfidUid");
const rfidStatus = document.getElementById("rfidStatus");
const uploadStatus = document.getElementById("uploadStatus");
const readResult = document.getElementById("readResult");

let lastSubmittedPayload = "";

async function saveRFIDConfig() {
    await fetch("/api/rfid/config", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            mode: "read",
            format: "ndef",
            block: 4,
            data: ""
        })
    });
}

function normalizeUrl(url) {
    const trimmed = String(url || "").trim();

    if (!trimmed) {
        return "";
    }

    const normalized = trimmed
        .replace("/edit", "/viewform")
        .replace("/formResponse", "/viewform");

    try {
        const parsed = new URL(normalized);
        parsed.hash = "";
        parsed.search = "";

        return parsed.toString();
    } catch (err) {
        return normalized.split("#")[0].split("?")[0];
    }
}

function formResponseUrl(url) {
    return normalizeUrl(url).replace("/viewform", "/formResponse");
}

function parseDirectFields(payload) {
    if (!Array.isArray(payload.fields)) {
        return [];
    }

    return payload.fields.map(field => {
        return {
            entry: String(field && field.entry ? field.entry : ""),
            label: String(field && field.label ? field.label : ""),
            value: field ? field.value : undefined
        };
    }).filter(field => field.entry && field.entry.startsWith("entry.") && field.value !== undefined);
}

function parseCardPayload(text) {
    const payload = JSON.parse(text);

    if (!payload || typeof payload !== "object" || Array.isArray(payload)) {
        throw new Error("Card data is not a form object");
    }

    const form = normalizeUrl(payload.form || payload.Form || "");

    if (!form) {
        throw new Error("Card data does not contain a form URL");
    }

    const fields = parseDirectFields(payload);

    if (fields.length === 0) {
        throw new Error("Card does not contain direct Google Form fields. Write the card again with the updated form writer.");
    }

    return {
        form,
        fields
    };
}

function appendValue(body, entry, value) {
    if (Array.isArray(value)) {
        value.forEach(item => {
            if (String(item).trim()) {
                body.append(entry, String(item).trim());
            }
        });

        return;
    }

    body.append(entry, String(value).trim());
}

async function submitForm(cardData) {
    const body = new URLSearchParams();
    let submittedFields = 0;

    cardData.fields.forEach(field => {
        appendValue(body, field.entry, field.value);
        submittedFields++;
    });

    if (submittedFields === 0) {
        throw new Error("No filled fields found on card");
    }

    uploadStatus.innerText = "Submitting form...";

    await fetch(formResponseUrl(cardData.form), {
        method: "POST",
        mode: "no-cors",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body
    });

    uploadStatus.innerText = "Submitted " + submittedFields + " fields";
}

function previewCardData(cardData) {
    let text = "Form: " + cardData.form + "\n\n";

    cardData.fields.forEach(field => {
        text += field.label + ": " + (Array.isArray(field.value) ? field.value.join(", ") : field.value) + "\n";
    });

    return text.trim();
}

async function handleCardData(text) {
    if (text === lastSubmittedPayload) {
        return;
    }

    lastSubmittedPayload = text;

    try {
        const cardData = parseCardPayload(text);
        readResult.innerText = previewCardData(cardData);
        await submitForm(cardData);
    } catch (err) {
        readResult.innerText = formatRfidData(text) || "No data read";
        uploadStatus.innerText = "Upload failed: " + err.message;
    }
}

function resetPage() {
    rfidUid.innerText = "No Card";
    rfidStatus.innerText = "Waiting for card...";
    uploadStatus.innerText = "Tap an NDEF card with form data";
    readResult.innerText = "No data read yet";
    lastSubmittedPayload = "";
}

let socket = new WebSocket(`ws://${location.host}/ws`);

socket.onopen = saveRFIDConfig;

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
            ? "NDEF Read Successful"
            : "NDEF Read Failed: " + (data.error || "unknown");

        if (data.success !== false) {
            handleCardData(data.data || "");
        } else {
            uploadStatus.innerText = "Read failed";
            readResult.innerText = data.error || "unknown";
        }
    }

    if (data.type === "rfid_removed") {
        resetPage();
    }
};

document.addEventListener("visibilitychange", () => {
    if (!document.hidden) {
        saveRFIDConfig();
    }
});

saveRFIDConfig();
