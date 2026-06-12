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

    return trimmed
        .replace("/edit", "/viewform")
        .replace("/formResponse", "/viewform");
}

function formResponseUrl(url) {
    return normalizeUrl(url).replace("/viewform", "/formResponse");
}

function normalizeLabel(label) {
    return String(label || "")
        .replace(/\s+/g, " ")
        .replace(/\s+\*$/, "")
        .trim()
        .toLowerCase();
}

async function fetchFormHtml(url) {
    const normalized = normalizeUrl(url);
    const urls = [
        normalized,
        "https://api.allorigins.win/raw?url=" + encodeURIComponent(normalized),
        "https://corsproxy.io/?" + encodeURIComponent(normalized)
    ];

    for (const candidate of urls) {
        try {
            const response = await fetch(candidate);

            if (!response.ok) {
                continue;
            }

            const html = await response.text();

            if (html.includes("FB_PUBLIC_LOAD_DATA_")) {
                return html;
            }
        } catch (err) {
            // Try the next source.
        }
    }

    throw new Error("Could not load Google Form");
}

function extractPublicLoadData(html) {
    const marker = "FB_PUBLIC_LOAD_DATA_";
    const markerIndex = html.indexOf(marker);

    if (markerIndex < 0) {
        return null;
    }

    const equalsIndex = html.indexOf("=", markerIndex);
    const dataStart = html.indexOf("[", equalsIndex);

    if (equalsIndex < 0 || dataStart < 0) {
        return null;
    }

    let depth = 0;
    let inString = false;
    let quote = "";
    let escaped = false;

    for (let i = dataStart; i < html.length; i++) {
        const char = html[i];

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (char === "\\") {
                escaped = true;
            } else if (char === quote) {
                inString = false;
            }

            continue;
        }

        if (char === "\"" || char === "'") {
            inString = true;
            quote = char;
            continue;
        }

        if (char === "[") {
            depth++;
        } else if (char === "]") {
            depth--;

            if (depth === 0) {
                return Function("return " + html.slice(dataStart, i + 1))();
            }
        }
    }

    return null;
}

function collectEntryNodes(node, found) {
    if (!Array.isArray(node)) {
        return;
    }

    if (typeof node[0] === "number" && String(node[0]).length >= 5) {
        found.push(node);
        return;
    }

    node.forEach(child => collectEntryNodes(child, found));
}

function maybeQuestion(node) {
    if (!Array.isArray(node)) {
        return null;
    }

    const label = String(node[1] || "").replace(/\s+/g, " ").replace(/\s+\*$/, "").trim();
    const entriesRoot = node[4];

    if (!label || label === ":" || !Array.isArray(entriesRoot)) {
        return null;
    }

    const entryNodes = [];
    collectEntryNodes(entriesRoot, entryNodes);

    if (entryNodes.length === 0 || typeof entryNodes[0][0] !== "number") {
        return null;
    }

    return {
        label,
        entry: "entry." + entryNodes[0][0]
    };
}

function findQuestions(node, found) {
    if (!Array.isArray(node)) {
        return;
    }

    const field = maybeQuestion(node);

    if (field) {
        found.push(field);
        return;
    }

    node.forEach(child => findQuestions(child, found));
}

function parseGoogleFormFields(html) {
    const data = extractPublicLoadData(html);
    const fields = [];

    if (!data) {
        return [];
    }

    findQuestions(data, fields);

    return fields.filter((field, index, all) => {
        return all.findIndex(candidate => candidate.entry === field.entry) === index;
    });
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

    const values = {};

    Object.keys(payload).forEach(key => {
        if (normalizeLabel(key) !== "form") {
            values[key] = payload[key];
        }
    });

    if (Object.keys(values).length === 0) {
        throw new Error("Card data does not contain form field values");
    }

    return {
        form,
        values
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
    uploadStatus.innerText = "Loading Google Form...";

    const html = await fetchFormHtml(cardData.form);
    const fields = parseGoogleFormFields(html);
    const fieldMap = new Map();

    fields.forEach(field => {
        fieldMap.set(normalizeLabel(field.label), field);
    });

    const body = new URLSearchParams();
    const unmatched = [];
    let matched = 0;

    Object.keys(cardData.values).forEach(label => {
        const field = fieldMap.get(normalizeLabel(label));

        if (!field) {
            unmatched.push(label);
            return;
        }

        appendValue(body, field.entry, cardData.values[label]);
        matched++;
    });

    if (matched === 0) {
        throw new Error("No card fields matched the Google Form");
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

    uploadStatus.innerText = unmatched.length > 0
        ? "Submitted. Unmatched fields: " + unmatched.join(", ")
        : "Submitted successfully";
}

async function handleCardData(text) {
    if (text === lastSubmittedPayload) {
        return;
    }

    lastSubmittedPayload = text;
    readResult.innerText = text;

    try {
        const cardData = parseCardPayload(text);
        await submitForm(cardData);
    } catch (err) {
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
