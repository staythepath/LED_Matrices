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
 * Debounce function to prevent too many API calls
 ************************************************/
function debounce(func, wait) {
  let timeout;
  return function(...args) {
    const context = this;
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(context, args), wait);
  };
}

// Queue for API calls to prevent overwhelming the ESP32
const apiQueue = {
  queue: [],
  processing: false,
  
  add: function(url, callback) {
    this.queue.push({ url, callback });
    if (!this.processing) {
      this.processNext();
    }
  },
  
  processNext: function() {
    if (this.queue.length === 0) {
      this.processing = false;
      return;
    }
    
    this.processing = true;
    const { url, callback } = this.queue.shift();
    
    fetch(url)
      .then(r => r.text())
      .then(txt => {
        if (callback) callback(txt);
        // Wait 100ms between API calls to give ESP32 time to breathe
        setTimeout(() => this.processNext(), 100);
      })
      .catch(err => {
        log(`Error calling ${url}: ${err}`);
        // Continue processing even if there was an error
        setTimeout(() => this.processNext(), 100);
      });
  }
};

/************************************************
 * On window load, fetch initial data
 ************************************************/
window.addEventListener("load", () => {
  log("Page loaded, fetching data...");

  try {
    // Load animations first
    fetch("/api/listAnimations")
      .then(r => r.json())
      .then(data => {
        log("Got animations: " + JSON.stringify(data));
        try {
          const sel = document.getElementById("selAnimation");
          if (sel) {
            // Fill dropdown with animation options
            sel.innerHTML = "";
            
            // Check if data.animations exists
            if (!data.animations) {
              log("ERROR: No animations array in response");
              return;
            }
            
            log("Adding " + data.animations.length + " animation options...");
            data.animations.forEach((name, i) => {
              const opt = document.createElement("option");
              opt.value = i;
              opt.text = name;
              sel.appendChild(opt);
              log("Added animation: " + name);
            });

            // Set current selection
            log("Setting selected animation to: " + data.current);
            sel.value = data.current;

            // Add change handler
            sel.addEventListener("change", () => {
              const val = sel.options[sel.selectedIndex].value;
              apiQueue.add(`/api/setAnimation?val=${val}`, result => {
                log("Animation set to: " + result);
              });
            });
          }
        } catch (err) {
          log("Error populating animations: " + err.message);
        }

        // After animations load, get palettes
        return fetch("/api/listPalettes")
          .then(r => r.json());
      })
      .then(data => {
        if (data) {
          log("Got palettes: " + JSON.stringify(data));
          
          try {
            const sel = document.getElementById("paletteSelect");
            if (sel) {
              // Fill dropdown
              sel.innerHTML = "";
              
              // Check if data.palettes exists
              if (!data.palettes) {
                log("ERROR: No palettes array in response");
                return;
              }
              
              log("Adding " + data.palettes.length + " palette options...");
              data.palettes.forEach((name, i) => {
                const opt = document.createElement("option");
                opt.value = i;
                opt.text = name;
                sel.appendChild(opt);
                log("Added palette: " + name);
              });

              // Set current
              log("Setting selected palette to: " + data.current);
              sel.value = data.current;

              // Add change handler
              sel.addEventListener("change", () => {
                const val = sel.options[sel.selectedIndex].value;
                apiQueue.add(`/api/setPalette?val=${val}`, result => {
                  log("Palette set to: " + result);
                });
              });
            }
          } catch (err) {
            log("Error populating palettes: " + err.message);
          }
        }
        
        // After palettes, let's get current configuration for all controls
        return Promise.all([
          fetch("/api/getBrightness").then(r => r.text()),
          fetch("/api/getFadeAmount").then(r => r.text()),
          fetch("/api/getTailLength").then(r => r.text()),
          fetch("/api/getSpawnRate").then(r => r.text()),
          fetch("/api/getMaxFlakes").then(r => r.text()),
          fetch("/api/getSpeed").then(r => r.text()),
          fetch("/api/getPanelCount").then(r => r.text()),
          fetch("/api/getRotation?panel=PANEL1").then(r => r.text()),
          fetch("/api/getRotation?panel=PANEL2").then(r => r.text()),
          fetch("/api/getRotation?panel=PANEL3").then(r => r.text()),
          fetch("/api/getPanelOrder").then(r => r.text())
        ]);
      })
      .then(results => {
        if (results) {
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
            panelOrder
          ] = results;
          
          log("Got parameters:");
          log(`Brightness: ${brightness}`);
          log(`Fade: ${fade}`);
          log(`Tail: ${tail}`);
          log(`Spawn: ${spawn}`);
          log(`MaxFlakes: ${maxFlakes}`);
          log(`Speed: ${speed}`);
          log(`Panel Count: ${panelCount}`);
          log(`Panel 1 Rotation: ${rotation1}┬░`);
          log(`Panel 2 Rotation: ${rotation2}┬░`);
          log(`Panel 3 Rotation: ${rotation3}┬░`);
          log(`Panel Order: ${panelOrder}`);

          // Set UI values
          updateUIValue("sliderBrightness", "numBrightness", brightness);
          updateUIValue("sliderFade", "numFade", fade);
          updateUIValue("sliderTail", "numTail", tail);
          updateUIValue("sliderSpawn", "numSpawn", spawn);
          updateUIValue("sliderMaxFlakes", "numMaxFlakes", maxFlakes);
          updateSpeedUI(speed);
          updateUIValue("sliderPanelCount", "numPanelCount", panelCount);
          
          // Set panel rotation dropdowns
          if (document.getElementById("rotatePanel1")) {
            document.getElementById("rotatePanel1").value = rotation1;
          }
          if (document.getElementById("rotatePanel2")) {
            document.getElementById("rotatePanel2").value = rotation2;
          }
          if (document.getElementById("rotatePanel3")) {
            document.getElementById("rotatePanel3").value = rotation3;
          }
          if (document.getElementById("panelOrder")) {
            document.getElementById("panelOrder").value = panelOrder.toLowerCase();
          }
        }
      })
      .catch(err => {
        log("Error fetching data: " + err);
      });
  } catch (e) {
    log("Error in initialization: " + e.message);
  }
  
  // Setup panel rotation buttons
  setupPanelRotationControls();
});

/************************************************
 * Animations
 ************************************************/
// On-change => set the animation
const selAnimation = document.getElementById("selAnimation");
if (selAnimation) {
  selAnimation.addEventListener("change", debounce(() => {
    apiQueue.add(`/api/setAnimation?val=${selAnimation.value}`, txt => {
      log(txt);
    });
  }, 300));
}

/************************************************
 * Palettes
 ************************************************/
// On-change => set palette
const paletteSelect = document.getElementById("paletteSelect");
if (paletteSelect) {
  paletteSelect.addEventListener("change", debounce(() => {
    apiQueue.add(`/api/setPalette?val=${paletteSelect.value}`, txt => {
      log(txt);
    });
  }, 300));
}

/************************************************
 * Speed Slider
 ************************************************/
function sliderToSpeed(slVal) {
  // Non-linear mapping with better granularity at lower values
  // 0-50 on slider maps to 3-50ms (linear)
  // 50-100 on slider maps to 50-1500ms (quadratic)
  
  const sliderValue = parseFloat(slVal);
  
  if (sliderValue <= 50) {
    // Linear mapping for 0-50 slider values to 3-50ms
    return Math.round(3 + (sliderValue / 50) * 47);
  } else {
    // Quadratic mapping for 50-100 slider values to 50-1500ms
    const normalizedValue = (sliderValue - 50) / 50; // 0 to 1
    const quadraticValue = normalizedValue * normalizedValue; // Apply quadratic curve
    return Math.round(50 + quadraticValue * 1450);
  }
}

function speedToSliderVal(spd) {
  // Inverse of sliderToSpeed
  const speed = parseFloat(spd);
  
  if (speed <= 50) {
    // Inverse linear mapping for 3-50ms to 0-50 slider values
    return Math.round(((speed - 3) / 47) * 50);
  } else {
    // Inverse quadratic mapping for 50-1500ms to 50-100 slider values
    const normalizedValue = (speed - 50) / 1450; // 0 to 1
    const sqrtValue = Math.sqrt(normalizedValue); // Inverse of quadratic
    return Math.round(50 + sqrtValue * 50);
  }
}

const sSpeed = document.getElementById("sliderSpeed");
const tSpeed = document.getElementById("txtSpeed");
if (sSpeed && tSpeed) {
  // 'input' => real-time update
  sSpeed.addEventListener("input", () => {
    const spd = sliderToSpeed(sSpeed.value);
    tSpeed.value = spd;
  });

  // 'change' => call API
  sSpeed.addEventListener("change", debounce(() => {
    const spd = sliderToSpeed(sSpeed.value);
    tSpeed.value = spd;
    apiQueue.add(`/api/setSpeed?val=${spd}`, txt => {
      log(txt);
    });
  }, 300));

  // Update from text input
  tSpeed.addEventListener("input", () => {
    const spd = parseInt(tSpeed.value, 10);
    if (!isNaN(spd)) {
      sSpeed.value = speedToSliderVal(spd);
    }
  });

  tSpeed.addEventListener("change", debounce(() => {
    const spd = parseInt(tSpeed.value, 10);
    if (!isNaN(spd)) {
      apiQueue.add(`/api/setSpeed?val=${spd}`, txt => {
        log(txt);
      });
    }
  }, 300));
}

function updateSpeedUI(spd) {
  spd = parseInt(spd, 10);
  if (!isNaN(spd)) {
    if (tSpeed) tSpeed.value = spd;
    if (sSpeed) sSpeed.value = speedToSliderVal(spd);
  }
}

/************************************************
 * Sliders for: Brightness, Fade, Tail, Spawn, MaxFlakes
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

  // real-time sync from slider->number
  slider.addEventListener("input", () => {
    number.value = slider.value;
  });
  
  // call /api on 'change' with debounce
  slider.addEventListener("change", debounce(() => {
    number.value = slider.value;
    sendValueToApi(apiUrl, slider.value);
  }, 300));

  // real-time sync from number->slider
  number.addEventListener("input", () => {
    let val = isFloat ? parseFloat(number.value) : parseInt(number.value, 10);
    if (isNaN(val)) val = minVal;
    if (val < minVal) val = minVal;
    if (val > maxVal) val = maxVal;
    number.value = val;
    slider.value = val;
  });
  
  // call /api on 'change' with debounce
  number.addEventListener("change", debounce(() => {
    sendValueToApi(apiUrl, slider.value);
  }, 300));
}

function sendValueToApi(apiUrl, val) {
  apiQueue.add(`/api/${apiUrl}?val=${val}`, txt => {
    log(txt);
  });
}

// brightness, fade, tail, spawn, maxFlakes
bindSliderAndNumber(
  "sliderBrightness",
  "numBrightness",
  "setBrightness",
  0,
  255
);
bindSliderAndNumber("sliderFade", "numFade", "setFadeAmount", 0, 255);
bindSliderAndNumber("sliderTail", "numTail", "setTailLength", 1, 32);
bindSliderAndNumber("sliderSpawn", "numSpawn", "setSpawnRate", 0, 100);
bindSliderAndNumber(
  "sliderMaxFlakes",
  "numMaxFlakes",
  "setMaxFlakes",
  1,
  100
);

/************************************************
 * Panel Count
 ************************************************/
const sliderPanelCount = document.getElementById("sliderPanelCount");
const numPanelCount = document.getElementById("numPanelCount");
const btnSetPanelCount = document.getElementById("btnSetPanelCount");

if (sliderPanelCount && numPanelCount) {
  // real-time sync, no /api call
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
    apiQueue.add(`/api/setPanelCount?val=${val}`, txt => {
      log(txt);
    });
  });
}

/************************************************
 * Identify Panels
 ************************************************/
const btnIdentifyPanels = document.getElementById("btnIdentifyPanels");
if (btnIdentifyPanels) {
  btnIdentifyPanels.addEventListener("click", () => {
    apiQueue.add("/api/identifyPanels", txt => {
      log(txt);
    });
  });
}

/************************************************
 * Panel Rotation Controls
 ************************************************/
function setupPanelRotationControls() {
  // Panel 1 rotation
  const btnRotatePanel1 = document.getElementById("btnRotatePanel1");
  if (btnRotatePanel1) {
    btnRotatePanel1.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel1").value;
      apiQueue.add(`/api/rotatePanel1?val=${angle}`, result => {
        log(`Panel 1 rotation set to: ${angle}┬░ - ${result}`);
      });
    });
  }
  
  // Panel 2 rotation
  const btnRotatePanel2 = document.getElementById("btnRotatePanel2");
  if (btnRotatePanel2) {
    btnRotatePanel2.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel2").value;
      apiQueue.add(`/api/rotatePanel2?val=${angle}`, result => {
        log(`Panel 2 rotation set to: ${angle}┬░ - ${result}`);
      });
    });
  }
  
  // Panel 3 rotation
  const btnRotatePanel3 = document.getElementById("btnRotatePanel3");
  if (btnRotatePanel3) {
    btnRotatePanel3.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel3").value;
      apiQueue.add(`/api/rotatePanel3?val=${angle}`, result => {
        log(`Panel 3 rotation set to: ${angle}┬░ - ${result}`);
      });
    });
  }
  
  // Panel order
  const btnPanelOrder = document.getElementById("btnPanelOrder");
  if (btnPanelOrder) {
    btnPanelOrder.addEventListener("click", () => {
      const order = document.getElementById("panelOrder").value;
      apiQueue.add(`/api/setPanelOrder?val=${order}`, result => {
        log(`Panel order set to: ${order} - ${result}`);
      });
    });
  }
  
  // Set panel count button
  const btnSetPanelCount = document.getElementById("btnSetPanelCount");
  if (btnSetPanelCount) {
    btnSetPanelCount.addEventListener("click", () => {
      const count = document.getElementById("numPanelCount").value;
      apiQueue.add(`/api/setPanelCount?val=${count}`, result => {
        log(`Panel count set to: ${count} - ${result}`);
      });
    });
  }
  
  // Identify panels button
  const btnIdentifyPanels = document.getElementById("btnIdentifyPanels");
  if (btnIdentifyPanels) {
    btnIdentifyPanels.addEventListener("click", () => {
      apiQueue.add("/api/identifyPanels", result => {
        log("Identifying panels: " + result);
      });
    });
  }
}

// Helper function to update a UI value (slider + number input)
function updateUIValue(sliderId, numberId, value) {
  const slider = document.getElementById(sliderId);
  const number = document.getElementById(numberId);
  
  if (slider) slider.value = value;
  if (number) number.value = value;
}
