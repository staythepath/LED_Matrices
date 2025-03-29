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
        const sel = document.getElementById("selAnimation");
        if (!sel) return;
        sel.innerHTML = "";
        data.forEach((anim, idx) => {
          const opt = document.createElement("option");
          opt.value = idx;
          opt.innerText = `${idx + 1}: ${anim}`;
          sel.appendChild(opt);
        });
        
        // After animations are loaded, get current animation
        apiQueue.add("/api/getAnimation", val => {
          sel.value = val;
          
          // After animation is set, load palettes
          fetch("/api/listPalettes")
            .then(r => r.json())
            .then(data => {
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
              
              // After palettes are loaded, get current palette
              apiQueue.add("/api/getPalette", val => {
                paletteSelect.value = val;
                
                // After palette is set, load other settings sequentially
                apiQueue.add("/api/getBrightness", val => {
                  log("Brightness: " + val);
                  const sBrightness = document.getElementById("sliderBrightness");
                  const nBrightness = document.getElementById("numBrightness");
                  if (sBrightness && nBrightness) {
                    sBrightness.value = val;
                    nBrightness.value = val;
                  }
                  
                  apiQueue.add("/api/getFadeAmount", val => {
                    log("FadeAmount: " + val);
                    const sFade = document.getElementById("sliderFade");
                    const nFade = document.getElementById("numFade");
                    if (sFade && nFade) {
                      sFade.value = val;
                      nFade.value = val;
                    }
                    
                    apiQueue.add("/api/getTailLength", val => {
                      const sTail = document.getElementById("sliderTail");
                      const nTail = document.getElementById("numTail");
                      if (sTail && nTail) {
                        sTail.value = val;
                        nTail.value = val;
                      }
                      
                      apiQueue.add("/api/getSpawnRate", val => {
                        const sSpawn = document.getElementById("sliderSpawn");
                        const nSpawn = document.getElementById("numSpawn");
                        if (sSpawn && nSpawn) {
                          sSpawn.value = val;
                          nSpawn.value = val;
                        }
                        
                        apiQueue.add("/api/getMaxFlakes", val => {
                          const sMaxFlakes = document.getElementById("sliderMaxFlakes");
                          const nMaxFlakes = document.getElementById("numMaxFlakes");
                          if (sMaxFlakes && nMaxFlakes) {
                            sMaxFlakes.value = val;
                            nMaxFlakes.value = val;
                          }
                          
                          apiQueue.add("/api/getSpeed", val => {
                            log("Speed: " + val);
                            const spd = parseInt(val, 10);
                            if (!isNaN(spd)) {
                              updateSpeedUI(spd);
                            }
                            
                            apiQueue.add("/api/getPanelCount", val => {
                              const panelCount = document.getElementById("numPanelCount");
                              const sliderPanelCount = document.getElementById("sliderPanelCount");
                              if (panelCount && sliderPanelCount) {
                                panelCount.value = val;
                                sliderPanelCount.value = val;
                              }
                              log("Initial data loading complete");
                            });
                          });
                        });
                      });
                    });
                  });
                });
              });
            })
            .catch(err => {
              log("Error fetching /api/listPalettes: " + err);
            });
        });
      })
      .catch(err => {
        log("Error fetching /api/listAnimations: " + err);
      });
  } catch (e) {
    log("Error in initialization: " + e.message);
  }
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
