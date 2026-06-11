function createFormWriter(options) {
    const root = document.getElementById(options.rootId);
    const output = document.getElementById(options.outputId);
    const onPayloadChange = options.onPayloadChange;

    if (!root || !output || !onPayloadChange) {
        return;
    }

    let fields = [];

    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;");
    }

    function setStatus(message) {
        const status = root.querySelector("[data-form-status]");

        if (status) {
            status.innerText = message;
        }
    }

    function normalizeUrl(url) {
        const trimmed = url.trim();

        if (!trimmed) {
            return "";
        }

        return trimmed.replace("/edit", "/viewform").replace("/formResponse", "/viewform");
    }

    async function fetchText(url) {
        try {
            const direct = await fetch(url);

            if (direct.ok) {
                return await direct.text();
            }
        } catch (err) {
            // Google Forms normally blocks direct browser reads from the ESP32 page.
        }

        const proxiedUrl = "https://api.allorigins.win/raw?url=" + encodeURIComponent(url);
        const proxied = await fetch(proxiedUrl);

        if (!proxied.ok) {
            throw new Error("Unable to load form");
        }

        return proxied.text();
    }

    function extractPublicLoadData(html) {
        const marker = "var FB_PUBLIC_LOAD_DATA_ = ";
        const start = html.indexOf(marker);

        if (start < 0) {
            return null;
        }

        const dataStart = start + marker.length;
        const dataEnd = html.indexOf(";</script>", dataStart);

        if (dataEnd < 0) {
            return null;
        }

        return Function("return " + html.slice(dataStart, dataEnd))();
    }

    function questionType(field) {
        if (field.type === 2) {
            return "textarea";
        }

        if (field.options.length > 0) {
            return "select";
        }

        return "text";
    }

    function findQuestions(node, found) {
        if (!Array.isArray(node)) {
            return;
        }

        if (typeof node[1] === "string" && typeof node[3] === "number" && Array.isArray(node[4])) {
            const entry = Array.isArray(node[4][0]) ? node[4][0] : null;

            if (entry && typeof entry[0] === "number") {
                const options = Array.isArray(entry[1])
                    ? entry[1].filter(option => Array.isArray(option) && typeof option[0] === "string").map(option => option[0])
                    : [];

                found.push({
                    id: "entry." + entry[0],
                    label: node[1],
                    type: node[3],
                    options
                });

                return;
            }
        }

        node.forEach(child => findQuestions(child, found));
    }

    function parseGoogleForm(html) {
        const data = extractPublicLoadData(html);
        const parsedFields = [];

        findQuestions(data, parsedFields);

        return parsedFields.filter((field, index, all) => {
            return all.findIndex(candidate => candidate.id === field.id) === index;
        });
    }

    function renderFields() {
        const fieldsRoot = root.querySelector("[data-form-fields]");

        if (!fieldsRoot) {
            return;
        }

        if (fields.length === 0) {
            fieldsRoot.innerHTML = "";
            return;
        }

        fieldsRoot.innerHTML = fields.map(field => {
            const id = escapeHtml(field.id);
            const label = escapeHtml(field.label);
            const type = questionType(field);

            if (type === "select") {
                return `<label class="fieldLabel" for="${id}">${label}</label>
                    <select id="${id}" data-form-input="${id}">
                        <option value=""></option>
                        ${field.options.map(option => `<option value="${escapeHtml(option)}">${escapeHtml(option)}</option>`).join("")}
                    </select>`;
            }

            if (type === "textarea") {
                return `<label class="fieldLabel" for="${id}">${label}</label>
                    <textarea id="${id}" rows="3" data-form-input="${id}"></textarea>`;
            }

            return `<label class="fieldLabel" for="${id}">${label}</label>
                <input id="${id}" type="text" data-form-input="${id}">`;
        }).join("");

        fieldsRoot.querySelectorAll("[data-form-input]").forEach(input => {
            input.addEventListener("input", updatePayload);
            input.addEventListener("change", updatePayload);
        });

        updatePayload();
    }

    function buildPayload() {
        const formUrl = normalizeUrl(root.querySelector("[data-form-url]").value);
        const lines = [];

        if (formUrl) {
            lines.push("Form: " + formUrl);
        }

        fields.forEach(field => {
            const input = root.querySelector(`[data-form-input="${field.id}"]`);
            const value = input ? input.value.trim() : "";

            if (value) {
                lines.push(field.label + ": " + value);
            }
        });

        return lines.join("\n");
    }

    function updatePayload() {
        const payload = buildPayload();

        output.value = payload;
        onPayloadChange(payload);
    }

    async function loadForm() {
        const url = normalizeUrl(root.querySelector("[data-form-url]").value);

        if (!url) {
            setStatus("Enter a Google Form link");
            return;
        }

        setStatus("Loading form...");

        try {
            const html = await fetchText(url);
            fields = parseGoogleForm(html);

            if (fields.length === 0) {
                setStatus("No supported fields found");
                renderFields();
                return;
            }

            setStatus("Loaded " + fields.length + " fields");
            renderFields();
        } catch (err) {
            fields = [];
            renderFields();
            setStatus("Could not load form automatically. Use manual fields.");
        }
    }

    function addManualField() {
        const labelInput = root.querySelector("[data-manual-field]");
        const label = labelInput.value.trim();

        if (!label) {
            return;
        }

        fields.push({
            id: "manual_" + Date.now(),
            label,
            type: 0,
            options: []
        });

        labelInput.value = "";
        setStatus("Added manual field");
        renderFields();
    }

    root.querySelector("[data-load-form]").addEventListener("click", loadForm);
    root.querySelector("[data-add-field]").addEventListener("click", addManualField);
    root.querySelector("[data-form-url]").addEventListener("input", updatePayload);
}
