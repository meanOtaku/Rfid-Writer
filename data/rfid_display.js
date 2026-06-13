function formatRfidData(data) {
    if (!data) {
        return "";
    }

    function unescapePayloadValue(value) {
        let out = "";
        let escaped = false;

        String(value || "").split("").forEach(char => {
            if (escaped) {
                if (char === "t") {
                    out += "\t";
                } else if (char === "n") {
                    out += "\n";
                } else if (char === "r") {
                    out += "\r";
                } else {
                    out += char;
                }

                escaped = false;
                return;
            }

            if (char === "\\") {
                escaped = true;
                return;
            }

            out += char;
        });

        if (escaped) {
            out += "\\";
        }

        return out;
    }

    function displayCompactForm(value) {
        const lines = String(value || "").split("\n");

        if (lines[0] !== "GFORM1") {
            return "";
        }

        const out = [
            "Form: " + unescapePayloadValue(lines[1] || "")
        ];

        for (let i = 2; i < lines.length; i++) {
            const parts = lines[i].split("\t");

            if (parts.length < 3) {
                continue;
            }

            out.push(unescapePayloadValue(parts[1]) + ": " + unescapePayloadValue(parts.slice(2).join("\t")));
        }

        return out.join("\n");
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
        const form = payload.u || "";

        if (form) {
            lines.push("Form: " + form);
        }

        if (payload.f && typeof payload.f === "object" && !Array.isArray(payload.f)) {
            if (lines.length > 0) {
                lines.push("");
            }

            Object.keys(payload.f).forEach(entryId => {
                const field = payload.f[entryId];
                const label = field && field.n ? field.n : "Field";
                const value = field ? field.v : undefined;

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

    const compact = displayCompactForm(data);

    if (compact) {
        return compact;
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
