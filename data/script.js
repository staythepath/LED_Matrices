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
  
  // Setup UI controls immediately, they'll work as soon as settings are loaded
  setupUIControls();
  
  // Load everything in a single fast sequence
  loadEverything();
});

// Show loading overlay to prevent premature interactions
/*
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
*/

// Hide loading overlay when ready
/*
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
*/

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
  
  // Set up the slider for speed
  const speedSlider = document.getElementById("sliderSpeed");
  if (speedSlider) {
    speedSlider.addEventListener("input", debounce(function() {
      const value = this.value;
      updateUIValue("numSpeed", value);
      
      // Debug the exact URL we're calling
      const speedUrl = `/api/setSpeed?val=${value}`;
      console.log(`DEBUG - Calling API: ${speedUrl}`);
      
      // Make the API call with proper error handling
      fetch(speedUrl)
        .then(response => {
          console.log(`DEBUG - Response status: ${response.status}`);
          const contentType = response.headers.get('content-type');
          console.log(`DEBUG - Content type: ${contentType}`);
          
          if (!response.ok) {
            // Log the error but don't throw - this allows the UI to continue working
            log(`Error setting speed: ${response.status} ${response.statusText}`);
            return response.text().then(text => {
              console.log(`DEBUG - Error response body: "${text}"`);
              log(`Server response: ${text}`);
            });
          }
          
          return response.text().then(text => {
            console.log(`DEBUG - Success response body: "${text}"`);
            log(`Successfully set speed to ${value}`);
          });
        })
        .catch(error => {
          console.log(`DEBUG - Network error: ${error.message}`);
          log(`Network error setting speed: ${error.message}`);
        });
        
      log(`Setting animation speed to ${value}`);
    }, 100));
  }
  
  // Set up the slider for spawn rate
  const spawnRateSlider = document.getElementById("sliderSpawn");
  if (spawnRateSlider) {
    spawnRateSlider.addEventListener("input", debounce(function() {
      const sliderValue = this.value;
      updateUIValue("numSpawn", sliderValue);
      
      // Convert slider value (1-100) to API expected range (0.0-1.0)
      const apiValue = (sliderValue / 100).toFixed(2);
      
      // Debug the exact URL we're calling
      const spawnRateUrl = `/api/setSpawnRate?val=${apiValue}`;
      console.log(`DEBUG - Calling API: ${spawnRateUrl}`);
      
      // Make the API call with proper error handling
      fetch(spawnRateUrl)
        .then(response => {
          console.log(`DEBUG - Response status: ${response.status}`);
          const contentType = response.headers.get('content-type');
          console.log(`DEBUG - Content type: ${contentType}`);
          
          if (!response.ok) {
            // Log the error but don't throw - this allows the UI to continue working
            log(`Error setting spawn rate: ${response.status} ${response.statusText}`);
            return response.text().then(text => {
              console.log(`DEBUG - Error response body: "${text}"`);
              log(`Server response: ${text}`);
            });
          }
          
          return response.text().then(text => {
            console.log(`DEBUG - Success response body: "${text}"`);
            log(`Successfully set spawn rate to ${apiValue}`);
          });
        })
        .catch(error => {
          console.log(`DEBUG - Network error: ${error.message}`);
          log(`Network error setting spawn rate: ${error.message}`);
        });
        
      log(`Converting spawn rate slider value ${sliderValue} to API value ${apiValue}`);
    }, 100));
  }
  
  // Set up the slider for max flakes
  const maxFlakesSlider = document.getElementById("sliderMaxFlakes");
  if (maxFlakesSlider) {
    maxFlakesSlider.addEventListener("input", debounce(function() {
      const sliderValue = this.value;
      updateUIValue("numMaxFlakes", sliderValue);
      
      // Convert slider value (1-100) to API expected range (1-500)
      // Scale from 1-100 to 1-500 (linear scaling)
      const apiValue = Math.floor(1 + (sliderValue - 1) * 5);
      
      // Debug the exact URL we're calling
      const maxFlakesUrl = `/api/setMaxFlakes?val=${apiValue}`;
      console.log(`DEBUG - Calling API: ${maxFlakesUrl}`);
      
      // Make the API call with proper error handling
      fetch(maxFlakesUrl)
        .then(response => {
          console.log(`DEBUG - Response status: ${response.status}`);
          const contentType = response.headers.get('content-type');
          console.log(`DEBUG - Content type: ${contentType}`);
          
          if (!response.ok) {
            // Log the error but don't throw - this allows the UI to continue working
            log(`Error setting max flakes: ${response.status} ${response.statusText}`);
            return response.text().then(text => {
              console.log(`DEBUG - Error response body: "${text}"`);
              log(`Server response: ${text}`);
            });
          }
          
          return response.text().then(text => {
            console.log(`DEBUG - Success response body: "${text}"`);
            log(`Successfully set max flakes to ${apiValue}`);
          });
        })
        .catch(error => {
          console.log(`DEBUG - Network error: ${error.message}`);
          log(`Network error setting max flakes: ${error.message}`);
        });
        
      log(`Converting max flakes slider value ${sliderValue} to API value ${apiValue}`);
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
    // For input elements, set the value property
    if (element.tagName === 'INPUT') {
      element.value = value;
    } else {
      // For other elements (like span, div), use innerHTML
      element.innerHTML = value;
    }
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
            const val = parseInt(this.value, 10);
            
            // Send the animation change request to the API
            apiQueue.add(`/api/setAnimation?val=${val}`);
            log(`Animation changed to ${val}`);
            
            // Update UI controls based on the new animation
            // Use our dedicated function for more reliable detection
            setTimeout(() => {
              // Add a small delay to ensure the dropdown has updated
              const isGoL = updateGameOfLifeControls();
              log(`Animation change handler: Updated controls, isGameOfLife=${isGoL}`);
            }, 100);
          });
          
          // Initialize controls for the current animation
          const currentAnimIndex = parseInt(data.current, 10);
          log(`Initial animation index: ${currentAnimIndex}`);
          
          // Use our dedicated function for reliable Game of Life detection
          // This will also update the UI controls accordingly
          updateGameOfLifeControls();
          
          // Also fetch the initial column skip value if Game of Life is the current animation
          if (isGameOfLifeAnimation()) {
            log("Initial animation is Game of Life, fetching current column skip value");
            
            fetch("/api/getColumnSkip")
              .then(response => response.json())
              .then(data => {
                // Update the column skip slider with the current value
                const columnSkipSlider = document.getElementById("sliderColumnSkip");
                const columnSkipNumber = document.getElementById("numColumnSkip");
                
                if (columnSkipSlider && columnSkipNumber && data.value) {
                  const skipValue = parseInt(data.value, 10);
                  columnSkipSlider.value = skipValue;
                  columnSkipNumber.value = skipValue;
                  log(`Updated column skip controls with initial value: ${skipValue}`);
                }
              })
              .catch(error => {
                log(`Error fetching initial column skip value: ${error.message}`);
              });
          }
          
          // Perform a second check after a delay to ensure reliable UI updates
          setTimeout(() => {
            // Double-check that Game of Life controls are properly set up
            const isGoL = updateGameOfLifeControls();
            log(`Delayed initialization check: isGameOfLife=${isGoL}`);
          }, 500);
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
  
  // Immediately resolve with default values
  Promise.resolve()
    .then(() => {
      const results = [
        "32",   // brightness
        "39",   // fade
        "3",    // tail
        "1.0",  // spawn
        "100",  // maxFlakes
        "30",   // speed
        "2",    // panelCount
        "90",   // rotation1
        "90",   // rotation2
        "90",   // rotation3
        "normal"// panelOrder
      ];

      // Destructure results (same as before)
      const [
        brightness, fade, tail, spawn, maxFlakes, speed, panelCount,
        rotation1, rotation2, rotation3, panelOrder
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

      // Set UI values (same as before)
      setSliderValue("sliderBrightness", brightness, "numBrightness");
      setSliderValue("sliderFade", fade, "numFade");
      setSliderValue("sliderTail", tail, "numTail");
      // Convert spawn rate from API format (0.0-1.0) to slider format (1-100)
      const spawnSliderValue = spawnRateToSlider(parseFloat(spawn));
      setSliderValue("sliderSpawn", spawnSliderValue, "numSpawn");
      
      // Convert max flakes from API format (10-500) to slider format (1-100)
      const maxFlakesSliderValue = maxFlakesToSlider(parseInt(maxFlakes));
      setSliderValue("sliderMaxFlakes", maxFlakesSliderValue, "numMaxFlakes");
      updateSpeedUI(speed);

      // Force panel count to 2
      const pcSlider = document.getElementById("sliderPanelCount");
      if (pcSlider) {
        pcSlider.value = 2;
        updateUIValue("numPanelCount", 2);
      }

      // Set rotations and panel order
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
 * Simplified loading process - load everything at once
 ************************************************/
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
        // hideLoadingOverlay();
      });
    });
  });
}

/************************************************
 * Speed Slider
 ************************************************/
function sliderToSpeed(slVal) {
  // More aggressive mapping for higher speeds
  // 0-50 on slider maps to 3-50ms (linear)
  // 50-100 on slider maps to 50-500ms (quadratic)
  
  const sliderValue = parseFloat(slVal);
  
  if (sliderValue <= 50) {
    // Linear mapping for 0-50 slider values to 3-50ms
    return Math.round(3 + (sliderValue / 50) * 47);
  } else {
    // More aggressive quadratic mapping for 50-100 slider values to 50-500ms
    const normalizedValue = (sliderValue - 50) / 50; // 0 to 1
    const quadraticValue = normalizedValue * normalizedValue; // Apply quadratic curve
    return Math.round(50 + quadraticValue * 450);
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

// Helper functions for spawn rate and max flakes conversion
function spawnRateToSlider(apiValue) {
  // Convert API value (0.0-1.0) to slider value (1-100)
  return Math.round(apiValue * 100);
}

function maxFlakesToSlider(apiValue) {
  // Convert API value (1-500) to slider value (1-100)
  return Math.round(((apiValue - 1) / 5) + 1);
}

// Define animation constants to match the actual animation indexes
const ANIMATIONS = {
  TRAFFIC: 0,
  BLINK: 1,
  RAINBOW_WAVE: 2,
  FIREWORK: 3,
  GAME_OF_LIFE: 4 // GameOfLife is index 4 according to the log
};

// Function to check if current animation is Game of Life
// This is a separate, dedicated function to ensure reliable detection
function isGameOfLifeAnimation() {
  try {
    // Get current animation from dropdown
    const animSelect = document.getElementById("selAnimation");
    if (!animSelect) return false;
    
    // Check by index (most reliable method)
    const selectedIndex = parseInt(animSelect.value, 10);
    
    // Basic check - animation index 4 is Game of Life
    return (selectedIndex === 4);
  } catch (err) {
    // If any error occurs, log it and return false
    log("Error in isGameOfLifeAnimation: " + err.message);
    return false;
  }
}

// Function to update UI visibility based on current animation
function updateGameOfLifeControls() {
  try {
    // Check if current animation is Game of Life
    const isGoL = isGameOfLifeAnimation();
    
    // Get DOM elements
    const columnSkipContainer = document.getElementById("columnSkipContainer");
    const speedLabel = document.getElementById("speedLabel");
    
    // Update column skip slider visibility
    if (columnSkipContainer) {
      columnSkipContainer.style.display = isGoL ? "flex" : "none";
      log(`Column skip container display set to: ${isGoL ? "flex" : "none"}`);
    }
    
    // Set speed label consistently to "Speed:"
    if (speedLabel) {
      speedLabel.textContent = "Speed:";
    }
    
    return isGoL;
  } catch (err) {
    // Handle any errors gracefully
    log("Error in updateGameOfLifeControls: " + err.message);
    return false;
  }
}

// Helper function to visually indicate which controls directly affect the current animation
function updateControlsForAnimation(animationIndex) {
  // Convert to number if it's a string
  const index = parseInt(animationIndex, 10);
  
  // Get control elements containers
  const spawnRateContainer = document.getElementById("sliderSpawn")?.closest(".slider-container");
  const maxFlakesContainer = document.getElementById("sliderMaxFlakes")?.closest(".slider-container");
  
  // Debug which animation index is selected
  log(`Animation index selected: ${index}`);
  
  // Update Game of Life specific controls
  const isGameOfLife = updateGameOfLifeControls();
  
  // Check if Traffic animation is selected
  const isTraffic = (index === ANIMATIONS.TRAFFIC);
  
  // Update visual indicators but don't disable the controls
  if (spawnRateContainer) {
    spawnRateContainer.style.opacity = isTraffic ? "1" : "0.7";
    spawnRateContainer.title = isTraffic ? "Controls car spawn rate" : 
        "Values are saved but only visibly affect the Traffic animation";
  }
  
  if (maxFlakesContainer) {
    maxFlakesContainer.style.opacity = isTraffic ? "1" : "0.7";
    maxFlakesContainer.title = isTraffic ? "Controls maximum number of cars" : 
        "Values are saved but only visibly affect the Traffic animation";
  }
  
  log(`Controls updated for animation ${index}: ${isTraffic ? "Highlighted" : "Dimmed"} spawn rate and max flakes controls`);
  if (isGameOfLife) {
    log("Game of Life animation selected: showing column skip slider");
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

// brightness, fade, tail - we skip spawn & maxFlakes as they have custom handlers already
bindSliderAndNumber(
  "sliderBrightness",
  "numBrightness",
  "setBrightness",
  0,
  255
);
bindSliderAndNumber("sliderFade", "numFade", "setFadeAmount", 0, 255);
bindSliderAndNumber("sliderTail", "numTail", "setTailLength", 1, 32);

// Column skip slider for Game of Life animation
bindSliderAndNumber(
  "sliderColumnSkip",
  "numColumnSkip",
  "setColumnSkip",
  1,
  5
);

// Explicitly check for the column skip container
const columnSkipContainer = document.getElementById("columnSkipContainer");
if (columnSkipContainer) {
  log("Found column skip container in DOM");
} else {
  log("ERROR: Column skip container NOT found in DOM");
}

// IMPORTANT: Don't call bindSliderAndNumber for spawn rate and max flakes sliders
// These have custom handlers in setupUIControls that do proper value conversion
// bindSliderAndNumber("sliderSpawn", "numSpawn", "setSpawnRate", 0, 100);
// bindSliderAndNumber("sliderMaxFlakes", "numMaxFlakes", "setMaxFlakes", 1, 100);

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
