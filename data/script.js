/************************************************
 * Logging helper
 ************************************************/
function log(msg) {
  console.log(msg);
  const logDiv = document.getElementById("log");
  if (logDiv) {
    logDiv.innerText += msg + "\n";
    logDiv.scrollTop = logDiv.scrollHeight;
  }
}

/************************************************
 * 1) On window load, fetch animations, palettes,
 *    brightness, speed, panelCount, etc.
 ************************************************/
window.addEventListener("load", () => {
  log("Page loaded, fetching data...");

  // 1) Animations
  loadAnimationList();

  // 2) Palettes
  fetch("/api/listPalettes")
    .then((r) => r.json())
    .then((data) => {
      log("Got palettes: " + JSON.stringify(data));
      const paletteSelect = document.getElementById("paletteSelect");
      if (!paletteSelect) return;
      paletteSelect.innerHTML = "";
      data.forEach((p, idx) => {
        const opt = document.createElement("option");
        opt.value = idx;
        opt.innerText = `${idx + 1}: ${p}`;
        paletteSelect.appendChild(opt);
      });
      getPalette(); // fetch the current palette index from server
    })
    .catch((err) => log("Error fetching /api/listPalettes: " + err));

  // 3) Brightness
  fetch("/api/getBrightness")
    .then((r) => r.text())
    .then((val) => {
      log("Brightness: " + val);
      const sBrightness = document.getElementById("sliderBrightness");
      const nBrightness = document.getElementById("numBrightness");
      if (sBrightness && nBrightness) {
        sBrightness.value = val;
        nBrightness.value = val;
      }
    });

  // 4) Fade
  fetch("/api/getFadeAmount")
    .then((r) => r.text())
    .then((val) => {
      log("FadeAmount: " + val);
      const sFade = document.getElementById("sliderFade");
      const nFade = document.getElementById("numFade");
      if (sFade && nFade) {
        sFade.value = val;
        nFade.value = val;
      }
    });

  // 5) Tail
  fetch("/api/getTailLength")
    .then((r) => r.text())
    .then((val) => {
      const sTail = document.getElementById("sliderTail");
      const nTail = document.getElementById("numTail");
      if (sTail && nTail) {
        sTail.value = val;
        nTail.value = val;
      }
    });

  // 6) Spawn
  fetch("/api/getSpawnRate")
    .then((r) => r.text())
    .then((val) => {
      const sSpawn = document.getElementById("sliderSpawn");
      const nSpawn = document.getElementById("numSpawn");
      if (sSpawn && nSpawn) {
        sSpawn.value = val;
        nSpawn.value = val;
      }
    });

  // 7) MaxFlakes
  fetch("/api/getMaxFlakes")
    .then((r) => r.text())
    .then((val) => {
      const sMaxFlakes = document.getElementById("sliderMaxFlakes");
      const nMaxFlakes = document.getElementById("numMaxFlakes");
      if (sMaxFlakes && nMaxFlakes) {
        sMaxFlakes.value = val;
        nMaxFlakes.value = val;
      }
    });

  // 8) Speed
  fetch("/api/getSpeed")
    .then((r) => r.text())
    .then((val) => {
      const spd = parseInt(val, 10);
      if (isNaN(spd)) return;
      updateSpeedUI(spd);
    })
    .catch((err) => log("getSpeed error: " + err));

  // 9) Panel Count
  getPanelCount();
});

/************************************************
 * 2) Animations
 ************************************************/
function loadAnimationList() {
  fetch("/api/listAnimations")
    .then((r) => r.json())
    .then((data) => {
      log("Got animation list: " + JSON.stringify(data));
      const selAnimation = document.getElementById("selAnimation");
      if (!selAnimation) return;
      selAnimation.innerHTML = "";
      data.forEach((animName, idx) => {
        const opt = document.createElement("option");
        opt.value = idx;
        opt.innerText = `${idx}: ${animName}`;
        selAnimation.appendChild(opt);
      });
    })
    .catch((err) => log("Error fetching /api/listAnimations: " + err));
}

// On-change => set the animation
const selAnimation = document.getElementById("selAnimation");
if (selAnimation) {
  selAnimation.addEventListener("change", () => {
    const val = selAnimation.value;
    log("Animation changed to " + val);
    fetch(`/api/setAnimation?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setAnimation error: " + err));
  });
}

// Example: get current palette
function getPalette() {
  fetch("/api/getPalette")
    .then((r) => r.json())
    .then((obj) => {
      log("Current palette index: " + obj.current);
      const paletteSelect = document.getElementById("paletteSelect");
      if (paletteSelect) {
        paletteSelect.value = obj.current;
      }
    })
    .catch((err) => log("Palette fetch error: " + err));
}

// On-change => set palette
const paletteSelect = document.getElementById("paletteSelect");
if (paletteSelect) {
  paletteSelect.addEventListener("change", () => {
    const val = paletteSelect.value;
    log("Palette changed to " + val);
    fetch(`/api/setPalette?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setPalette error: " + err));
  });
}

/************************************************
 * 3) Speed Slider
 ************************************************/
function sliderToSpeed(slVal) {
  const minSpeed = 10,
    maxSpeed = 60000;
  const minLog = Math.log(minSpeed),
    maxLog = Math.log(maxSpeed);
  const fraction = slVal / 100.0;
  const scale = minLog + (maxLog - minLog) * fraction;
  return Math.round(Math.exp(scale));
}
function speedToSliderVal(spd) {
  const minSpeed = 10,
    maxSpeed = 60000;
  const minLog = Math.log(minSpeed),
    maxLog = Math.log(maxSpeed);
  if (spd < minSpeed) spd = minSpeed;
  if (spd > maxSpeed) spd = maxSpeed;
  const scale = Math.log(spd);
  const fraction = (scale - minLog) / (maxLog - minLog);
  return Math.round(fraction * 100);
}

const sSpeed = document.getElementById("sliderSpeed");
const tSpeed = document.getElementById("txtSpeed");
if (sSpeed && tSpeed) {
  // Move slider => update number in real time
  sSpeed.addEventListener("input", () => {
    tSpeed.value = sliderToSpeed(parseInt(sSpeed.value, 10));
  });
  // Let go of slider => call /api
  sSpeed.addEventListener("change", () => {
    const val = sliderToSpeed(parseInt(sSpeed.value, 10));
    tSpeed.value = val;
    fetch(`/api/setSpeed?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setSpeed error:" + err));
  });
  // Number box => on input => update slider, on change => call /api
  tSpeed.addEventListener("input", () => {
    let typedVal = parseInt(tSpeed.value, 10);
    if (isNaN(typedVal)) typedVal = 10;
    if (typedVal < 10) typedVal = 10;
    if (typedVal > 60000) typedVal = 60000;
    sSpeed.value = speedToSliderVal(typedVal);
  });
  tSpeed.addEventListener("change", () => {
    let typedVal = parseInt(tSpeed.value, 10);
    if (isNaN(typedVal)) typedVal = 10;
    if (typedVal < 10) typedVal = 10;
    if (typedVal > 60000) typedVal = 60000;
    updateSpeedUI(typedVal);
    fetch(`/api/setSpeed?val=${typedVal}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setSpeed error:" + err));
  });
}

function updateSpeedUI(spd) {
  if (!isNaN(spd)) {
    if (tSpeed) tSpeed.value = spd;
    if (sSpeed) sSpeed.value = speedToSliderVal(spd);
  }
}

/************************************************
 * 4) Sliders for Brightness, Fade, Tail, Spawn, MaxFlakes
 *    same pattern: 'input' => sync the number, 'change' => call /api
 ************************************************/
function bindSliderAndNumber(
  sliderId,
  numberId,
  apiUrl,
  minVal,
  maxVal,
  isFloat = false
) {
  const slider = document.getElementById(sliderId);
  const number = document.getElementById(numberId);
  if (!slider || !number) return;

  // 'input' => update text box in real time
  slider.addEventListener("input", () => {
    number.value = slider.value;
  });
  // 'change' => call /api
  slider.addEventListener("change", () => {
    number.value = slider.value;
    sendValueToApi(apiUrl, slider.value);
  });

  // text box => 'input' => update slider in real time
  number.addEventListener("input", () => {
    let val = isFloat ? parseFloat(number.value) : parseInt(number.value, 10);
    if (isNaN(val)) val = minVal;
    if (val < minVal) val = minVal;
    if (val > maxVal) val = maxVal;
    number.value = val;
    slider.value = val;
  });
  // text box => 'change' => call /api
  number.addEventListener("change", () => {
    sendValueToApi(apiUrl, slider.value);
  });
}

function sendValueToApi(apiUrl, val) {
  fetch(`/api/${apiUrl}?val=${val}`)
    .then((r) => r.text())
    .then((txt) => log(txt))
    .catch((err) => log(`Error calling ${apiUrl}: ` + err));
}

// Apply to your brightness, fade, tail, spawn, maxFlakes
bindSliderAndNumber(
  "sliderBrightness",
  "numBrightness",
  "setBrightness",
  0,
  255
);
bindSliderAndNumber("sliderFade", "numFade", "setFadeAmount", 0, 255);
bindSliderAndNumber("sliderTail", "numTail", "setTailLength", 1, 30);
bindSliderAndNumber("sliderSpawn", "numSpawn", "setSpawnRate", 0, 1, true);
bindSliderAndNumber("sliderMaxFlakes", "numMaxFlakes", "setMaxFlakes", 10, 500);

/************************************************
 * 5) Panel Count (needs an Apply button)
 ************************************************/
const sliderPanelCount = document.getElementById("sliderPanelCount");
const numPanelCount = document.getElementById("numPanelCount");
const btnSetPanelCount = document.getElementById("btnSetPanelCount");

if (sliderPanelCount && numPanelCount) {
  // Real-time sync, but do NOT call /api on slider change
  sliderPanelCount.addEventListener("input", () => {
    numPanelCount.value = sliderPanelCount.value;
  });
  numPanelCount.addEventListener("input", () => {
    sliderPanelCount.value = numPanelCount.value;
  });
}
if (btnSetPanelCount) {
  btnSetPanelCount.addEventListener("click", () => {
    let val = parseInt(numPanelCount.value, 10);
    if (isNaN(val)) val = 1;
    if (val < 1) val = 1;
    if (val > 8) val = 8;
    fetch(`/api/setPanelCount?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setPanelCount error:" + err));
  });
}

function getPanelCount() {
  fetch("/api/getPanelCount")
    .then((r) => r.json())
    .then((obj) => {
      log("PanelCount: " + obj.panelCount);
      if (sliderPanelCount && numPanelCount) {
        sliderPanelCount.value = obj.panelCount;
        numPanelCount.value = obj.panelCount;
      }
    })
    .catch((err) => log("getPanelCount error:" + err));
}

/************************************************
 * 6) Panel Order, Rotation, etc. (unchanged)
 ************************************************/
const selPanelOrder = document.getElementById("selPanelOrder");
const btnSetPanelOrder = document.getElementById("btnSetPanelOrder");
if (btnSetPanelOrder) {
  btnSetPanelOrder.addEventListener("click", () => {
    const val = selPanelOrder.value;
    fetch(`/api/setPanelOrder?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("setPanelOrder error:" + err));
  });
}

const selRotateP1 = document.getElementById("selRotateP1");
const btnRotateP1 = document.getElementById("btnRotateP1");
if (btnRotateP1) {
  btnRotateP1.addEventListener("click", () => {
    const val = selRotateP1.value;
    fetch(`/api/rotatePanel1?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("rotatePanel1 error:" + err));
  });
}

const selRotateP2 = document.getElementById("selRotateP2");
const btnRotateP2 = document.getElementById("btnRotateP2");
if (btnRotateP2) {
  btnRotateP2.addEventListener("click", () => {
    const val = selRotateP2.value;
    fetch(`/api/rotatePanel2?val=${val}`)
      .then((r) => r.text())
      .then((txt) => log(txt))
      .catch((err) => log("rotatePanel2 error:" + err));
  });
}
