function createFormWriter(options) {
    const root = document.getElementById(options.rootId);
    const output = document.getElementById(options.outputId);
    const onPayloadChange = options.onPayloadChange;

    if (!root || !output || !onPayloadChange) {
        return;
    }

    let fields = [];
    let loadVersion = 0;

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

    async function fetchText(url) {
        const urls = [
            url,
            "https://api.allorigins.win/raw?url=" + encodeURIComponent(url),
            "https://corsproxy.io/?" + encodeURIComponent(url)
        ];

        for (const candidate of urls) {
            try {
                const response = await fetch(candidate);

                if (response.ok) {
                    const text = await response.text();

                    if (text && text.includes("FB_PUBLIC_LOAD_DATA_")) {
                        return text;
                    }
                }
            } catch (err) {
                // Try the next source.
            }
        }

        throw new Error("Unable to load form HTML");
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

    function cleanLabel(label) {
        return String(label || "")
            .replace(/\s+/g, " ")
            .replace(/\s+\*$/, "")
            .trim();
    }

    function normalizeOption(option) {
        if (typeof option === "string") {
            return cleanLabel(option);
        }

        if (Array.isArray(option)) {
            for (const value of option) {
                const label = normalizeOption(value);

                if (label) {
                    return label;
                }
            }
        }

        return "";
    }

    function extractOptions(entryNode) {
        const optionsRoot = Array.isArray(entryNode) ? entryNode[1] : null;

        if (!Array.isArray(optionsRoot)) {
            return [];
        }

        const options = [];

        optionsRoot.forEach(option => {
            const label = normalizeOption(option);

            if (label && label !== "__other_option__" && !options.includes(label)) {
                options.push(label);
            }
        });

        return options;
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

    function fieldType(googleType, options) {
        if (googleType === 1) {
            return "textarea";
        }

        if (googleType === 2) {
            return "radio";
        }

        if (googleType === 3) {
            return "select";
        }

        if (googleType === 4) {
            return "checkbox";
        }

        if (googleType === 9) {
            return "date";
        }

        if (googleType === 10) {
            return "time";
        }

        if (options.length > 0) {
            return "select";
        }

        return "text";
    }

    function maybeQuestion(node) {
        if (!Array.isArray(node)) {
            return null;
        }

        const label = cleanLabel(node[1]);
        const googleType = node[3];
        const entriesRoot = node[4];

        if (!label || label === ":" || typeof googleType !== "number" || !Array.isArray(entriesRoot)) {
            return null;
        }

        const entryNodes = [];
        collectEntryNodes(entriesRoot, entryNodes);

        if (entryNodes.length === 0 || typeof entryNodes[0][0] !== "number") {
            return null;
        }

        const options = extractOptions(entryNodes[0]);

        return {
            id: "entry." + entryNodes[0][0],
            label,
            type: fieldType(googleType, options),
            googleType,
            options
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

    function parseGoogleForm(html) {
        const data = extractPublicLoadData(html);
        const parsedFields = [];

        if (!data) {
            return [];
        }

        findQuestions(data, parsedFields);

        return parsedFields.filter((field, index, all) => {
            return all.findIndex(candidate => candidate.id === field.id) === index;
        });
    }

    function domIdFor(field, index) {
        return "form_" + index + "_" + field.id.replace(/[^a-zA-Z0-9_-]/g, "_");
    }

    function matchingInputs(field) {
        return Array.from(root.querySelectorAll("[data-form-input]"))
            .filter(input => input.dataset.formInput === field.id);
    }

    function renderOptionInputs(field, id, index, inputType) {
        return field.options.map((option, optionIndex) => {
            const optionId = id + "_" + optionIndex;
            const name = "form_option_" + index;

            return `<label class="fieldOption" for="${optionId}">
                <input id="${optionId}" type="${inputType}" name="${name}" value="${escapeHtml(option)}" data-form-input="${escapeHtml(field.id)}">
                ${escapeHtml(option)}
            </label>`;
        }).join("");
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

        fieldsRoot.innerHTML = fields.map((field, index) => {
            const id = domIdFor(field, index);
            const label = escapeHtml(field.label);

            if (field.type === "select") {
                return `<label class="fieldLabel" for="${id}">${label}</label>
                    <select id="${id}" data-form-input="${escapeHtml(field.id)}">
                        <option value=""></option>
                        ${field.options.map(option => `<option value="${escapeHtml(option)}">${escapeHtml(option)}</option>`).join("")}
                    </select>`;
            }

            if (field.type === "radio") {
                return `<div class="fieldLabel">${label}</div>
                    <div class="fieldOptions">${renderOptionInputs(field, id, index, "radio")}</div>`;
            }

            if (field.type === "checkbox") {
                return `<div class="fieldLabel">${label}</div>
                    <div class="fieldOptions">${renderOptionInputs(field, id, index, "checkbox")}</div>`;
            }

            if (field.type === "textarea") {
                return `<label class="fieldLabel" for="${id}">${label}</label>
                    <textarea id="${id}" rows="3" data-form-input="${escapeHtml(field.id)}"></textarea>`;
            }

            if (field.type === "date" || field.type === "time") {
                return `<label class="fieldLabel" for="${id}">${label}</label>
                    <input id="${id}" type="${field.type}" data-form-input="${escapeHtml(field.id)}">`;
            }

            return `<label class="fieldLabel" for="${id}">${label}</label>
                <input id="${id}" type="text" data-form-input="${escapeHtml(field.id)}">`;
        }).join("");

        fieldsRoot.querySelectorAll("[data-form-input]").forEach(input => {
            input.addEventListener("input", updatePayload);
            input.addEventListener("change", updatePayload);
        });

        updatePayload();
    }

    function clearFields() {
        fields = [];
        renderFields();
        updatePayload();
    }

    function fieldValue(field) {
        const inputs = matchingInputs(field);

        if (field.type === "checkbox") {
            return inputs
                .filter(input => input.checked)
                .map(input => input.value.trim())
                .filter(Boolean);
        }

        const checked = inputs.find(input => input.type === "radio" && input.checked);

        if (checked) {
            return checked.value.trim();
        }

        const input = inputs[0];

        return input ? input.value.trim() : "";
    }

    function buildPayload() {
        const formUrl = normalizeUrl(root.querySelector("[data-form-url]").value);
        const payload = {
            form: formUrl,
            fields: []
        };

        fields.forEach(field => {
            const value = fieldValue(field);

            if (Array.isArray(value) ? value.length > 0 : value) {
                payload.fields.push({
                    entry: field.id,
                    label: field.label,
                    value
                });
            }
        });

        if (!payload.form && payload.fields.length === 0) {
            return "";
        }

        return JSON.stringify(payload);
    }

    function updatePayload() {
        const payload = buildPayload();

        output.value = payload;
        onPayloadChange(payload);
    }

    async function loadForm() {
        const currentLoad = ++loadVersion;
        const url = normalizeUrl(root.querySelector("[data-form-url]").value);

        if (!url) {
            clearFields();
            setStatus("Enter a Google Form link");
            return;
        }

        clearFields();
        setStatus("Loading form...");

        try {
            const html = await fetchText(url);
            const parsedFields = parseGoogleForm(html);

            if (currentLoad !== loadVersion) {
                return;
            }

            fields = parsedFields;

            if (fields.length === 0) {
                setStatus("No supported fields found");
                renderFields();
                return;
            }

            setStatus("Loaded " + fields.length + " fields");
            renderFields();
        } catch (err) {
            if (currentLoad !== loadVersion) {
                return;
            }

            clearFields();
            setStatus("Could not load form automatically. Check browser internet access.");
        }
    }

    function resetForm() {
        const urlInput = root.querySelector("[data-form-url]");
        const labelInput = root.querySelector("[data-manual-field]");

        loadVersion++;

        if (urlInput) {
            urlInput.value = "";
        }

        if (labelInput) {
            labelInput.value = "";
        }

        clearFields();
        setStatus("Enter a Google Form link");
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
            type: "text",
            googleType: -1,
            options: []
        });

        labelInput.value = "";
        setStatus("Added manual field");
        renderFields();
    }

    root.querySelector("[data-load-form]").addEventListener("click", loadForm);
    root.querySelector("[data-add-field]").addEventListener("click", addManualField);
    const resetButton = root.querySelector("[data-reset-form]");

    if (resetButton) {
        resetButton.addEventListener("click", resetForm);
    }

    root.querySelector("[data-form-url]").addEventListener("input", updatePayload);
}
