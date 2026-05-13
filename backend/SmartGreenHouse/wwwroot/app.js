const API_URL = "http://localhost:5067";


let configurations = [];
let selectedConfiguration = null;
async function loadConfigurations() {
    const response = await fetch(`${API_URL}/configuration`);
    configurations = await response.json();
    renderPlantList();
}
function renderPlantList() {
    const container = document.getElementById("plant-list");
    container.innerHTML = "";
    configurations.forEach(configuration => {
        const div = document.createElement("div");
        div.className = "plant-item";
        if (selectedConfiguration?.id === configuration.id)
            div.classList.add("active");
        div.innerText = configuration.name;
        div.onclick = () => selectConfiguration(configuration.id);
        container.appendChild(div);
    });
}
function selectConfiguration(id) {
    selectedConfiguration = configurations.find(x => x.id === Number(id));

    if (!selectedConfiguration) return;

    document.getElementById("plantName").value = selectedConfiguration.name;
    document.getElementById("soilHumidity").value = selectedConfiguration.soilHumidity;
    document.getElementById("lightLevel").value = selectedConfiguration.lightLevel;
    document.getElementById("wateringInterval").value = selectedConfiguration.wateringInterval;
    document.getElementById("ventilationInterval").value = selectedConfiguration.ventilationInterval;

    renderPlantList();
}

async function saveConfiguration() {
    const configuration = {
        id: selectedConfiguration?.id || 0,
        name: document.getElementById("plantName").value,
        soilHumidity: Number(document.getElementById("soilHumidity").value),
        lightLevel: Number(document.getElementById("lightLevel").value),
        wateringInterval:
            Number(document.getElementById("wateringInterval").value),
        ventilationInterval:
            Number(document.getElementById("ventilationInterval").value)
    };
    await fetch(`${API_URL}/configuration`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(configuration)
    });
    await loadConfigurations();
}
async function applyConfiguration() {
    if (!selectedConfiguration)
        return;
    await fetch(`${API_URL}/configuration/current`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            configurationId: selectedConfiguration.id
        })
    });
    alert("Конфигурация применена");
}
async function loadState() {
    const response = await fetch(`${API_URL}/state`);
    const state = await response.json();
    document.getElementById("temperature").innerText = state.temperature;
    document.getElementById("waterLevel").innerText = state.waterLevel + "%";
    document.getElementById("stateSoilHumidity").innerText =
        state.soilHumidity + "%";
    document.getElementById("airHumidity").innerText = state.airHumidity +
        "%";
}
function addPlant() {
    selectedConfiguration = null;
    document.getElementById("plantName").value = "";
    document.getElementById("soilHumidity").value = "";
    document.getElementById("lightLevel").value = "";
    document.getElementById("wateringInterval").value = "";
    document.getElementById("ventilationInterval").value = "";
}
function deletePlant() {
    if (!selectedConfiguration)
        return;
    configurations = configurations.filter(x => x.id !==
        selectedConfiguration.id);
    selectedConfiguration = null;
    renderPlantList();
}
document.getElementById("saveBtn").onclick = saveConfiguration;
document.getElementById("applyBtn").onclick = applyConfiguration;
document.getElementById("addPlantBtn").onclick = addPlant;
document.getElementById("deletePlantBtn").onclick = deletePlant;
loadConfigurations();
loadState();
setInterval(loadState, 3000);


document.addEventListener("DOMContentLoaded", () => {

    const dialog =
        document.getElementById("addPlantDialog");

    document.getElementById("addPlantBtn").onclick = () => {

        document.getElementById("newPlantName").value = "";

        dialog.showModal();
    };

    document.getElementById("cancelAddPlant").onclick = () => {
        dialog.close();
    };

    let nextId = 1;

    document.getElementById("confirmAddPlant").onclick = () => {

        const plantName =
            document.getElementById("newPlantName")
                .value
                .trim();

        if (!plantName)
            return;

        
        const newPlant = {
            id: nextId++,
            name: plantName,
            soilHumidity: 0,
            lightLevel: 0,
            wateringInterval: 0,
            ventilationInterval: 0
        };

        configurations.push(newPlant);

        renderPlantList();

        dialog.close();
    };

});