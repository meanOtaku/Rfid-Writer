function formatRfidData(data) {
    if (!data) {
        return "";
    }

    function extractJson(value) {
        const text = String(value).trim();
        const start = text.indexOf("{");
        const end = text.lastIndexOf("}");

        if (start < 0 || end <= start) {
            return "";
        }

        return text.slice(start, end + 1);
    }

    function displayPayload(payload, fallback) {
        if (!payload || typeof payload !== "object" || Array.isArray(payload)) {
            return fallback;
        }

        const lines = [];
        const form = payload.form || payload.Form || "";

        if (form) {
            lines.push("Form: " + form);
        }

        if (Array.isArray(payload.fields)) {
            if (lines.length > 0) {
                lines.push("");
            }

            payload.fields.forEach(field => {
                const label = field && field.label ? field.label : "Field";
                const value = field ? field.value : undefined;

                if (value !== undefined && value !== null && String(value).trim()) {
                    lines.push(label + ": " + (Array.isArray(value) ? value.join(", ") : value));
                }
            });
        } else {
            Object.keys(payload).forEach(key => {
                if (key.toLowerCase() === "form") {
                    return;
                }

                const value = payload[key];

                if (value !== undefined && value !== null && String(value).trim()) {
                    lines.push(key + ": " + (Array.isArray(value) ? value.join(", ") : value));
                }
            });
        }

        return lines.length > 0 ? lines.join("\n") : fallback;
    }

    try {
        return displayPayload(JSON.parse(data), data);
    } catch (err) {
        const json = extractJson(data);

        if (!json) {
            return data;
        }

        try {
            return displayPayload(JSON.parse(json), data);
        } catch (nestedErr) {
            return data;
        }
    }
}
