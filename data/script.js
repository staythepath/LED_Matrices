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
  maxRetries: 3,
  
  add: function(url, callback) {
    this.queue.push({ url, callback, retries: 0 });
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
    const request = this.queue.shift();
    const { url, callback, retries } = request;
    
    log(`Sending request to ${url}...`);
    
    fetch(url)
      .then(response => {
        if (!response.ok) {
          throw new Error(`Server returned ${response.status}: ${response.statusText}`);
        }
        return response.text();
      })
      .then(txt => {
        if (callback) callback(txt);
        // Wait 300ms between API calls to give ESP32 more breathing room
        setTimeout(() => this.processNext(), 300);
      })
      .catch(err => {
        log(`Error calling ${url}: ${err}`);
        
        // Retry logic for failed requests
        if (retries < this.maxRetries) {
          log(`Retrying request (${retries + 1}/${this.maxRetries})...`);
          // Put back in queue with incremented retry count
          this.queue.unshift({ 
            url, 
            callback, 
            retries: retries + 1 
          });
          // Wait longer between retries (exponential backoff with higher initial delay)
          setTimeout(() => this.processNext(), 1000 * Math.pow(2, retries));
        } else {
          log(`Failed after ${this.maxRetries} attempts. Giving up.`);
          // Show error to user
          const errorDiv = document.createElement('div');
          errorDiv.className = 'error-message';
          errorDiv.textContent = `Failed to communicate with LED controller. Try refreshing the page.`;
          document.body.prepend(errorDiv);
          setTimeout(() => {
            if (errorDiv.parentNode) {
              errorDiv.parentNode.removeChild(errorDiv);
            }
          }, 5000);
          
          // Continue processing other requests
          setTimeout(() => this.processNext(), 300);
        }
      });
  }
};

/************************************************
 * On window load, fetch initial data
 ************************************************/
document.addEventListener("DOMContentLoaded", () => {
  log("DOM loaded, initializing app...");
  
  // Add loading overlay to prevent interactions until fully loaded
  showLoadingOverlay();
  
  // Setup UI controls immediately, they'll work as soon as settings are loaded
  setupUIControls();
  
  // Load everything in a single fast sequence
  loadEverything();
});

// Show loading overlay to prevent premature interactions
function showLoadingOverlay() {
  const overlay = document.createElement('div');
  overlay.id = 'loading-overlay';
  overlay.innerHTML = `
    <div class="loading-container">
      <div class="loading-spinner"></div>
      <p>Loading LED Control Panel...</p>
    </div>
  `;
  
  // Add overlay styles
  const style = document.createElement('style');
  style.textContent = `
    #loading-overlay {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0,0,0,0.7);
      z-index: 1000;
      display: flex;
      justify-content: center;
      align-items: center;
      color: white;
      font-size: 18px;
    }
    .loading-container {
      text-align: center;
    }
    .loading-spinner {
      border: 5px solid #f3f3f3;
      border-top: 5px solid #3498db;
      border-radius: 50%;
      width: 50px;
      height: 50px;
      margin: 0 auto 20px auto;
      animation: spin 1s linear infinite;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
  `;
  
  document.head.appendChild(style);
  document.body.appendChild(overlay);
}

// Hide loading overlay when ready
function hideLoadingOverlay() {
  const overlay = document.getElementById('loading-overlay');
  if (overlay) {
    overlay.style.opacity = '0';
    overlay.style.transition = 'opacity 0.5s ease';
    setTimeout(() => {
      if (overlay.parentNode) {
        overlay.parentNode.removeChild(overlay);
      }
    }, 500);
  }
}

// Simplified loading process - load everything at once
function loadEverything() {
  log("Loading system settings and controls...");
  
  // Load settings first
  loadSettings(() => {
    // Then load animations
    loadAnimations(() => {
      // Then load palettes last
      loadPalettes(() => {
        // We're finally done loading everything
        log("Control panel ready!");
        hideLoadingOverlay();
      });
    });
  });
}

// Setup all UI controls and event listeners
function setupUIControls() {
  log("Setting up UI controls...");
  
  // Set up panel rotation controls
  setupPanelRotationControls();
  
  // Set up the slider for brightness
  const brightnessSlider = document.getElementById("sliderBrightness");
  if (brightnessSlider) {
    brightnessSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numBrightness", value);
      apiQueue.add(`/api/setBrightness?val=${value}`);
    }, 100));
  }
  
  // Set up the slider for spawn rate
  const spawnRateSlider = document.getElementById("sliderSpawn");
  if (spawnRateSlider) {
    spawnRateSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numSpawn", value);
      apiQueue.add(`/api/setSpawnRate?val=${value}`);
    }, 100));
  }
  
  // Set up the slider for max flakes
  const maxFlakesSlider = document.getElementById("sliderMaxFlakes");
  if (maxFlakesSlider) {
    maxFlakesSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numMaxFlakes", value);
      apiQueue.add(`/api/setMaxFlakes?val=${value}`);
    }, 100));
  }
  
  // Set up the slider for tail length
  const tailLengthSlider = document.getElementById("sliderTail");
  if (tailLengthSlider) {
    tailLengthSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numTail", value);
      apiQueue.add(`/api/setTailLength?val=${value}`);
    }, 100));
  }
  
  // Set up the slider for fade amount
  const fadeAmountSlider = document.getElementById("sliderFade");
  if (fadeAmountSlider) {
    fadeAmountSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numFade", value);
      apiQueue.add(`/api/setFadeAmount?val=${value}`);
    }, 100));
  }
  
  // Set up panel count slider - always default to 2
  const panelCountSlider = document.getElementById("sliderPanelCount");
  if (panelCountSlider) {
    panelCountSlider.value = 2;
    updateUIValue("numPanelCount", 2);
    
    panelCountSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numPanelCount", value);
      apiQueue.add(`/api/setPanelCount?val=${value}`);
    }, 100));
  }
}

// Helper function to update UI values
function updateUIValue(targetId, value) {
  const element = document.getElementById(targetId);
  if (element) {
    element.innerHTML = value;
  }
}

/************************************************
 * Load Animations
 ************************************************/
function loadAnimations(callback) {
  log("Loading animations...");
  
  fetch("/api/listAnimations")
    .then(r => r.json())
    .then(data => {
      log("Got animations: " + JSON.stringify(data));
      try {
        const sel = document.getElementById("selAnimation");
        if (sel) {
          // Clear dropdown first
          sel.innerHTML = "";
          
          // Check if data.animations exists
          if (!data.animations) {
            log("ERROR: No animations array in response");
            if (callback) callback();
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

          // Add change handler - IMPORTANT: use 'val' not 'index'
          sel.addEventListener("change", function() {
            const val = this.value;
            apiQueue.add(`/api/setAnimation?val=${val}`);
          });
        }
      } catch (err) {
        log("Error populating animations: " + err.message);
      }
      
      if (callback) callback();
    })
    .catch(err => {
      log("Error fetching animations: " + err);
      if (callback) callback();
    });
}

/************************************************
 * Load Palettes
 ************************************************/
function loadPalettes(callback) {
  log("Loading palettes...");
  
  fetch("/api/listPalettes")
    .then(r => r.json())
    .then(data => {
      if (data) {
        log("Got palettes: " + JSON.stringify(data));
        
        try {
          const sel = document.getElementById("paletteSelect");
          if (sel) {
            // Clear dropdown first
            sel.innerHTML = "";
            
            // Check if data.palettes exists
            if (!data.palettes) {
              log("ERROR: No palettes array in response");
              if (callback) callback();
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

            // Set current selection
            log("Setting selected palette to: " + data.current);
            sel.value = data.current;

            // Add change handler - IMPORTANT: use 'val' not 'index'
            sel.addEventListener("change", function() {
              const val = this.value;
              apiQueue.add(`/api/setPalette?val=${val}`);
            });
          }
        } catch (err) {
          log("Error populating palettes: " + err.message);
        }
      }
      
      if (callback) callback();
    })
    .catch(err => {
      log("Error fetching palettes: " + err);
      if (callback) callback();
    });
}

/************************************************
 * Load Settings
 ************************************************/
function loadSettings(callback) {
  log("Loading settings...");
  
  // Get all settings in separate requests to avoid overwhelming the ESP32
  Promise.all([
    fetch("/api/getBrightness").then(r => r.text()).catch(() => "32"),
    fetch("/api/getFadeAmount").then(r => r.text()).catch(() => "39"),
    fetch("/api/getTailLength").then(r => r.text()).catch(() => "3"),
    fetch("/api/getSpawnRate").then(r => r.text()).catch(() => "1.0"),
    fetch("/api/getMaxFlakes").then(r => r.text()).catch(() => "100"),
    fetch("/api/getSpeed").then(r => r.text()).catch(() => "30"),
    fetch("/api/getPanelCount").then(r => r.text()).catch(() => "2"),
    fetch("/api/getRotation?panel=PANEL1").then(r => r.text()).catch(() => "90"),
    fetch("/api/getRotation?panel=PANEL2").then(r => r.text()).catch(() => "90"),
    fetch("/api/getRotation?panel=PANEL3").then(r => r.text()).catch(() => "90"),
    fetch("/api/getPanelOrder").then(r => r.text()).catch(() => "normal")
  ])
  .then(results => {
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
    log(`Panel 1 Rotation: ${rotation1}°`);
    log(`Panel 2 Rotation: ${rotation2}°`);
    log(`Panel 3 Rotation: ${rotation3}°`);
    log(`Panel Order: ${panelOrder}`);

    // Set UI values - parse the values to handle JSON responses
    setSliderValue("sliderBrightness", brightness, "numBrightness");
    setSliderValue("sliderFade", fade, "numFade");
    setSliderValue("sliderTail", tail, "numTail");
    setSliderValue("sliderSpawn", spawn, "numSpawn");
    setSliderValue("sliderMaxFlakes", maxFlakes, "numMaxFlakes");
    updateSpeedUI(speed);
    
    // Handle panel count specially - always use 2 regardless of API response
    const pcSlider = document.getElementById("sliderPanelCount");
    if (pcSlider) {
      pcSlider.value = 2;
      updateUIValue("numPanelCount", 2);
    }
    
    // Set panel rotation dropdowns
    if (document.getElementById("rotatePanel1")) {
      document.getElementById("rotatePanel1").value = parseValue(rotation1);
    }
    if (document.getElementById("rotatePanel2")) {
      document.getElementById("rotatePanel2").value = parseValue(rotation2);
    }
    if (document.getElementById("rotatePanel3")) {
      document.getElementById("rotatePanel3").value = parseValue(rotation3);
    }
    if (document.getElementById("panelOrder")) {
      document.getElementById("panelOrder").value = parseValue(panelOrder).toLowerCase();
    }
    
    log("UI initialization complete.");
    
    if (callback) callback();
  })
  .catch(err => {
    log("Error loading settings: " + err);
    if (callback) callback();
  });
}

// Helper function to handle potential JSON responses
function parseValue(val) {
  if (!val) return "";
  
  try {
    // If it's a JSON object with a known property, extract it
    if (val.startsWith("{")) {
      const obj = JSON.parse(val);
      if (obj.panelCount !== undefined) return obj.panelCount;
      // Add other properties as needed
    }
    
    // Otherwise return as is
    return val.replace("°", ""); // Remove degrees symbol if present
  } catch (e) {
    return val.replace("°", ""); // Just return the raw value without degrees
  }
}

// Helper to set slider value and text display
function setSliderValue(sliderId, value, textId) {
  const slider = document.getElementById(sliderId);
  const parsedValue = parseValue(value);
  
  if (slider) {
    slider.value = parsedValue;
  }
  
  updateUIValue(textId, parsedValue);
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
        log(`Panel 1 rotation set to: ${angle}° - ${result}`);
      });
    });
  }
  
  // Panel 2 rotation
  const btnRotatePanel2 = document.getElementById("btnRotatePanel2");
  if (btnRotatePanel2) {
    btnRotatePanel2.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel2").value;
      apiQueue.add(`/api/rotatePanel2?val=${angle}`, result => {
        log(`Panel 2 rotation set to: ${angle}° - ${result}`);
      });
    });
  }
  
  // Panel 3 rotation
  const btnRotatePanel3 = document.getElementById("btnRotatePanel3");
  if (btnRotatePanel3) {
    btnRotatePanel3.addEventListener("click", () => {
      const angle = document.getElementById("rotatePanel3").value;
      apiQueue.add(`/api/rotatePanel3?val=${angle}`, result => {
        log(`Panel 3 rotation set to: ${angle}° - ${result}`);
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
  
  if (slider && number) {
    // Temporarily remove event listeners by cloning the elements
    const newSlider = slider.cloneNode(true);
    const newNumber = number.cloneNode(true);
    
    // Update values without triggering events
    newSlider.value = value;
    newNumber.value = value;
    
    // Replace the original elements with the new ones
    slider.parentNode.replaceChild(newSlider, slider);
    number.parentNode.replaceChild(newNumber, number);
  }
}

// Function to initialize the UI - separated to improve readability
function initializeUI() {
  // This function is now deprecated - we're using a staged initialization approach instead
  log("DEPRECATED - using staged initialization instead");
}
