const GAME_OF_LIFE_INDEX = 4;
const controlBindings = {};
let currentAnimationIndex = 0;

/************************************************
 * Logging
 ************************************************/
function log(message) {
  console.log(message);
  const logDiv = document.getElementById("log");
  if (!logDiv) return;
  logDiv.innerText += `${message}\n`;
  logDiv.scrollTop = logDiv.scrollHeight;
}

/************************************************
 * Debounce helper
 ************************************************/
function debounce(fn, wait = 200) {
  let timeout;
  return function (...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => fn.apply(this, args), wait);
  };
}

/************************************************
 * Sequential API queue
 ************************************************/
const apiQueue = {
  items: [],
  busy: false,

  add(path, onSuccess) {
    this.items.push({ path, onSuccess });
    this.process();
  },

  process() {
    if (this.busy || this.items.length === 0) {
      return;
    }

    this.busy = true;
    const { path, onSuccess } = this.items.shift();

    fetch(path)
      .then(async (response) => {
        const text = await response.text();
        if (!response.ok) {
          throw new Error(`${path} => ${response.status} ${response.statusText} :: ${text}`);
        }
        if (onSuccess) {
          onSuccess(text);
        } else {
          log(text);
        }
      })
      .catch((error) => {
        log(`API error: ${error.message}`);
      })
      .finally(() => {
        this.busy = false;
        setTimeout(() => this.process(), 120);
      });
  },
};

/************************************************
 * DOM helpers
 ************************************************/
const $ = (id) => document.getElementById(id);

function setValue(id, value) {
  const el = $(id);
  if (el) {
    el.value = value;
  }
}

function clamp(value, min, max) {
  let result = value;
  if (min !== undefined) result = Math.max(min, result);
  if (max !== undefined) result = Math.min(max, result);
  return result;
}

/************************************************
 * Slider bindings
 ************************************************/
function bindSliderToApi(key, config) {
  const slider = $(config.sliderId);
  const number = $(config.numberId);
  if (!slider || !number) {
    return;
  }

  const sendToApi = debounce((raw) => {
    const clamped = clamp(raw, config.min, config.max);
    const apiValue = config.toApi ? config.toApi(clamped) : clamped;
    apiQueue.add(`${config.endpoint}?val=${encodeURIComponent(apiValue)}`);
  }, config.debounce || 200);

  slider.addEventListener("input", () => {
    number.value = slider.value;
  });

  slider.addEventListener("change", () => {
    const raw = parseFloat(slider.value);
    const clamped = clamp(isNaN(raw) ? 0 : raw, config.min, config.max);
    slider.value = clamped;
    number.value = clamped;
    sendToApi(clamped);
  });

  number.addEventListener("input", () => {
    slider.value = number.value;
  });

  number.addEventListener("change", () => {
    const raw = parseFloat(number.value);
    const clamped = clamp(isNaN(raw) ? 0 : raw, config.min, config.max);
    number.value = clamped;
    slider.value = clamped;
    sendToApi(clamped);
  });

  controlBindings[key] = {
    setFromApi(apiValue) {
      const uiValue = config.fromApi ? config.fromApi(apiValue) : apiValue;
      const clamped = clamp(uiValue, config.min, config.max);
      slider.value = clamped;
      number.value = clamped;
    },
  };
}

function applyBindingValue(key, apiValue) {
  if (controlBindings[key] && controlBindings[key].setFromApi !== undefined) {
    controlBindings[key].setFromApi(apiValue);
  }
}

/************************************************
 * Game of Life toggle
 ************************************************/
function toggleGameOfLifeControls(show) {
  const container = $("columnSkipContainer");
  if (container) {
    container.style.display = show ? "flex" : "none";
  }
}

/************************************************
 * UI setup
 ************************************************/
function setupUIControls() {
  log("Setting up UI controls...");

  bindSliderToApi("brightness", {
    sliderId: "sliderBrightness",
    numberId: "numBrightness",
    endpoint: "/api/setBrightness",
    min: 0,
    max: 255,
  });

  bindSliderToApi("fade", {
    sliderId: "sliderFade",
    numberId: "numFade",
    endpoint: "/api/setFadeAmount",
    min: 0,
    max: 255,
  });

  bindSliderToApi("tail", {
    sliderId: "sliderTail",
    numberId: "numTail",
    endpoint: "/api/setTailLength",
    min: 0,
    max: 20,
  });

  bindSliderToApi("speed", {
    sliderId: "sliderSpeed",
    numberId: "numSpeed",
    endpoint: "/api/setSpeed",
    min: 0,
    max: 100,
  });

  bindSliderToApi("spawn", {
    sliderId: "sliderSpawn",
    numberId: "numSpawn",
    endpoint: "/api/setSpawnRate",
    min: 0,
    max: 100,
    toApi: (value) => (value / 100).toFixed(2),
    fromApi: (value) => Math.round(parseFloat(value) * 100),
  });

  bindSliderToApi("maxFlakes", {
    sliderId: "sliderMaxFlakes",
    numberId: "numMaxFlakes",
    endpoint: "/api/setMaxFlakes",
    min: 1,
    max: 100,
    toApi: (value) => Math.round(1 + (value - 1) * 5),
    fromApi: (apiValue) => {
      const parsed = parseFloat(apiValue);
      if (isNaN(parsed)) return 1;
      return Math.round(((parsed - 1) / 5) + 1);
    },
  });

  bindSliderToApi("columnSkip", {
    sliderId: "sliderColumnSkip",
    numberId: "numColumnSkip",
    endpoint: "/api/setColumnSkip",
    min: 1,
    max: 10,
  });

  setupPanelControls();
}

function setupPanelControls() {
  const panelCountSlider = $("sliderPanelCount");
  const numPanelCount = $("numPanelCount");
  if (panelCountSlider && numPanelCount) {
    panelCountSlider.addEventListener("input", () => {
      numPanelCount.value = panelCountSlider.value;
    });
    numPanelCount.addEventListener("input", () => {
      panelCountSlider.value = numPanelCount.value;
    });
  }

  const btnSetPanelCount = $("btnSetPanelCount");
  if (btnSetPanelCount && numPanelCount) {
    btnSetPanelCount.addEventListener("click", () => {
      let val = parseInt(numPanelCount.value, 10);
      if (isNaN(val)) val = 1;
      val = clamp(val, 1, 8);
      numPanelCount.value = val;
      panelCountSlider.value = val;
      apiQueue.add(`/api/setPanelCount?val=${val}`);
    });
  }

  const btnIdentifyPanels = $("btnIdentifyPanels");
  if (btnIdentifyPanels) {
    btnIdentifyPanels.addEventListener("click", () => {
      apiQueue.add("/api/identifyPanels");
    });
  }

  const btnPanelOrder = $("btnPanelOrder");
  const panelOrderSelect = $("panelOrder");
  if (btnPanelOrder && panelOrderSelect) {
    btnPanelOrder.addEventListener("click", () => {
      const order = panelOrderSelect.value;
      apiQueue.add(`/api/setPanelOrder?val=${encodeURIComponent(order)}`);
    });
  }

  const rotationButtons = [
    { buttonId: "btnRotatePanel1", selectId: "rotatePanel1", endpoint: "/api/rotatePanel1" },
    { buttonId: "btnRotatePanel2", selectId: "rotatePanel2", endpoint: "/api/rotatePanel2" },
  ];

  rotationButtons.forEach(({ buttonId, selectId, endpoint }) => {
    const button = $(buttonId);
    const select = $(selectId);
    if (button && select) {
      button.addEventListener("click", () => {
        const angle = parseInt(select.value, 10);
        if ([0, 90, 180, 270].includes(angle)) {
          apiQueue.add(`${endpoint}?val=${angle}`);
        } else {
          log(`Invalid rotation angle: ${angle}`);
        }
      });
    }
  });
}

/************************************************
 * Animation and palette loading
 ************************************************/
async function loadAnimations() {
  try {
    const response = await fetch("/api/listAnimations");
    if (!response.ok) {
      throw new Error(`listAnimations => ${response.status}`);
    }
    const data = await response.json();
    const select = $("selAnimation");
    if (!select) return;

    select.innerHTML = "";
    data.animations.forEach((name, index) => {
      const option = document.createElement("option");
      option.value = index;
      option.textContent = name;
      select.appendChild(option);
    });

    currentAnimationIndex = parseInt(data.current, 10) || 0;
    select.value = currentAnimationIndex;
    toggleGameOfLifeControls(currentAnimationIndex === GAME_OF_LIFE_INDEX);

    select.addEventListener("change", () => {
      const val = parseInt(select.value, 10);
      apiQueue.add(`/api/setAnimation?val=${val}`, () => {
        currentAnimationIndex = val;
        toggleGameOfLifeControls(val === GAME_OF_LIFE_INDEX);
        log(`Animation changed to ${select.options[select.selectedIndex].text}`);
      });
    });
  } catch (error) {
    log(`Failed to load animations: ${error.message}`);
  }
}

async function loadPalettes() {
  try {
    const response = await fetch("/api/listPalettes");
    if (!response.ok) {
      throw new Error(`listPalettes => ${response.status}`);
    }
    const data = await response.json();
    const select = $("paletteSelect");
    if (!select) return;

    select.innerHTML = "";
    data.palettes.forEach((name, index) => {
      const option = document.createElement("option");
      option.value = index;
      option.textContent = name;
      select.appendChild(option);
    });

    select.value = data.current;

    select.addEventListener("change", () => {
      const val = parseInt(select.value, 10);
      apiQueue.add(`/api/setPalette?val=${val}`, () => {
        log(`Palette changed to ${select.options[select.selectedIndex].text}`);
      });
    });
  } catch (error) {
    log(`Failed to load palettes: ${error.message}`);
  }
}

/************************************************
 * Settings loading
 ************************************************/
async function fetchText(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${url} => ${response.status} ${response.statusText}`);
  }
  return response.text();
}

async function fetchJson(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${url} => ${response.status} ${response.statusText}`);
  }
  return response.json();
}

async function loadSettings() {
  log("Loading current settings...");
  try {
    const brightness = parseInt(await fetchText("/api/getBrightness"), 10);
    if (!isNaN(brightness)) {
      applyBindingValue("brightness", brightness);
    }

    const fade = parseInt(await fetchText("/api/getFadeAmount"), 10);
    if (!isNaN(fade)) {
      applyBindingValue("fade", fade);
    }

    const tail = parseInt(await fetchText("/api/getTailLength"), 10);
    if (!isNaN(tail)) {
      applyBindingValue("tail", tail);
    }

    const spawn = parseFloat(await fetchText("/api/getSpawnRate"));
    if (!isNaN(spawn)) {
      applyBindingValue("spawn", spawn);
    }

    const maxFlakes = parseInt(await fetchText("/api/getMaxFlakes"), 10);
    if (!isNaN(maxFlakes)) {
      applyBindingValue("maxFlakes", maxFlakes);
    }

    const speed = parseInt(await fetchText("/api/getSpeed"), 10);
    if (!isNaN(speed)) {
      applyBindingValue("speed", speed);
    }

    try {
      const columnInfo = await fetchJson("/api/getColumnSkip");
      if (columnInfo && columnInfo.value !== undefined) {
        applyBindingValue("columnSkip", columnInfo.value);
      }
    } catch (err) {
      log(`Column skip fetch failed: ${err.message}`);
    }

    try {
      const panelInfo = await fetchJson("/api/getPanelCount");
      if (panelInfo && panelInfo.panelCount !== undefined) {
        setValue("sliderPanelCount", panelInfo.panelCount);
        setValue("numPanelCount", panelInfo.panelCount);
      }
    } catch (err) {
      log(`Panel count fetch failed: ${err.message}`);
    }
  } catch (error) {
    log(`Failed to load settings: ${error.message}`);
  }
}

/************************************************
 * WebSocket for live logs
 ************************************************/
function openLogSocket() {
  const host = window.location.hostname || "localhost";
  try {
    const socket = new WebSocket(`ws://${host}/ws`);
    socket.onopen = () => log("WebSocket connected");
    socket.onmessage = (event) => {
      log(event.data.trim());
    };
    socket.onclose = () => log("WebSocket disconnected");
    socket.onerror = (err) => log(`WebSocket error: ${err.message || err}`);
  } catch (error) {
    log(`Unable to open WebSocket: ${error.message}`);
  }
}

/************************************************
 * Initialization
 ************************************************/
async function initializeControlPanel() {
  setupUIControls();
  openLogSocket();

  await loadAnimations();
  await loadPalettes();
  await loadSettings();

  toggleGameOfLifeControls(currentAnimationIndex === GAME_OF_LIFE_INDEX);
  log("Control panel ready!");
}

document.addEventListener("DOMContentLoaded", () => {
  initializeControlPanel();
});
