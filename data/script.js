/************************************************
 * Logging helper
 ************************************************/
function log(msg) {
  console.log(msg);
  const logDiv = document.getElementById("log");
  if (logDiv) {
    logDiv.textContent += msg + "\n";
    logDiv.scrollTop = logDiv.scrollHeight;
  }
}

function getApiToken() {
  return localStorage.getItem("apiToken") || "";
}

function authFetch(url, options = {}) {
  const headers = new Headers(options.headers || {});
  const token = getApiToken();
  if (token) {
    headers.set("X-API-Key", token);
  }
  return fetch(url, { ...options, headers });
}

/************************************************
 * Debounce utility
 ************************************************/
function debounce(func, wait) {
  let timeout;
  return function (...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(this, args), wait);
  };
}

/************************************************
 * API queue (avoids flooding the ESP32)
 ************************************************/
const apiQueue = {
  queue: [],
  processing: false,
  maxRetries: 3,

  add(url, callback, options = {}) {
    this.queue.push({ url, callback, retries: 0, options });
    if (!this.processing) this.processNext();
  },

  processNext() {
    if (this.queue.length === 0) {
      this.processing = false;
      return;
    }

    this.processing = true;
    const request = this.queue.shift();
    const { url, callback, retries, options } = request;

    authFetch(url, options)
      .then((response) => {
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`);
        }
        return response.text();
      })
      .then((txt) => {
        if (callback) callback(txt);
        setTimeout(() => this.processNext(), 250);
      })
      .catch((err) => {
        log(`Error calling ${url}: ${err}`);
        if (retries < this.maxRetries) {
          this.queue.unshift({ url, callback, retries: retries + 1, options });
          setTimeout(() => this.processNext(), 800 * Math.pow(2, retries));
        } else {
          log(`Failed after ${this.maxRetries} attempts: ${url}`);
          setTimeout(() => this.processNext(), 250);
        }
      });
  }
};

function apiSet(endpoint, value) {
  apiQueue.add(
    `/api/${endpoint}?val=${encodeURIComponent(value)}`,
    (txt) => {
      log(txt);
    },
    { method: "POST" }
  );
}

function apiCall(endpoint) {
  apiQueue.add(endpoint, (txt) => log(txt), { method: "POST" });
}

async function fetchText(url, fallback) {
  try {
    const resp = await authFetch(url);
    if (!resp.ok) return fallback;
    return await resp.text();
  } catch (err) {
    return fallback;
  }
}

/************************************************
 * UI helpers
 ************************************************/
function setRangePair(sliderId, numberId, value) {
  const slider = document.getElementById(sliderId);
  const number = document.getElementById(numberId);
  if (slider) slider.value = value;
  if (number) number.value = value;
}

function setToggle(id, value) {
  const toggle = document.getElementById(id);
  if (toggle) toggle.checked = value;
}

function setSelect(id, value) {
  const select = document.getElementById(id);
  if (select) select.value = value;
}

function bindRangePair(options) {
  const slider = document.getElementById(options.sliderId);
  const number = document.getElementById(options.numberId);
  if (!slider || !number) return;

  const clamp = (val) => {
    let parsed = options.float ? parseFloat(val) : parseInt(val, 10);
    if (isNaN(parsed)) parsed = options.min;
    if (parsed < options.min) parsed = options.min;
    if (parsed > options.max) parsed = options.max;
    if (options.float) {
      return parseFloat(parsed.toFixed(2));
    }
    return Math.round(parsed);
  };

  slider.addEventListener("input", () => {
    number.value = slider.value;
  });

  slider.addEventListener(
    "change",
    debounce(() => {
      const val = clamp(slider.value);
      slider.value = val;
      number.value = val;
      apiSet(options.api, val);
    }, 200)
  );

  number.addEventListener("input", () => {
    const val = clamp(number.value);
    slider.value = val;
    number.value = val;
  });

  number.addEventListener(
    "change",
    debounce(() => {
      const val = clamp(number.value);
      slider.value = val;
      number.value = val;
      apiSet(options.api, val);
    }, 200)
  );
}

function bindToggle(id, api) {
  const toggle = document.getElementById(id);
  if (!toggle) return;
  toggle.addEventListener("change", () => {
    apiSet(api, toggle.checked ? 1 : 0);
  });
}

function bindSelect(id, api) {
  const select = document.getElementById(id);
  if (!select) return;
  select.addEventListener("change", () => {
    apiSet(api, select.value);
  });
}

function bindText(id, api) {
  const input = document.getElementById(id);
  if (!input) return;
  input.addEventListener(
    "change",
    debounce(() => {
      apiSet(api, input.value);
    }, 200)
  );
}

/************************************************
 * Speed slider mapping
 ************************************************/
function sliderToSpeed(slVal) {
  const sliderValue = parseFloat(slVal);
  if (sliderValue <= 50) {
    return Math.round(3 + (sliderValue / 50) * 47);
  }
  const normalizedValue = (sliderValue - 50) / 50;
  const quadraticValue = normalizedValue * normalizedValue;
  return Math.round(50 + quadraticValue * 1450);
}

function speedToSliderVal(spd) {
  const speed = parseFloat(spd);
  if (speed <= 50) {
    return Math.round(((speed - 3) / 47) * 50);
  }
  const normalizedValue = (speed - 50) / 1450;
  const sqrtValue = Math.sqrt(normalizedValue);
  return Math.round(50 + sqrtValue * 50);
}

function bindSpeedControl() {
  const slider = document.getElementById("sliderSpeed");
  const number = document.getElementById("txtSpeed");
  if (!slider || !number) return;

  slider.addEventListener("input", () => {
    number.value = sliderToSpeed(slider.value);
  });

  slider.addEventListener(
    "change",
    debounce(() => {
      const speed = sliderToSpeed(slider.value);
      number.value = speed;
      apiSet("setSpeed", speed);
    }, 250)
  );

  number.addEventListener("input", () => {
    const speed = parseInt(number.value, 10);
    if (!isNaN(speed)) {
      slider.value = speedToSliderVal(speed);
    }
  });

  number.addEventListener(
    "change",
    debounce(() => {
      const speed = parseInt(number.value, 10);
      if (!isNaN(speed)) {
        apiSet("setSpeed", speed);
      }
    }, 250)
  );
}

/************************************************
 * Animation visibility
 ************************************************/
function updateAnimationVisibility(name) {
  document.querySelectorAll(".anim-group").forEach((group) => {
    group.classList.toggle("active", group.dataset.anim === name);
  });

  document.querySelectorAll("[data-anim]").forEach((el) => {
    if (el.classList.contains("anim-group")) return;
    el.style.display = el.dataset.anim === name ? "" : "none";
  });
}

/************************************************
 * Data loading
 ************************************************/
async function loadAnimations() {
  const res = await authFetch("/api/listAnimations");
  const data = await res.json();
  const select = document.getElementById("selAnimation");
  if (!select) return;

  select.innerHTML = "";
  data.animations.forEach((name, i) => {
    const option = document.createElement("option");
    option.value = i;
    option.textContent = name;
    select.appendChild(option);
  });

  select.value = data.current;
  updateAnimationVisibility(data.animations[data.current]);

  select.addEventListener("change", () => {
    const val = select.value;
    apiSet("setAnimation", val);
    updateAnimationVisibility(data.animations[val]);
  });
}

async function loadPalettes() {
  const res = await authFetch("/api/listPalettes");
  const data = await res.json();
  const select = document.getElementById("paletteSelect");
  if (!select) return;

  select.innerHTML = "";
  data.palettes.forEach((name, i) => {
    const option = document.createElement("option");
    option.value = i;
    option.textContent = name;
    select.appendChild(option);
  });

  select.value = data.current;
  select.addEventListener("change", () => apiSet("setPalette", select.value));
}

async function loadLifeRules() {
  const select = document.getElementById("lifeRule");
  if (!select) return;
  const res = await authFetch("/api/listLifeRules");
  const data = await res.json();
  select.innerHTML = "";
  data.rules.forEach((rule, i) => {
    const option = document.createElement("option");
    option.value = i;
    option.textContent = rule;
    select.appendChild(option);
  });
  select.value = data.current;
  select.addEventListener("change", () => apiSet("setLifeRule", select.value));
}

async function loadSettings() {
  const values = await Promise.all([
    fetchText("/api/getBrightness", "30"),
    fetchText("/api/getFadeAmount", "20"),
    fetchText("/api/getTailLength", "3"),
    fetchText("/api/getSpawnRate", "0.2"),
    fetchText("/api/getMaxFlakes", "50"),
    fetchText("/api/getSpeed", "30"),
    fetchText("/api/getPanelCount", "2"),
    fetchText("/api/getRotation?panel=PANEL1", "90"),
    fetchText("/api/getRotation?panel=PANEL2", "90"),
    fetchText("/api/getRotation?panel=PANEL3", "90"),
    fetchText("/api/getPanelOrder", "left"),
    fetchText("/api/getLifeDensity", "33"),
    fetchText("/api/getLifeStagnation", "45"),
    fetchText("/api/getLifeWrap", "1"),
    fetchText("/api/getLifeColorMode", "0"),
    fetchText("/api/getAntRule", "LR"),
    fetchText("/api/getAntCount", "1"),
    fetchText("/api/getAntSteps", "8"),
    fetchText("/api/getAntWrap", "1"),
    fetchText("/api/getCarpetDepth", "4"),
    fetchText("/api/getCarpetShift", "2"),
    fetchText("/api/getCarpetInvert", "0"),
    fetchText("/api/getFireworkMax", "10"),
    fetchText("/api/getFireworkParticles", "40"),
    fetchText("/api/getFireworkGravity", "0.15"),
    fetchText("/api/getFireworkLaunch", "0.15"),
    fetchText("/api/getRainbowHueScale", "4")
  ]);

  const [
    brightness,
    fade,
    tail,
    spawn,
    maxFlakes,
    speed,
    panelCount,
    rotation1,
    rotation2,
    rotation3,
    panelOrder,
    lifeDensity,
    lifeStagnation,
    lifeWrap,
    lifeColorMode,
    antRule,
    antCount,
    antSteps,
    antWrap,
    carpetDepth,
    carpetShift,
    carpetInvert,
    fireworkMax,
    fireworkParticles,
    fireworkGravity,
    fireworkLaunch,
    rainbowHueScale
  ] = values;

  let panelCountValue = panelCount;
  if (typeof panelCountValue === "string" && panelCountValue.trim().startsWith("{")) {
    try {
      const obj = JSON.parse(panelCountValue);
      if (obj.panelCount !== undefined) {
        panelCountValue = obj.panelCount;
      }
    } catch (err) {
      panelCountValue = panelCount;
    }
  }

  setRangePair("sliderBrightness", "numBrightness", brightness);
  setRangePair("sliderFade", "numFade", fade);
  setRangePair("sliderTail", "numTail", tail);
  setRangePair("sliderSpawn", "numSpawn", spawn);
  setRangePair("sliderMaxFlakes", "numMaxFlakes", maxFlakes);

  const speedNumber = document.getElementById("txtSpeed");
  const speedSlider = document.getElementById("sliderSpeed");
  if (speedNumber) speedNumber.value = parseInt(speed, 10);
  if (speedSlider) speedSlider.value = speedToSliderVal(speed);

  setRangePair("sliderPanelCount", "numPanelCount", panelCountValue);
  setSelect("rotatePanel1", rotation1);
  setSelect("rotatePanel2", rotation2);
  setSelect("rotatePanel3", rotation3);
  setSelect("panelOrder", String(panelOrder).trim().toLowerCase());

  setRangePair("lifeDensity", "lifeDensityVal", lifeDensity);
  setRangePair("lifeStagnation", "lifeStagnationVal", lifeStagnation);
  setSelect("lifeColorMode", lifeColorMode);
  setToggle("lifeWrap", lifeWrap === "1" || lifeWrap === "true");

  const antRuleInput = document.getElementById("antRule");
  if (antRuleInput) antRuleInput.value = antRule;
  setRangePair("antCount", "antCountVal", antCount);
  setRangePair("antSteps", "antStepsVal", antSteps);
  setToggle("antWrap", antWrap === "1" || antWrap === "true");

  setRangePair("carpetDepth", "carpetDepthVal", carpetDepth);
  setRangePair("carpetShift", "carpetShiftVal", carpetShift);
  setToggle("carpetInvert", carpetInvert === "1" || carpetInvert === "true");

  setRangePair("fireworkMax", "fireworkMaxVal", fireworkMax);
  setRangePair("fireworkParticles", "fireworkParticlesVal", fireworkParticles);
  setRangePair("fireworkGravity", "fireworkGravityVal", fireworkGravity);
  setRangePair("fireworkLaunch", "fireworkLaunchVal", fireworkLaunch);

  setRangePair("rainbowHueScale", "rainbowHueScaleVal", rainbowHueScale);
}

async function refreshConnectionStatus() {
  const pill = document.getElementById("connectionPill");
  if (!pill) return;
  try {
    const res = await authFetch("/api/status");
    if (!res.ok) throw new Error("status");
    const data = await res.json();
    if (data.wifi && data.wifi.connected) {
      pill.textContent = "Online";
      pill.classList.remove("offline");
    } else {
      pill.textContent = "Offline";
      pill.classList.add("offline");
    }
  } catch (err) {
    pill.textContent = "Offline";
    pill.classList.add("offline");
  }
}

/************************************************
 * Control bindings
 ************************************************/
function setupControls() {
  const tokenInput = document.getElementById("apiToken");
  const saveTokenButton = document.getElementById("saveApiToken");
  if (tokenInput) {
    tokenInput.value = getApiToken();
  }
  if (saveTokenButton && tokenInput) {
    saveTokenButton.addEventListener("click", () => {
      localStorage.setItem("apiToken", tokenInput.value.trim());
      log("API token saved.");
    });
  }

  bindSpeedControl();

  bindRangePair({
    sliderId: "sliderBrightness",
    numberId: "numBrightness",
    api: "setBrightness",
    min: 0,
    max: 255
  });

  bindRangePair({
    sliderId: "sliderFade",
    numberId: "numFade",
    api: "setFadeAmount",
    min: 0,
    max: 255
  });

  bindRangePair({
    sliderId: "sliderTail",
    numberId: "numTail",
    api: "setTailLength",
    min: 1,
    max: 30
  });

  bindRangePair({
    sliderId: "sliderSpawn",
    numberId: "numSpawn",
    api: "setSpawnRate",
    min: 0,
    max: 1,
    float: true
  });

  bindRangePair({
    sliderId: "sliderMaxFlakes",
    numberId: "numMaxFlakes",
    api: "setMaxFlakes",
    min: 10,
    max: 500
  });

  // Panel controls
  const panelSlider = document.getElementById("sliderPanelCount");
  const panelNumber = document.getElementById("numPanelCount");
  if (panelSlider && panelNumber) {
    panelSlider.addEventListener("input", () => {
      panelNumber.value = panelSlider.value;
    });
    panelNumber.addEventListener("input", () => {
      panelSlider.value = panelNumber.value;
    });
  }

  const btnSetPanelCount = document.getElementById("btnSetPanelCount");
  if (btnSetPanelCount) {
    btnSetPanelCount.addEventListener("click", () => {
      const count = document.getElementById("numPanelCount").value;
      apiSet("setPanelCount", count);
    });
  }

  const btnPanelOrder = document.getElementById("btnPanelOrder");
  if (btnPanelOrder) {
    btnPanelOrder.addEventListener("click", () => {
      const order = document.getElementById("panelOrder").value;
      apiSet("setPanelOrder", order);
    });
  }

  const btnIdentify = document.getElementById("btnIdentifyPanels");
  if (btnIdentify) {
    btnIdentify.addEventListener("click", () => apiCall("/api/identifyPanels"));
  }

  const btnSwap = document.getElementById("btnSwapPanels");
  if (btnSwap) {
    btnSwap.addEventListener("click", () => {
      apiCall("/api/swapPanels");
      const orderSelect = document.getElementById("panelOrder");
      if (orderSelect) {
        orderSelect.value = orderSelect.value === "left" ? "right" : "left";
      }
    });
  }

  const btnRotatePanel1 = document.getElementById("btnRotatePanel1");
  if (btnRotatePanel1) {
    btnRotatePanel1.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel1").value;
      apiSet("rotatePanel1", angle);
    });
  }

  const btnRotatePanel2 = document.getElementById("btnRotatePanel2");
  if (btnRotatePanel2) {
    btnRotatePanel2.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel2").value;
      apiSet("rotatePanel2", angle);
    });
  }

  const btnRotatePanel3 = document.getElementById("btnRotatePanel3");
  if (btnRotatePanel3) {
    btnRotatePanel3.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel3").value;
      apiSet("rotatePanel3", angle);
    });
  }

  // Life controls
  bindRangePair({
    sliderId: "lifeDensity",
    numberId: "lifeDensityVal",
    api: "setLifeDensity",
    min: 0,
    max: 100
  });
  bindRangePair({
    sliderId: "lifeStagnation",
    numberId: "lifeStagnationVal",
    api: "setLifeStagnation",
    min: 5,
    max: 250
  });
  bindSelect("lifeColorMode", "setLifeColorMode");
  bindToggle("lifeWrap", "setLifeWrap");

  const lifeReseed = document.getElementById("lifeReseed");
  if (lifeReseed) {
    lifeReseed.addEventListener("click", () => apiCall("/api/lifeReseed"));
  }

  // Ant controls
  bindText("antRule", "setAntRule");
  bindRangePair({
    sliderId: "antCount",
    numberId: "antCountVal",
    api: "setAntCount",
    min: 1,
    max: 6
  });
  bindRangePair({
    sliderId: "antSteps",
    numberId: "antStepsVal",
    api: "setAntSteps",
    min: 1,
    max: 50
  });
  bindToggle("antWrap", "setAntWrap");

  // Carpet controls
  bindRangePair({
    sliderId: "carpetDepth",
    numberId: "carpetDepthVal",
    api: "setCarpetDepth",
    min: 1,
    max: 6
  });
  bindRangePair({
    sliderId: "carpetShift",
    numberId: "carpetShiftVal",
    api: "setCarpetShift",
    min: 1,
    max: 20
  });
  bindToggle("carpetInvert", "setCarpetInvert");

  // Firework controls
  bindRangePair({
    sliderId: "fireworkMax",
    numberId: "fireworkMaxVal",
    api: "setFireworkMax",
    min: 1,
    max: 25
  });
  bindRangePair({
    sliderId: "fireworkParticles",
    numberId: "fireworkParticlesVal",
    api: "setFireworkParticles",
    min: 10,
    max: 120
  });
  bindRangePair({
    sliderId: "fireworkGravity",
    numberId: "fireworkGravityVal",
    api: "setFireworkGravity",
    min: 0.01,
    max: 0.5,
    float: true
  });
  bindRangePair({
    sliderId: "fireworkLaunch",
    numberId: "fireworkLaunchVal",
    api: "setFireworkLaunch",
    min: 0.01,
    max: 1,
    float: true
  });

  // Rainbow
  bindRangePair({
    sliderId: "rainbowHueScale",
    numberId: "rainbowHueScaleVal",
    api: "setRainbowHueScale",
    min: 1,
    max: 12
  });
}

/************************************************
 * Init
 ************************************************/
document.addEventListener("DOMContentLoaded", async () => {
  log("Loading control panel...");
  setupControls();
  await loadAnimations();
  await loadPalettes();
  await loadLifeRules();
  await loadSettings();
  await refreshConnectionStatus();
  log("Control panel ready.");
});
