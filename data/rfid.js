const rfidUid =
    document.getElementById(
        "rfidUid");

const rfidStatus =
    document.getElementById(
        "rfidStatus");

const blockNumber =
    document.getElementById(
        "blockNumber");

const blockData =
    document.getElementById(
        "blockData");

function getRFIDMode()
{
    return document.querySelector(
        'input[name="rfidMode"]:checked'
    ).value;
}

async function saveRFIDConfig()
{
    await fetch(
        "/api/rfid/config",
        {
            method: "POST",
            headers:
            {
                "Content-Type":
                    "application/json"
            },
            body:
                JSON.stringify(
                {
                    mode:
                        getRFIDMode(),

                    block:
                        parseInt(
                            blockNumber.value),

                    data:
                        blockData.value
                })
        });
}

let socket =
    new WebSocket(
        `ws://${location.host}/ws`);

socket.onmessage = e =>
{
    let data =
        JSON.parse(e.data);

    if (data.type === "rfid")
    {
        rfidUid.innerText =
            data.uid;

        rfidStatus.innerText =
            "Card Detected";
    }

    if (data.type === "rfid_read")
    {
        rfidUid.innerText =
            data.uid;

        rfidStatus.innerText =
            "Read Successful";

        blockData.value =
            data.data;

        document
            .getElementById(
                "readResult")
            .innerText =
            data.data;
    }

    if (data.type === "rfid_write")
    {
        rfidUid.innerText =
            data.uid;

        rfidStatus.innerText =
            data.success
                ? "Write Successful"
                : "Write Failed";
    }
};

document
    .querySelectorAll(
        'input[name="rfidMode"]')
    .forEach(r =>
    {
        r.addEventListener(
            "change",
            saveRFIDConfig);
    });

blockNumber.addEventListener(
    "change",
    saveRFIDConfig);

blockData.addEventListener(
    "input",
    saveRFIDConfig);

saveRFIDConfig();