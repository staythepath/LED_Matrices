const GAME_OF_LIFE_INDEX = 4;
const STRANGE_LOOP_INDEX = 5;
const TEXT_TICKER_INDEX = 6;
const elementaryRulePresets = [30, 45, 73, 90, 102, 110, 129, 165, 225];
const controlBindings = {};
let currentAnimationIndex = 0;
let currentAutomataMode = 0;
let cryptoTickerInterval = null;
let cryptoTickerLastUpdate = 0;
const CRYPTO_REFRESH_MS = 60_000;
const defaultLabelText = {
  speed: "Speed:",
  brightness: "Brightness:",
  fade: "Fade Amount:",
  tail: "Tail Length:",
  spawn: "Spawn Rate:",
  maxFlakes: "Max Flakes:",
};

const defaultHelpText = {
  speed: "Controls how quickly each animation updates. Higher values = faster motion.",
  fade: "Controls how aggressively trails fade in non-GoL animations.",
  tail: "Adjusts persistence of motion trails for supported animations.",
};

const animationPreviewModes = {
  0: "traffic",
  1: "blink",
  2: "rainbow",
  3: "firework",
  [GAME_OF_LIFE_INDEX]: "gol",
  [STRANGE_LOOP_INDEX]: "automata",
  [TEXT_TICKER_INDEX]: "ticker",
};

const MAX_CUSTOM_PALETTE_COLORS = 16;
let customPaletteIndex = -1;
let customPaletteColors = ["#FF0000", "#00FF00", "#0000FF", "#FFFFFF"];

const golModeDescriptions = {
  0: "Sweep: Cursor scans the grid, updating one column at a time with the sweep highlight.",
  1: "Simultaneous: Updates the entire board together each tick with no sweep cursor.",
  2: "Fade Only: Steps generations together while easing brightness in and out.",
};

const automataDescriptors = {
  0: {
    name: "Langton's Ant Colony",
    primary: {
      label: "Ant Count",
      help: "Adds more self-referential walkers exploring the plane.",
      formatter: (value) => `${mapAntCount(value)} ants`,
    },
    secondary: {
      label: "Steps Per Update",
      help: "How many ant moves occur per frame. Higher values accelerate emergent highways.",
      formatter: (value) => `${mapLangtonSteps(value)} steps`,
    },
  },
  1: {
    name: "Elementary Rule Tapestry",
    primary: {
      label: "Rule Preset",
      help: "Selects one of the curated Wolfram elementary cellular automata rules.",
      formatter: (value) => {
        const idx = mapRuleIndex(value);
        return `Rule ${elementaryRulePresets[idx]}`;
      },
    },
    secondary: {
      label: "Rows Per Update",
      help: "Generations drawn each frame; higher values scroll faster and revisit history sooner.",
      formatter: (value) => `${mapRuleIterations(value)} rows`,
    },
  },
  2: {
    name: "Recursive Moire",
    primary: {
      label: "Density",
      help: "Controls the interference frequency and depth of the recursive layering.",
      formatter: (value) => `density ×${mapMoireDensity(value)}`,
    },
    secondary: {
      label: "Tempo",
      help: "Speed of the phase warp animating the recursive interference loops.",
      formatter: (value) => `tempo ×${mapMoireTempo(value)}`,
    },
  },
};

function mapAntCount(value) {
  return 1 + Math.floor((value * 7) / 100);
}

function mapLangtonSteps(value) {
  return 1 + Math.floor((value * 63) / 100);
}

function mapRuleIndex(value) {
  const maxIndex = elementaryRulePresets.length - 1;
  return Math.min(maxIndex, Math.round((value * maxIndex) / 100));
}

function mapRuleIterations(value) {
  return 1 + Math.floor((value * 19) / 100);
}

function mapMoireDensity(value) {
  return (0.8 + (value / 100) * 7.0).toFixed(2);
}

function mapMoireTempo(value) {
  return (0.35 + (value / 100) * 2.3).toFixed(2);
}

const previewState = {
  canvas: null,
  ctx: null,
  cols: 32,
  rows: 16,
  phase: 0,
  speed: 0.02,
  highlightWidth: 2,
  columnSkip: 6,
  subMode: "0",
  mode: "generic",
  automataPrimary: 50,
  automataSecondary: 55,
  fade: 0,
  tail: 0,
  spawn: 0,
  brightness: 30,
  fireworkParticles: [],
  fireworkCooldown: 0,
  trafficCars: [],
  blinkTimer: 0,
  lastTime: 0,
};

function initializePanelPreview() {
  const canvas = $("panelPreview");
  if (!canvas || !canvas.getContext) {
    previewState.canvas = null;
    previewState.ctx = null;
    togglePreviewFallback(true);
    return;
  }
  previewState.canvas = canvas;
  previewState.ctx = canvas.getContext("2d");
  togglePreviewFallback(false);
  resetPreviewStateForMode(previewState.mode);
  previewState.lastTime = performance.now();
  updatePreviewParameters();
  requestAnimationFrame(stepPanelPreview);
}

function togglePreviewFallback(show) {
  const canvas = $("panelPreview");
  const fallback = $("previewFallback");
  if (canvas) {
    canvas.hidden = !!show;
  }
  if (fallback) {
    fallback.hidden = !show;
  }
}

function seedTrafficCars() {
  const lanes = [2, 5, 10, 13];
  previewState.trafficCars = lanes.map((row, index) => ({
    x: Math.random() * previewState.cols,
    row,
    dir: index % 2 === 0 ? 1 : -1,
    hue: 10 + (index * 80),
    length: 1 + Math.random() * 1.5,
  }));
}

function resetPreviewStateForMode(mode) {
  previewState.phase = 0;
  previewState.fireworkParticles = [];
  previewState.fireworkCooldown = 0;
  previewState.blinkTimer = 0;
  if (mode === "traffic") {
    seedTrafficCars();
  } else {
    previewState.trafficCars = [];
  }
  if (mode === "firework") {
    spawnFireworkBurst();
    spawnFireworkBurst();
  }
}

function setPreviewMode(mode) {
  if (previewState.mode === mode) {
    return;
  }
  previewState.mode = mode;
  resetPreviewStateForMode(mode);
}

function getPreviewModeForAnimation(index) {
  return animationPreviewModes[index] || "generic";
}

function setPreviewModeFromAnimation(index) {
  setPreviewMode(getPreviewModeForAnimation(index));
  updatePreviewParameters();
}

function updatePreviewParameters() {
  const speedSlider = $("sliderSpeed");
  const highlightWidthSlider = $("sliderHighlightWidth");
  const goLMode = $("selectGoLMode");
  const automataPrimary = $("sliderAutomataPrimary");
  const automataSecondary = $("sliderAutomataSecondary");
  const columnSkipSlider = $("sliderColumnSkip");
  const fadeSlider = $("sliderFade");
  const tailSlider = $("sliderTail");
  const spawnSlider = $("sliderSpawn");
  const brightnessSlider = $("sliderBrightness");

  const speedValue = speedSlider ? parseFloat(speedSlider.value) || 0 : 0;
  previewState.speed = 0.02 + (speedValue / 100) * 0.2;

  const widthValue = highlightWidthSlider ? parseInt(highlightWidthSlider.value, 10) || 0 : 0;
  previewState.highlightWidth = Math.max(0, widthValue);

  const skipValue = columnSkipSlider ? parseInt(columnSkipSlider.value, 10) || 1 : 1;
  previewState.columnSkip = Math.max(1, skipValue);

  previewState.fade = fadeSlider ? parseInt(fadeSlider.value, 10) || 0 : 0;
  previewState.tail = tailSlider ? parseInt(tailSlider.value, 10) || 0 : 0;
  previewState.spawn = spawnSlider ? parseInt(spawnSlider.value, 10) || 0 : 0;
  previewState.brightness = brightnessSlider ? parseInt(brightnessSlider.value, 10) || 0 : 0;

  previewState.subMode = goLMode ? goLMode.value || "0" : "0";

  previewState.automataPrimary = automataPrimary ? parseInt(automataPrimary.value, 10) || 0 : 0;
  previewState.automataSecondary = automataSecondary ? parseInt(automataSecondary.value, 10) || 0 : 0;
}

function stepPanelPreview(timestamp) {
  if (!previewState.ctx || !previewState.canvas) {
    return;
  }
  const ctx = previewState.ctx;
  const canvas = previewState.canvas;
  const dt = timestamp - (previewState.lastTime || timestamp);
  previewState.lastTime = timestamp;
  const dtSeconds = dt / 1000;
  const sweepRange = previewState.cols * 4;
  previewState.phase = (previewState.phase + previewState.speed * dt) % sweepRange;

  ctx.fillStyle = "rgba(13, 16, 27, 0.94)";
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  const cellWidth = canvas.width / previewState.cols;
  const cellHeight = canvas.height / previewState.rows;

  const drawGolSweep = () => {
    const baseHue = 210;
    const highlightWidth = Math.max(1, previewState.highlightWidth || 1);
    const position = previewState.phase % previewState.cols;
    const mode = previewState.subMode || "0";
    const skip = Math.max(1, previewState.columnSkip);
    const fadeLevel = previewState.fade / 255;
    for (let y = 0; y < previewState.rows; y++) {
      for (let x = 0; x < previewState.cols; x++) {
        const base = 0.28 + 0.3 * Math.sin(x * 0.22 + y * 0.18 + previewState.phase * 0.08);
        let brightness = base + fadeLevel * 0.35;
        let hue = baseHue + Math.sin((x + previewState.phase * 0.12) * 0.4) * 45;
        if (mode === "0") {
          const distance = Math.abs(((x - position + previewState.cols) % previewState.cols));
          if (distance <= highlightWidth) {
            brightness = 0.72 + fadeLevel * 0.22;
            hue = 320;
          } else if (x % skip === 0) {
            brightness *= 0.8;
            hue = baseHue - 20;
          }
        } else if (mode === "1") {
          const pulse = 0.5 + 0.5 * Math.sin(previewState.phase * 0.14 + x * 0.35);
          brightness = 0.3 + pulse * (0.4 + fadeLevel * 0.25);
          hue = baseHue + pulse * 90;
        } else {
          const glow = Math.sin(previewState.phase * 0.08 + x * 0.16 + y * 0.12);
          brightness = 0.28 + Math.abs(glow) * (0.45 + fadeLevel * 0.3);
          hue = baseHue + glow * 110;
        }
        const lightness = clamp(brightness, 0, 1) * 60 + 12;
        ctx.fillStyle = `hsl(${hue}, 70%, ${lightness}%)`;
        ctx.fillRect(x * cellWidth, y * cellHeight, Math.ceil(cellWidth) + 1, Math.ceil(cellHeight) + 1);
      }
    }
  };

  const drawAutomataPreview = () => {
    const mode = currentAutomataMode;
    const primary = previewState.automataPrimary / 100;
    const secondary = previewState.automataSecondary / 100;
    for (let y = 0; y < previewState.rows; y++) {
      for (let x = 0; x < previewState.cols; x++) {
        let hue;
        let light;
        if (mode === 0) {
          const spiral = Math.sin((x * 0.32) + previewState.phase * (0.015 + primary * 0.08)) +
                        Math.cos((y * 0.35) - previewState.phase * (0.02 + secondary * 0.07));
          const checker = ((x + y + Math.floor(previewState.phase / 6)) % 2 === 0) ? 1 : -1;
          hue = 20 + primary * 180 + checker * 18;
          light = 0.32 + spiral * 0.22 + checker * 0.15;
        } else if (mode === 1) {
          const generation = (Math.floor(previewState.phase / 2) + y) % previewState.rows;
          const ruleIndex = mapRuleIndex(previewState.automataPrimary);
          const bit = (elementaryRulePresets[ruleIndex] >> ((x + generation) % 8)) & 1;
          hue = bit ? 200 + secondary * 120 : 220 - primary * 90;
          light = bit ? 0.55 + secondary * 0.25 : 0.18 + primary * 0.3;
        } else {
          const radius = Math.hypot(x - previewState.cols / 2, y - previewState.rows / 2);
          const wave = Math.sin(radius * (0.25 + primary * 0.5) - previewState.phase * (0.04 + secondary * 0.05));
          hue = 250 + wave * 120;
          light = 0.34 + (0.25 + primary * 0.2) * wave;
        }
        const clampedLight = clamp(light, 0, 1) * 55 + 18;
        ctx.fillStyle = `hsl(${(hue + 360) % 360}, 68%, ${clampedLight}%)`;
        ctx.fillRect(x * cellWidth, y * cellHeight, Math.ceil(cellWidth) + 1, Math.ceil(cellHeight) + 1);
      }
    }
  };

  const drawTrafficPreview = () => {
    if (!previewState.trafficCars.length) {
      seedTrafficCars();
    }
    const lanes = new Set(previewState.trafficCars.map((car) => car.row));
    const dashOffset = (previewState.phase * cellWidth * 0.25) % (cellWidth * 4);
    lanes.forEach((row) => {
      const centerY = row * cellHeight;
      ctx.fillStyle = "rgba(34, 38, 54, 0.85)";
      ctx.fillRect(0, centerY - cellHeight * 1.2, canvas.width, cellHeight * 2.4);
      ctx.fillStyle = "rgba(189, 147, 249, 0.28)";
      for (let x = -dashOffset; x < canvas.width; x += cellWidth * 4) {
        ctx.fillRect(x, centerY - cellHeight * 0.12, cellWidth * 1.2, cellHeight * 0.24);
      }
    });

    const tailFactor = clamp(previewState.tail / 15, 0, 1);
    const speedScale = (2.4 + previewState.speed * 6.5 + previewState.spawn / 80) * dtSeconds;
    previewState.trafficCars.forEach((car) => {
      car.x += car.dir * speedScale;
      const carWidthCells = Math.max(1.2, car.length);
      if (car.dir > 0 && car.x > previewState.cols + carWidthCells) {
        car.x = -carWidthCells;
      } else if (car.dir < 0 && car.x < -carWidthCells) {
        car.x = previewState.cols + carWidthCells;
      }
      const carX = car.x * cellWidth;
      const carY = car.row * cellHeight;
      ctx.fillStyle = `hsla(${(car.hue + previewState.phase * 0.2) % 360}, 85%, 58%, 0.9)`;
      ctx.fillRect(carX, carY - cellHeight * 0.75, cellWidth * carWidthCells, cellHeight * 1.15);

      const trailLength = cellWidth * 1.4 * (1 + tailFactor);
      ctx.fillStyle = `hsla(${car.hue}, 85%, 65%, ${0.35 + tailFactor * 0.25})`;
      ctx.fillRect(carX - car.dir * trailLength, carY - cellHeight * 0.55, trailLength, cellHeight * 0.75);

      ctx.fillStyle = "rgba(255, 255, 255, 0.25)";
      const headlight = car.dir > 0 ? carX + cellWidth * carWidthCells : carX - cellWidth * 0.35;
      ctx.fillRect(headlight, carY - cellHeight * 0.45, cellWidth * 0.35, cellHeight * 0.4);
    });
  };

  const drawBlinkPreview = () => {
    const brightnessFactor = clamp(previewState.brightness / 255, 0, 1);
    previewState.blinkTimer += dtSeconds * (1.6 + previewState.speed * 4);
    const nodes = [
      { x: 6, y: 4, hue: 30 },
      { x: 16, y: 7, hue: 120 },
      { x: 25, y: 5, hue: 210 },
      { x: 11, y: 11, hue: 0 },
    ];
    nodes.forEach((node, idx) => {
      const pulse = (Math.sin(previewState.blinkTimer + idx * Math.PI / 2) + 1) / 2;
      const size = 2.6 + pulse * 2.4;
      const offsetX = (node.x - size / 2) * cellWidth;
      const offsetY = (node.y - size / 2) * cellHeight;
      const width = size * cellWidth;
      const height = size * cellHeight;
      const light = Math.min(90, 35 + pulse * 45 + brightnessFactor * 25);
      ctx.fillStyle = `hsla(${node.hue}, 80%, ${light}%, 0.85)`;
      ctx.fillRect(offsetX, offsetY, width, height);
      ctx.fillStyle = `hsla(${node.hue}, 90%, 95%, ${0.18 + pulse * 0.18})`;
      ctx.fillRect(offsetX + width * 0.2, offsetY + height * 0.2, width * 0.6, height * 0.6);
    });
  };

  const drawRainbowPreview = () => {
    const brightnessScale = clamp(previewState.brightness / 255, 0, 1);
    for (let y = 0; y < previewState.rows; y++) {
      for (let x = 0; x < previewState.cols; x++) {
        const wave = Math.sin((x * 0.25) + previewState.phase * 0.08) +
                     Math.cos((y * 0.18) - previewState.phase * 0.05);
        const hue = (previewState.phase * 8 + x * 12 + y * 4) % 360;
        const light = clamp(0.32 + wave * 0.18 + brightnessScale * 0.25, 0, 1);
        ctx.fillStyle = `hsl(${hue}, 85%, ${light * 60 + 15}%)`;
        ctx.fillRect(x * cellWidth, y * cellHeight, Math.ceil(cellWidth) + 1, Math.ceil(cellHeight) + 1);
      }
    }
  };

  const drawFireworkPreview = () => {
    previewState.fireworkCooldown -= dtSeconds;
    const cooldownMin = Math.max(0.35, 1.2 - previewState.spawn / 80);
    if (previewState.fireworkCooldown <= 0) {
      spawnFireworkBurst();
      previewState.fireworkCooldown = cooldownMin;
    }

    const survivors = [];
    const gravity = 0.9;
    const velocityScale = 3 + previewState.speed * 12;
    const decay = 0.45 + previewState.tail / 30;
    previewState.fireworkParticles.forEach((particle) => {
      const updated = particle;
      updated.vy += gravity * dtSeconds;
      updated.x += updated.vx * velocityScale * dtSeconds;
      updated.y += updated.vy * velocityScale * dtSeconds;
      updated.life -= dtSeconds * decay;
      if (updated.life > 0 && updated.y < previewState.rows + 2 && updated.y > -2 && updated.x > -2 && updated.x < previewState.cols + 2) {
        const alpha = Math.min(1, updated.life);
        ctx.fillStyle = `hsla(${updated.hue}, 85%, 60%, ${alpha})`;
        ctx.fillRect(updated.x * cellWidth, updated.y * cellHeight, Math.ceil(cellWidth) + 1, Math.ceil(cellHeight) + 1);
        survivors.push(updated);
      }
    });
    previewState.fireworkParticles = survivors;
  };

  const drawGenericPreview = () => {
    const lightBoost = clamp(previewState.brightness / 255, 0, 1);
    for (let y = 0; y < previewState.rows; y++) {
      for (let x = 0; x < previewState.cols; x++) {
        const swirl = Math.sin((x * 0.18) + previewState.phase * 0.07) +
                      Math.cos((y * 0.22) - previewState.phase * 0.05);
        const hue = 200 + swirl * 50;
        const brightness = clamp(0.28 + swirl * 0.2 + lightBoost * 0.3, 0, 1);
        ctx.fillStyle = `hsl(${hue}, 60%, ${brightness * 55 + 12}%)`;
        ctx.fillRect(x * cellWidth, y * cellHeight, Math.ceil(cellWidth) + 1, Math.ceil(cellHeight) + 1);
      }
    }
  };

  switch (previewState.mode) {
    case "gol":
      drawGolSweep();
      break;
    case "automata":
      drawAutomataPreview();
      break;
    case "traffic":
      drawTrafficPreview();
      break;
    case "blink":
      drawBlinkPreview();
      break;
    case "rainbow":
      drawRainbowPreview();
      break;
    case "firework":
      drawFireworkPreview();
      break;
    default:
      drawGenericPreview();
      break;
  }

  requestAnimationFrame(stepPanelPreview);
}

function spawnFireworkBurst() {
  const originX = 4 + Math.random() * (previewState.cols - 8);
  const originY = 2 + Math.random() * (previewState.rows * 0.6);
  const sparks = 28 + Math.floor(Math.random() * 28);
  const baseHue = Math.random() * 360;
  for (let i = 0; i < sparks; i++) {
    const angle = (Math.PI * 2 * i) / sparks;
    const speed = 0.9 + Math.random() * 1.4;
    previewState.fireworkParticles.push({
      x: originX,
      y: originY,
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed,
      life: 1.2 + Math.random() * 0.8,
      hue: (baseHue + Math.random() * 40) % 360,
    });
  }
  if (previewState.fireworkParticles.length > 400) {
    previewState.fireworkParticles.splice(0, previewState.fireworkParticles.length - 400);
  }
}
/************************************************
 * Logging
 ************************************************/
function log(message) {
  console.log(message);
  const logDiv = document.getElementById("log");
  if (!logDiv) return;
  const line = document.createElement("div");
  line.textContent = `${new Date().toLocaleTimeString()} ${message}`;
  logDiv.appendChild(line);
  logDiv.scrollTop = logDiv.scrollHeight;
}

function setControlStatus(id, message, tone = "info") {
  const element = $(id);
  if (!element) {
    return;
  }
  if (!message) {
    element.textContent = "";
    delete element.dataset.tone;
    element.hidden = true;
    return;
  }
  element.textContent = message;
  element.dataset.tone = tone;
  element.hidden = false;
}

function normalizeHexColor(value) {
  if (typeof value !== "string") {
    return null;
  }
  const trimmed = value.trim();
  if (!trimmed.length) {
    return null;
  }
  const sanitized = trimmed.startsWith("#") ? trimmed.slice(1) : trimmed;
  if (!/^[0-9a-fA-F]{6}$/.test(sanitized)) {
    return null;
  }
  return `#${sanitized.toUpperCase()}`;
}

function normalizePaletteName(value) {
  if (typeof value !== "string") {
    return "";
  }
  let trimmed = value.trim();
  if (!trimmed.length) {
    return "";
  }
  trimmed = trimmed.replace(/[\r\n]+/g, " ").replace(/\s+/g, " ");
  trimmed = trimmed.replace(/[|;]/g, "-");
  if (trimmed.length > 32) {
    trimmed = trimmed.slice(0, 32).trim();
  }
  return trimmed;
}

function toggleCustomPaletteEditor(show) {
  const container = $("customPaletteContainer");
  if (container) {
    container.style.display = show ? "flex" : "none";
  }
}

function renderCustomPaletteList() {
  const list = $("customPaletteList");
  if (!list) {
    return;
  }

  if (!Array.isArray(customPaletteColors) || customPaletteColors.length === 0) {
    customPaletteColors = ["#FF0000", "#00FF00", "#0000FF", "#FFFFFF"];
  }

  customPaletteColors = customPaletteColors
    .map((color) => normalizeHexColor(color) || "#FFFFFF")
    .slice(0, MAX_CUSTOM_PALETTE_COLORS);

  list.innerHTML = "";

  customPaletteColors.forEach((color, index) => {
    const row = document.createElement("div");
    row.className = "custom-color-entry";

    const colorInput = document.createElement("input");
    colorInput.type = "color";
    colorInput.value = color;
    colorInput.setAttribute("aria-label", `Select colour ${index + 1}`);

    const hexInput = document.createElement("input");
    hexInput.type = "text";
    hexInput.value = color;
    hexInput.maxLength = 7;
    hexInput.placeholder = "#RRGGBB";
    hexInput.setAttribute("aria-label", `Hex value for colour ${index + 1}`);

    const removeButton = document.createElement("button");
    removeButton.type = "button";
    removeButton.textContent = "×";
    removeButton.title = "Remove colour";
    removeButton.classList.add("danger");

    colorInput.addEventListener("input", (event) => {
      const normalized = normalizeHexColor(event.target.value) || "#FFFFFF";
      customPaletteColors[index] = normalized;
      hexInput.value = normalized;
      setControlStatus("customPaletteStatus", "");
    });

    hexInput.addEventListener("change", () => {
      const normalized = normalizeHexColor(hexInput.value);
      if (normalized) {
        customPaletteColors[index] = normalized;
        hexInput.value = normalized;
        colorInput.value = normalized;
        setControlStatus("customPaletteStatus", "");
      } else {
        hexInput.value = customPaletteColors[index];
        setControlStatus("customPaletteStatus", "Use hex colours like #FF6600.", "error");
      }
    });

    removeButton.addEventListener("click", () => {
      if (customPaletteColors.length <= 1) {
        return;
      }
      customPaletteColors.splice(index, 1);
      renderCustomPaletteList();
    });

    if (customPaletteColors.length <= 1) {
      removeButton.disabled = true;
    }

    row.appendChild(colorInput);
    row.appendChild(hexInput);
    row.appendChild(removeButton);
    list.appendChild(row);
  });
}

function bindCustomPaletteControls() {
  const addButton = $("btnAddCustomColor");
  if (addButton && !addButton.dataset.bound) {
    addButton.addEventListener("click", () => {
      if (customPaletteColors.length >= MAX_CUSTOM_PALETTE_COLORS) {
        setControlStatus(
          "customPaletteStatus",
          `Custom palette supports up to ${MAX_CUSTOM_PALETTE_COLORS} colours.`,
          "muted"
        );
        return;
      }
      const last = customPaletteColors[customPaletteColors.length - 1] || "#FFFFFF";
      customPaletteColors.push(last);
      renderCustomPaletteList();
    });
    addButton.dataset.bound = "1";
  }

  const applyButton = $("btnApplyCustomPalette");
  if (applyButton && !applyButton.dataset.bound) {
    applyButton.addEventListener("click", () => {
      applyCustomPalette();
    });
    applyButton.dataset.bound = "1";
  }

  const saveButton = $("btnSaveCustomPalette");
  if (saveButton && !saveButton.dataset.bound) {
    saveButton.addEventListener("click", () => {
      saveCustomPalette();
    });
    saveButton.dataset.bound = "1";
  }

  const nameInput = $("inputCustomPaletteName");
  if (nameInput && !nameInput.dataset.bound) {
    nameInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        saveCustomPalette();
      }
    });
    nameInput.dataset.bound = "1";
  }

  renderCustomPaletteList();
}

async function loadCustomPaletteDetails() {
  if (customPaletteIndex < 0) {
    renderCustomPaletteList();
    return;
  }
  try {
    const data = await fetchJson("/api/getCustomPalette");
    if (data && typeof data.index === "number") {
      customPaletteIndex = data.index;
    }
    if (data && Array.isArray(data.colors) && data.colors.length) {
      const normalized = data.colors
        .map((entry) => normalizeHexColor(entry))
        .filter(Boolean)
        .slice(0, MAX_CUSTOM_PALETTE_COLORS);
      if (normalized.length) {
        customPaletteColors = normalized;
      }
    }
  } catch (error) {
    log(`Custom palette fetch failed: ${error.message}`);
  }
  renderCustomPaletteList();
}

async function applyCustomPalette() {
  const statusId = "customPaletteStatus";
  const normalized = customPaletteColors
    .map((color) => normalizeHexColor(color))
    .filter(Boolean)
    .slice(0, MAX_CUSTOM_PALETTE_COLORS);

  if (!normalized.length) {
    setControlStatus(statusId, "Add at least one colour before applying.", "error");
    return;
  }

  customPaletteColors = normalized;
  const query = normalized.map((color) => `color=${encodeURIComponent(color)}`).join("&");
  setControlStatus(statusId, "Updating custom palette...", "muted");

  try {
    const response = await fetch(`/api/setCustomPalette?${query}&select=1`);
    const text = await response.text();
    if (!response.ok) {
      throw new Error(text || "Failed to update custom palette.");
    }
    log(text);
    setControlStatus(statusId, "Custom palette applied.", "success");
    const select = $("paletteSelect");
    if (select && customPaletteIndex >= 0) {
      select.value = String(customPaletteIndex);
      apiQueue.add(`/api/setPalette?val=${customPaletteIndex}`, () => {
        log("Custom palette selected.");
      });
    }
    toggleCustomPaletteEditor(true);
    await loadCustomPaletteDetails();
  } catch (error) {
    setControlStatus(statusId, error.message || "Unable to update custom palette.", "error");
    log(`Custom palette error: ${error.message}`);
  }
}

async function saveCustomPalette() {
  const statusId = "customPaletteStatus";
  const nameInput = $("inputCustomPaletteName");
  const paletteName = normalizePaletteName(nameInput ? nameInput.value : "");
  if (!paletteName) {
    setControlStatus(statusId, "Enter a palette name before saving.", "error");
    if (nameInput) {
      nameInput.focus();
    }
    return;
  }

  const normalized = customPaletteColors
    .map((color) => normalizeHexColor(color))
    .filter(Boolean)
    .slice(0, MAX_CUSTOM_PALETTE_COLORS);

  if (!normalized.length) {
    setControlStatus(statusId, "Add at least one colour before saving.", "error");
    return;
  }

  const select = $("paletteSelect");
  if (select) {
    const exists = Array.from(select.options).some(
      (option) => option.textContent && option.textContent.trim().toLowerCase() === paletteName.toLowerCase()
    );
    if (exists) {
      setControlStatus(statusId, "A palette with that name already exists. Pick another name.", "error");
      if (nameInput) {
        nameInput.focus();
        nameInput.select();
      }
      return;
    }
  }

  const params = new URLSearchParams();
  params.append("name", paletteName);
  normalized.forEach((color) => params.append("color", color));
  setControlStatus(statusId, `Saving "${paletteName}"...`, "muted");

  try {
    const response = await fetch(`/api/savePalette?${params.toString()}`);
    if (!response.ok) {
      const message = await response.text();
      throw new Error(message || "Failed to save palette.");
    }
    const result = await response.json().catch(() => ({}));
    await loadPalettes();
    const paletteIndex = typeof result.index === "number" ? result.index : null;
    const nextSelect = $("paletteSelect");
    if (typeof paletteIndex === "number" && nextSelect) {
      nextSelect.value = String(paletteIndex);
      toggleCustomPaletteEditor(paletteIndex === customPaletteIndex);
      apiQueue.add(`/api/setPalette?val=${paletteIndex}`, () => {
        log(`Palette changed to ${nextSelect.options[nextSelect.selectedIndex].text}`);
      });
    }
    if (nameInput) {
      nameInput.value = "";
    }
    const successName = (result && result.name) || paletteName;
    setControlStatus(statusId, `Palette "${successName}" saved.`, "success");
  } catch (error) {
    setControlStatus(statusId, error.message || "Unable to save palette.", "error");
    log(`Save palette error: ${error.message}`);
  }
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
    const queue = this;

    (async () => {
      try {
        const response = await fetch(path);
        const text = await response.text();
        if (!response.ok) {
          throw new Error(`${path} => ${response.status} ${response.statusText} :: ${text}`);
        }
        if (onSuccess) {
          const result = onSuccess(text);
          if (result && typeof result.then === "function") {
            await result;
          }
        } else {
          log(text);
        }
      } catch (error) {
        log(`API error: ${error.message}`);
      } finally {
        queue.busy = false;
        setTimeout(() => queue.process(), 120);
      }
    })();
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
  const output = config.outputId ? $(config.outputId) : null;
  const formatValue = config.formatValue || ((value) => value);

  const updateDisplays = (value) => {
    if (output) {
      output.textContent = formatValue(value);
    }
  };

  const sendToApi = debounce((raw) => {
    const clamped = clamp(raw, config.min, config.max);
    const apiValue = config.toApi ? config.toApi(clamped) : clamped;
    apiQueue.add(`${config.endpoint}?val=${encodeURIComponent(apiValue)}`);
  }, config.debounce || 200);

  slider.addEventListener("input", () => {
    const raw = parseFloat(slider.value);
    const clamped = clamp(isNaN(raw) ? config.min : raw, config.min, config.max);
    number.value = clamped;
    updateDisplays(clamped);
    if (config.onValue) {
      config.onValue(clamped);
    }
  });

  slider.addEventListener("change", () => {
    const raw = parseFloat(slider.value);
    const clamped = clamp(isNaN(raw) ? 0 : raw, config.min, config.max);
    slider.value = clamped;
    number.value = clamped;
    updateDisplays(clamped);
    if (config.onValue) {
      config.onValue(clamped);
    }
    sendToApi(clamped);
  });

  number.addEventListener("input", () => {
    const raw = parseFloat(number.value);
    const clamped = clamp(isNaN(raw) ? config.min : raw, config.min, config.max);
    slider.value = clamped;
    updateDisplays(clamped);
    if (config.onValue) {
      config.onValue(clamped);
    }
  });

  number.addEventListener("change", () => {
    const raw = parseFloat(number.value);
    const clamped = clamp(isNaN(raw) ? 0 : raw, config.min, config.max);
    number.value = clamped;
    slider.value = clamped;
    updateDisplays(clamped);
    if (config.onValue) {
      config.onValue(clamped);
    }
    sendToApi(clamped);
  });

  controlBindings[key] = {
    setFromApi(apiValue) {
      const uiValue = config.fromApi ? config.fromApi(apiValue) : apiValue;
      const clamped = clamp(uiValue, config.min, config.max);
      slider.value = clamped;
      number.value = clamped;
      updateDisplays(clamped);
      if (config.onValue) {
        config.onValue(clamped);
      }
    },
  };

  const initial = clamp(parseFloat(slider.value) || config.min, config.min, config.max);
  slider.value = initial;
  number.value = initial;
  updateDisplays(initial);
}

function applyBindingValue(key, apiValue) {
  if (controlBindings[key] && controlBindings[key].setFromApi !== undefined) {
    controlBindings[key].setFromApi(apiValue);
  }
}

/************************************************
 * Game of Life toggle
 ************************************************/
function setLabelText(id, text) {
  const element = $(id);
  if (element) {
    element.textContent = text;
  }
}

function setHelpText(id, text) {
  const element = $(id);
  if (element) {
    element.textContent = text;
  }
}

function applyGameOfLifeLabels(isGoL) {
  if (isGoL) {
    setLabelText("labelSpeed", "Generation Speed:");
    setLabelText("labelFade", "Highlight Brightness:");
    setLabelText("labelTail", "Fade Smoothness:");
    setLabelText("labelSpawn", defaultLabelText.spawn);
    setLabelText("labelMaxFlakes", defaultLabelText.maxFlakes);
    setHelpText("helpSpeed", "Shorter delay between generations across all GoL modes. 0 = slowest, 100 = fastest.");
    setHelpText("helpFade", "Boosts the sweep cursor and fresh births. Lower for a softer highlight.");
    setHelpText("helpTail", "Higher values ease cells in/out; lower values snap them on and off.");
  } else {
    setLabelText("labelSpeed", defaultLabelText.speed);
    setLabelText("labelFade", defaultLabelText.fade);
    setLabelText("labelTail", defaultLabelText.tail);
    setLabelText("labelSpawn", defaultLabelText.spawn);
    setLabelText("labelMaxFlakes", defaultLabelText.maxFlakes);
    setHelpText("helpSpeed", defaultHelpText.speed);
    setHelpText("helpFade", defaultHelpText.fade);
    setHelpText("helpTail", defaultHelpText.tail);
  }

  if (isGoL) {
    const modeSelect = $("selectGoLMode");
    if (modeSelect) {
      updateGoLModeUI(parseInt(modeSelect.value, 10));
    }
  }
}

function updateGoLModeUI(mode) {
  const highlightIds = ["columnSkipContainer", "highlightWidthContainer", "highlightColorContainer"];
  const showSweepOptions = mode === 0;
  highlightIds.forEach((id) => {
    const el = $(id);
    if (el) {
      el.style.display = showSweepOptions ? (id === "highlightColorContainer" ? "flex" : "grid") : "none";
    }
  });
  const highlightNumber = $("numHighlightWidth");
  const highlightSlider = $("sliderHighlightWidth");
  const highlightColor = $("inputHighlightColor");
  if (highlightNumber) {
    highlightNumber.disabled = !showSweepOptions;
  }
  if (highlightSlider) {
    highlightSlider.disabled = !showSweepOptions;
  }
  if (highlightColor) {
    highlightColor.disabled = !showSweepOptions;
  }

  const description = $("golModeDescription");
  if (description) {
    description.textContent = golModeDescriptions[mode] || "";
  }
}

function updateAutomataLabels() {
  const meta = automataDescriptors[currentAutomataMode] || automataDescriptors[0];
  const primarySlider = $("sliderAutomataPrimary");
  const secondarySlider = $("sliderAutomataSecondary");
  const primaryValue = primarySlider ? parseInt(primarySlider.value, 10) || 0 : 0;
  const secondaryValue = secondarySlider ? parseInt(secondarySlider.value, 10) || 0 : 0;

  setLabelText("labelAutomataPrimary", `${meta.primary.label}: ${meta.primary.formatter(primaryValue)}`);
  setHelpText("helpAutomataPrimary", meta.primary.help);
  setLabelText("labelAutomataSecondary", `${meta.secondary.label}: ${meta.secondary.formatter(secondaryValue)}`);
  setHelpText("helpAutomataSecondary", meta.secondary.help);

  const modeHelp = $("automataModeHelp");
  if (modeHelp) {
    modeHelp.textContent = meta.name;
  }
}

function toggleCommonControlsForAnimation(animIndex) {
  const showFade = [0,1,2,3,4].includes(animIndex);
  const showTail = [0,1,2,3,4].includes(animIndex);
  const showSpawn = [0,3].includes(animIndex);
  const showMax = [0,3].includes(animIndex);
  const map = [
    { id: "fadeContainer", show: showFade },
    { id: "tailContainer", show: showTail },
    { id: "spawnContainer", show: showSpawn },
    { id: "maxFlakesContainer", show: showMax },
  ];
  map.forEach(({ id, show }) => {
    const row = $(id);
    if (row) {
      row.style.display = show ? "flex" : "none";
      Array.from(row.querySelectorAll("input,select,button")).forEach((el) => {
        el.disabled = !show;
      });
    }
  });
}
function toggleAutomataControls(show) {
  const displayMap = {
    automataModeContainer: "flex",
    automataPrimaryContainer: "flex",
    automataSecondaryContainer: "flex",
    automataResetContainer: "flex",
  };

  Object.keys(displayMap).forEach((id) => {
    const element = $(id);
    if (element) {
      element.style.display = show ? displayMap[id] : "none";
    }
  });

  ["selectAutomataMode", "sliderAutomataPrimary", "numAutomataPrimary", "sliderAutomataSecondary", "numAutomataSecondary", "btnAutomataReset"].forEach((id) => {
    const element = $(id);
    if (element) {
      element.disabled = !show;
    }
  });

  if (show) {
    updateAutomataLabels();
  }
  updatePreviewParameters();
  setPreviewModeFromAnimation(currentAnimationIndex);
  toggleCommonControlsForAnimation(currentAnimationIndex);
  bindCryptoTickerControls();
}

function toggleTextScrollerControls(show) {
  const ids = [
    "textModeContainer",
    "textPrimaryContainer",
    "textDirectionContainer",
    "textGlyphOptionsContainer",
    "textLeftContainer",
    "textRightContainer",
    "textSpeedContainer",
    "cryptoTickerContainer",
  ];
  ids.forEach((id) => {
    const element = $(id);
    if (element) {
      element.style.display = show ? "flex" : "none";
      Array.from(element.querySelectorAll("input,select,button")).forEach((control) => {
        control.disabled = !show;
      });
    }
  });
}

/************************************************
 * Crypto ticker (browser-side fetch -> text ticker)
 ************************************************/
function bindCryptoTickerControls() {
  const startBtn = $("btnStartCryptoTicker");
  const stopBtn = $("btnStopCryptoTicker");
  if (startBtn && !startBtn.dataset.bound) {
    startBtn.addEventListener("click", () => startCryptoTicker());
    startBtn.dataset.bound = "1";
  }
  if (stopBtn && !stopBtn.dataset.bound) {
    stopBtn.addEventListener("click", stopCryptoTicker);
    stopBtn.dataset.bound = "1";
  }
}

function parseCryptoList() {
  const input = $("inputCryptoList");
  if (!input || !input.value.trim()) {
    return ["bitcoin", "ethereum", "solana", "dogecoin"];
  }
  return input.value
    .split(",")
    .map((id) => id.trim().toLowerCase())
    .filter(Boolean)
    .slice(0, 8);
}

function formatCryptoSymbol(id) {
  return id.toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 3) || id.toUpperCase();
}

async function fetchCryptoPrices(ids) {
  const statusId = "cryptoTickerStatus";
  if (!ids.length) {
    setControlStatus(statusId, "Add at least one coin id (e.g., bitcoin, ethereum).", "error");
    return null;
  }
  const url = `https://api.coingecko.com/api/v3/simple/price?ids=${encodeURIComponent(
    ids.join(",")
  )}&vs_currencies=usd&precision=3`;
  try {
    const data = await fetchJson(url);
    return data;
  } catch (err) {
    setControlStatus(statusId, `Crypto fetch failed: ${err.message}`, "error");
    return null;
  }
}

function buildCryptoTickerString(ids, priceData) {
  const parts = [];
  ids.forEach((id) => {
    const entry = priceData[id];
    if (!entry || typeof entry.usd !== "number") return;
    const symbol = formatCryptoSymbol(id);
    const price =
      entry.usd >= 100 ? entry.usd.toFixed(0) : entry.usd >= 10 ? entry.usd.toFixed(2) : entry.usd.toFixed(3);
    parts.push(`${symbol} - $${price}`);
  });
  return parts.join("  •  ");
}

async function applyCryptoTickerOnce() {
  const statusId = "cryptoTickerStatus";
  const ids = parseCryptoList();
  setControlStatus(statusId, "Fetching prices...", "muted");
  const prices = await fetchCryptoPrices(ids);
  if (!prices) return;
  const message = buildCryptoTickerString(ids, prices);
  if (!message) {
    setControlStatus(statusId, "No price data returned.", "error");
    return;
  }
  // Force text ticker mode
  apiQueue.add(`/api/setAnimation?val=${TEXT_TICKER_INDEX}`, () => {
    currentAnimationIndex = TEXT_TICKER_INDEX;
    toggleTextScrollerControls(true);
    setPreviewModeFromAnimation(TEXT_TICKER_INDEX);
    toggleCommonControlsForAnimation(TEXT_TICKER_INDEX);
    log("Switched to TextTicker for crypto feed");
  });
  apiQueue.add(`/api/text/setMode?val=0`, () => {});
  apiQueue.add(`/api/text/setMessage?slot=0&value=${encodeURIComponent(message)}`, () => {
    setControlStatus(statusId, "Crypto ticker updated.", "success");
  });
  cryptoTickerLastUpdate = Date.now();
}

function startCryptoTicker() {
  const statusId = "cryptoTickerStatus";
  stopCryptoTicker(); // clear any existing interval
  applyCryptoTickerOnce();
  cryptoTickerInterval = setInterval(() => {
    // avoid overlapping fetches
    if (Date.now() - cryptoTickerLastUpdate < CRYPTO_REFRESH_MS / 2) return;
    applyCryptoTickerOnce();
  }, CRYPTO_REFRESH_MS);
  setControlStatus(statusId, "Crypto ticker running.", "info");
}

function stopCryptoTicker() {
  const statusId = "cryptoTickerStatus";
  if (cryptoTickerInterval) {
    clearInterval(cryptoTickerInterval);
    cryptoTickerInterval = null;
  }
  setControlStatus(statusId, "Crypto ticker stopped.", "muted");
}

function toggleGameOfLifeControls(show) {
  const displayMap = {
    golModeContainer: "flex",
    golRuleContainer: "flex",
    golNeighborContainer: "flex",
    golWrapContainer: "flex",
    golClusterContainer: "flex",
    columnSkipContainer: "flex",
    highlightWidthContainer: "flex",
    highlightColorContainer: "flex",
    seedDensityContainer: "flex",
    mutationChanceContainer: "flex",
  };

  Object.keys(displayMap).forEach((id) => {
    const element = $(id);
    if (element) {
      element.style.display = show ? displayMap[id] : "none";
    }
  });
  if (show) {
    const modeSelect = $("selectGoLMode");
    if (modeSelect) {
      updateGoLModeUI(parseInt(modeSelect.value, 10));
    }
  }
  ["sliderSeedDensity", "numSeedDensity", "sliderMutationChance", "numMutationChance", "sliderHighlightWidth", "numHighlightWidth", "inputHighlightColor", "chkUniformBirths", "inputUniformBirthColor", "sliderColumnSkip", "numColumnSkip"].forEach((id) => {
    const control = $(id);
    if (control) {
      control.disabled = !show;
    }
  });
  applyGameOfLifeLabels(show);
  if (!show) {
    setHelpText("golModeDescription", "");
  }
  if (show) {
    setPreviewMode("gol");
  } else {
    setPreviewModeFromAnimation(currentAnimationIndex);
  toggleCommonControlsForAnimation(currentAnimationIndex);
  }
  updatePreviewParameters();
}

/************************************************
 * UI setup
 ************************************************/
function setupUIControls() {
  log("Setting up UI controls...");

  setHelpText("helpSpeed", defaultHelpText.speed);
  setHelpText("helpFade", defaultHelpText.fade);
  setHelpText("helpTail", defaultHelpText.tail);
  bindCustomPaletteControls();

  bindSliderToApi("brightness", {
    sliderId: "sliderBrightness",
    numberId: "numBrightness",
    endpoint: "/api/setBrightness",
    min: 0,
    max: 255,
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("fade", {
    sliderId: "sliderFade",
    numberId: "numFade",
    endpoint: "/api/setFadeAmount",
    min: 0,
    max: 255,
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("tail", {
    sliderId: "sliderTail",
    numberId: "numTail",
    endpoint: "/api/setTailLength",
    min: 0,
    max: 20,
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("speed", {
    sliderId: "sliderSpeed",
    numberId: "numSpeed",
    endpoint: "/api/setSpeed",
    min: 0,
    max: 100,
    formatValue: (value) => `${Math.round(value)}%`,
    onValue: updatePreviewParameters,
  });

  bindSliderToApi("spawn", {
    sliderId: "sliderSpawn",
    numberId: "numSpawn",
    endpoint: "/api/setSpawnRate",
    min: 0,
    max: 100,
    toApi: (value) => (value / 100).toFixed(2),
    fromApi: (value) => Math.round(parseFloat(value) * 100),
    formatValue: (value) => `${Math.round(value)}%`,
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
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("columnSkip", {
    sliderId: "sliderColumnSkip",
    numberId: "numColumnSkip",
    endpoint: "/api/setColumnSkip",
    min: 1,
    max: 20,
    formatValue: (value) => Math.round(value),
    onValue: updatePreviewParameters,
  });

  bindSliderToApi("textSpeed", {
    sliderId: "sliderTextSpeed",
    numberId: "numTextSpeed",
    endpoint: "/api/text/setSpeed",
    min: 0,
    max: 100,
    formatValue: (value) => `${Math.round(value)}%`,
  });

  bindSliderToApi("automataPrimary", {
    sliderId: "sliderAutomataPrimary",
    numberId: "numAutomataPrimary",
    endpoint: "/api/automata/setPrimary",
    min: 0,
    max: 100,
    onValue: (value) => {
      updateAutomataLabels();
      updatePreviewParameters();
    },
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("automataSecondary", {
    sliderId: "sliderAutomataSecondary",
    numberId: "numAutomataSecondary",
    endpoint: "/api/automata/setSecondary",
    min: 0,
    max: 100,
    onValue: (value) => {
      updateAutomataLabels();
      updatePreviewParameters();
    },
    formatValue: (value) => Math.round(value),
  });

  bindSliderToApi("gofHighlightWidth", {
    sliderId: "sliderHighlightWidth",
    numberId: "numHighlightWidth",
    endpoint: "/api/gol/setHighlightWidth",
    min: 0,
    max: 4,
    formatValue: (value) => Math.round(value),
    onValue: updatePreviewParameters,
  });

  bindSliderToApi("golSeedDensity", {
    sliderId: "sliderSeedDensity",
    numberId: "numSeedDensity",
    endpoint: "/api/gol/setSeedDensity",
    min: 5,
    max: 100,
    formatValue: (value) => `${Math.round(value)}%`,
    onValue: updatePreviewParameters,
  });

  bindSliderToApi("golMutationChance", {
    sliderId: "sliderMutationChance",
    numberId: "numMutationChance",
    endpoint: "/api/gol/setMutation",
    min: 0,
    max: 30,
    formatValue: (value) => `${Math.round(value)}%`,
  });

  const automataModeSelect = $("selectAutomataMode");
  if (automataModeSelect && !automataModeSelect.dataset.bound) {
    automataModeSelect.addEventListener("change", () => {
      const mode = parseInt(automataModeSelect.value, 10);
      currentAutomataMode = Number.isNaN(mode) ? 0 : mode;
      updateAutomataLabels();
      setPreviewModeFromAnimation(currentAnimationIndex);
  toggleCommonControlsForAnimation(currentAnimationIndex);
      updatePreviewParameters();
      apiQueue.add(`/api/automata/setMode?val=${currentAutomataMode}`);
    });
    automataModeSelect.dataset.bound = "1";
  }

  const automataResetButton = $("btnAutomataReset");
  if (automataResetButton && !automataResetButton.dataset.bound) {
    automataResetButton.addEventListener("click", () => {
      apiQueue.add("/api/automata/reset");
    });
    automataResetButton.dataset.bound = "1";
  }

  const highlightColorInput = $("inputHighlightColor");
  if (highlightColorInput) {
    const sendColor = debounce(() => {
      const hex = highlightColorInput.value;
      if (hex && /^#?[0-9a-fA-F]{6}$/.test(hex)) {
        apiQueue.add(`/api/gol/setHighlightColor?hex=${encodeURIComponent(hex)}`);
      }
    }, 200);
    highlightColorInput.addEventListener("input", sendColor);
    controlBindings.gofHighlightColor = {
      setFromApi(hex) {
        if (hex && hex.length) {
          highlightColorInput.value = hex.startsWith("#") ? hex : `#${hex}`;
        }
      },
    };
  }

  const uniformCheck = $("chkUniformBirths");
  const uniformColorInput = $("inputUniformBirthColor");
  if (uniformCheck) {
    uniformCheck.addEventListener("change", () => {
      const enabled = uniformCheck.checked ? 1 : 0;
      if (uniformColorInput) {
        uniformColorInput.disabled = !uniformCheck.checked;
      }
      apiQueue.add(`/api/gol/setUniformBirths?val=${enabled}`);
    });
  }
  if (uniformColorInput) {
    const sendUniformColor = debounce(() => {
      if (uniformCheck && !uniformCheck.checked) {
        return;
      }
      const hex = uniformColorInput.value;
      if (hex && /^#?[0-9a-fA-F]{6}$/.test(hex)) {
        apiQueue.add(`/api/gol/setBirthColor?hex=${encodeURIComponent(hex)}`);
      }
    }, 200);
    uniformColorInput.addEventListener("input", sendUniformColor);
    uniformColorInput.addEventListener("change", sendUniformColor);
  }

  const modeSelect = $("selectGoLMode");
  if (modeSelect) {
    modeSelect.addEventListener("change", () => {
      const val = parseInt(modeSelect.value, 10);
      updateGoLModeUI(val);
      updatePreviewParameters();
      apiQueue.add(`/api/gol/setUpdateMode?val=${val}`);
    });
  }

  const neighborSelect = $("selectGoLNeighbor");
  if (neighborSelect) {
    neighborSelect.addEventListener("change", () => {
      const val = parseInt(neighborSelect.value, 10);
      apiQueue.add(`/api/gol/setNeighborMode?val=${val}`);
    });
  }

  const wrapSelect = $("selectGoLWrap");
  if (wrapSelect) {
    wrapSelect.addEventListener("change", () => {
      const val = wrapSelect.value === "1" ? 1 : 0;
      apiQueue.add(`/api/gol/setWrapEdges?val=${val}`);
    });
  }

  const clusterSelect = $("selectGoLCluster");
  if (clusterSelect) {
    clusterSelect.addEventListener("change", () => {
      const val = parseInt(clusterSelect.value, 10);
      apiQueue.add(`/api/gol/setClusterMode?val=${val}`);
    });
  }

  const ruleInput = $("inputGoLRule");
  const ruleButton = $("btnApplyGoLRule");
  if (ruleInput && ruleButton) {
    const applyRule = () => {
      const value = ruleInput.value.trim();
      if (value.length) {
        apiQueue.add(`/api/gol/setRules?val=${encodeURIComponent(value)}`);
      }
    };
    ruleButton.addEventListener("click", applyRule);
    ruleInput.addEventListener("keydown", (evt) => {
      if (evt.key === "Enter") {
        evt.preventDefault();
        applyRule();
      }
    });
  }

  setupTextControls();
  setupPanelControls();
  applyGameOfLifeLabels(false);
}

function setupPanelControls() {
  const panelCountSlider = $("sliderPanelCount");
  const numPanelCount = $("numPanelCount");
  if (panelCountSlider && numPanelCount) {
    const syncPanelCount = (raw) => {
      const clamped = clamp(parseInt(raw, 10) || 1, 1, 8);
      panelCountSlider.value = clamped;
      numPanelCount.value = clamped;
      return clamped;
    };
    panelCountSlider.addEventListener("input", () => {
      syncPanelCount(panelCountSlider.value);
    });
    numPanelCount.addEventListener("input", () => {
      syncPanelCount(numPanelCount.value);
    });
  }

  const btnSetPanelCount = $("btnSetPanelCount");
  if (btnSetPanelCount && numPanelCount && panelCountSlider) {
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
  setupPresetControls();
}

function setupTextControls() {
  const modeSelect = $("selectTextMode");
  if (modeSelect && !modeSelect.dataset.bound) {
    modeSelect.addEventListener("change", () => {
      const val = parseInt(modeSelect.value, 10);
      apiQueue.add(`/api/text/setMode?val=${Number.isNaN(val) ? 0 : val}`);
    });
    modeSelect.dataset.bound = "1";
  }

  const directionSelect = $("selectTextDirection");
  if (directionSelect && !directionSelect.dataset.bound) {
    directionSelect.addEventListener("change", () => {
      const val = parseInt(directionSelect.value, 10);
      apiQueue.add(`/api/text/setDirection?val=${Number.isNaN(val) ? 0 : val}`);
    });
    directionSelect.dataset.bound = "1";
  }

  const mirrorChk = $("chkMirrorGlyphs");
  if (mirrorChk && !mirrorChk.dataset.bound) {
    mirrorChk.addEventListener("change", () => {
      apiQueue.add(`/api/text/setMirror?val=${mirrorChk.checked ? 1 : 0}`);
    });
    mirrorChk.dataset.bound = "1";
  }

  const compactChk = $("chkCompactGlyphs");
  if (compactChk && !compactChk.dataset.bound) {
    compactChk.addEventListener("change", () => {
      apiQueue.add(`/api/text/setCompact?val=${compactChk.checked ? 1 : 0}`);
    });
    compactChk.dataset.bound = "1";
  }

  const orderSelect = $("selectTextOrder");
  if (orderSelect && !orderSelect.dataset.bound) {
    orderSelect.addEventListener("change", () => {
      const val = parseInt(orderSelect.value, 10);
      apiQueue.add(`/api/text/setOrder?val=${Number.isNaN(val) ? 0 : val}`);
    });
    orderSelect.dataset.bound = "1";
  }

  const bindTextInput = (id, slot) => {
    const input = $(id);
    if (!input || input.dataset.bound) return;
    const sendUpdate = debounce(() => {
      const value = input.value ? input.value.trim() : "";
      apiQueue.add(`/api/text/setMessage?slot=${slot}&value=${encodeURIComponent(value)}`);
    }, 350);
    input.addEventListener("input", sendUpdate);
    input.addEventListener("change", sendUpdate);
    input.dataset.bound = "1";
  };

  bindTextInput("inputTextPrimary", "primary");
  bindTextInput("inputTextLeft", "left");
  bindTextInput("inputTextRight", "right");
}

function setupPresetControls() {
  const saveButton = $("btnSavePreset");
  const nameInput = $("inputPresetName");
  const loadButton = $("btnLoadPreset");
  const deleteButton = $("btnDeletePreset");
  const presetSelect = $("presetSelect");

  if (saveButton && nameInput) {
    saveButton.addEventListener("click", () => {
      const raw = nameInput.value.trim();
      if (!raw) {
        log("Enter a preset name before saving.");
        return;
      }
      apiQueue.add(`/api/presets/save?name=${encodeURIComponent(raw)}`, async () => {
        log(`Preset '${raw}' saved.`);
        nameInput.value = "";
        await loadPresets();
      });
    });
  }

  if (loadButton && presetSelect) {
    loadButton.addEventListener("click", () => {
      const selected = presetSelect.value;
      if (!selected) {
        log("Select a preset to load.");
        return;
      }
      apiQueue.add(`/api/presets/load?name=${encodeURIComponent(selected)}`, async () => {
        log(`Preset '${selected}' loaded.`);
        await loadAnimations();
        await loadPalettes();
        await loadSettings();
        await loadPresets();
      });
    });
  }

  if (deleteButton && presetSelect) {
    deleteButton.addEventListener("click", () => {
      const selected = presetSelect.value;
      if (!selected) {
        log("Select a preset to delete.");
        return;
      }
      apiQueue.add(`/api/presets/delete?name=${encodeURIComponent(selected)}`, async () => {
        log(`Preset '${selected}' deleted.`);
        await loadPresets();
      });
    });
  }
}

async function loadPresets() {
  try {
    const select = $("presetSelect");
    if (!select) return;
    const previous = select.value;
    const data = await fetchJson("/api/presets/list");
    const loadBtn = $("btnLoadPreset");
    const deleteBtn = $("btnDeletePreset");

    select.innerHTML = "";

    if (data.presets && data.presets.length) {
      data.presets.forEach((name) => {
        const option = document.createElement("option");
        option.value = name;
        option.textContent = name;
        select.appendChild(option);
      });
      select.disabled = false;
      if (loadBtn) loadBtn.disabled = false;
      if (deleteBtn) deleteBtn.disabled = false;
      if (previous) {
        const match = data.presets.find((name) => name.toLowerCase() === previous.toLowerCase());
        if (match) {
          select.value = match;
        }
      }
      setControlStatus("presetStatus", "");
    } else {
      const option = document.createElement("option");
      option.value = "";
      option.textContent = "No presets saved";
      select.appendChild(option);
      select.disabled = true;
      if (loadBtn) loadBtn.disabled = true;
      if (deleteBtn) deleteBtn.disabled = true;
      setControlStatus("presetStatus", "No presets saved yet. Create one from current settings.", "muted");
    }
  } catch (err) {
    log(`Preset list fetch failed: ${err.message}`);
    setControlStatus("presetStatus", "Unable to load presets. Try again after ensuring Wi-Fi is up.", "error");
  }
}

async function loadPanelConfig() {
  try {
    const data = await fetchJson("/api/panel/getConfig");
    const orderSelect = $("panelOrder");
    if (orderSelect && data.order) {
      orderSelect.value = data.order;
    }
    if (data.rotation1 !== undefined) {
      setValue("rotatePanel1", data.rotation1);
    }
    if (data.rotation2 !== undefined) {
      setValue("rotatePanel2", data.rotation2);
    }
  } catch (err) {
    log(`Panel config fetch failed: ${err.message}`);
  }
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
    toggleAutomataControls(currentAnimationIndex === STRANGE_LOOP_INDEX);
    toggleTextScrollerControls(currentAnimationIndex === TEXT_TICKER_INDEX);
    setPreviewModeFromAnimation(currentAnimationIndex);
    toggleCommonControlsForAnimation(currentAnimationIndex);
    bindCryptoTickerControls();
    setControlStatus("animationStatus", "");

    if (!select.dataset.bound) {
      select.addEventListener("change", () => {
        const val = parseInt(select.value, 10);
        apiQueue.add(`/api/setAnimation?val=${val}`, () => {
          currentAnimationIndex = val;
          toggleGameOfLifeControls(val === GAME_OF_LIFE_INDEX);
          toggleAutomataControls(val === STRANGE_LOOP_INDEX);
          toggleTextScrollerControls(val === TEXT_TICKER_INDEX);
          setPreviewModeFromAnimation(val);
          toggleCommonControlsForAnimation(val);
          log(`Animation changed to ${select.options[select.selectedIndex].text}`);
        });
      });
      select.dataset.bound = "1";
    }
  } catch (error) {
    log(`Failed to load animations: ${error.message}`);
    setControlStatus("animationStatus", "Unable to load animation list. Check device connection.", "error");
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

    customPaletteIndex = data.palettes.findIndex(
      (name) => typeof name === "string" && name.toLowerCase() === "custom"
    );

    select.value = data.current;
    setControlStatus("paletteStatus", "");

    if (!select.dataset.bound) {
      select.addEventListener("change", () => {
        const val = parseInt(select.value, 10);
        toggleCustomPaletteEditor(val === customPaletteIndex);
        apiQueue.add(`/api/setPalette?val=${val}`, () => {
          log(`Palette changed to ${select.options[select.selectedIndex].text}`);
        });
      });
      select.dataset.bound = "1";
    }

    toggleCustomPaletteEditor(parseInt(select.value, 10) === customPaletteIndex);

    if (customPaletteIndex >= 0) {
      await loadCustomPaletteDetails();
    } else {
      setControlStatus("customPaletteStatus", "");
      renderCustomPaletteList();
    }
  } catch (error) {
    log(`Failed to load palettes: ${error.message}`);
    setControlStatus("paletteStatus", "Palette list unavailable. Reload after reconnecting.", "error");
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

async function fetchText(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${url} => ${response.status} ${response.statusText}`);
  }
  return response.text();
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
      const highlightInfo = await fetchJson("/api/gol/getHighlight");
      if (highlightInfo) {
        if (highlightInfo.width !== undefined) {
          applyBindingValue("gofHighlightWidth", highlightInfo.width);
        }
        if (highlightInfo.color && controlBindings.gofHighlightColor) {
          controlBindings.gofHighlightColor.setFromApi(highlightInfo.color);
        }
      }
    } catch (err) {
      log(`Highlight fetch failed: ${err.message}`);
    }

    try {
      const config = await fetchJson("/api/gol/getConfig");
      if (config) {
        const modeSelect = $("selectGoLMode");
        if (modeSelect && config.updateMode !== undefined) {
          modeSelect.value = String(config.updateMode);
          updateGoLModeUI(parseInt(modeSelect.value, 10));
        }
        const neighborSelect = $("selectGoLNeighbor");
        if (neighborSelect && config.neighborMode !== undefined) {
          neighborSelect.value = String(config.neighborMode);
        }
        const wrapSelect = $("selectGoLWrap");
        if (wrapSelect && config.wrapEdges !== undefined) {
          wrapSelect.value = config.wrapEdges ? "1" : "0";
        }
        const clusterSelect = $("selectGoLCluster");
        if (clusterSelect && config.clusterColorMode !== undefined) {
          clusterSelect.value = String(config.clusterColorMode);
        }
        const ruleInput = $("inputGoLRule");
        if (ruleInput && config.rule) {
          ruleInput.value = config.rule;
        }
        if (config.seedDensity !== undefined) {
          applyBindingValue("golSeedDensity", config.seedDensity);
        }
        if (config.mutationChance !== undefined) {
          applyBindingValue("golMutationChance", config.mutationChance);
        }
        const uniformCheck = $("chkUniformBirths");
        if (uniformCheck && config.uniformBirths !== undefined) {
          uniformCheck.checked = !!config.uniformBirths;
          uniformCheck.disabled = false;
        }
        const uniformColorInput = $("inputUniformBirthColor");
        if (uniformColorInput && config.birthColor) {
          uniformColorInput.value = config.birthColor.startsWith("#") ? config.birthColor : `#${config.birthColor}`;
        }
        if (uniformColorInput) {
          uniformColorInput.disabled = !(uniformCheck && uniformCheck.checked);
        }
      }
    } catch (err) {
      log(`GoL config fetch failed: ${err.message}`);
    }

    try {
      const automataConfig = await fetchJson("/api/automata/getConfig");
      if (automataConfig) {
        if (automataConfig.mode !== undefined) {
          currentAutomataMode = parseInt(automataConfig.mode, 10);
          if (Number.isNaN(currentAutomataMode)) {
            currentAutomataMode = 0;
          }
          const autoSelect = $("selectAutomataMode");
          if (autoSelect) {
            autoSelect.value = String(currentAutomataMode);
          }
        }
        if (automataConfig.primary !== undefined) {
          applyBindingValue("automataPrimary", automataConfig.primary);
        }
        if (automataConfig.secondary !== undefined) {
          applyBindingValue("automataSecondary", automataConfig.secondary);
        }
        updateAutomataLabels();
      }
    } catch (err) {
      log(`Automata config fetch failed: ${err.message}`);
    }

    try {
      const textConfig = await fetchJson("/api/text/getConfig");
      if (textConfig) {
        if (textConfig.mode !== undefined) {
          const select = $("selectTextMode");
          if (select) {
            select.value = String(textConfig.mode);
          }
        }
        if (textConfig.direction !== undefined) {
          const select = $("selectTextDirection");
          if (select) {
            select.value = String(textConfig.direction);
          }
        }
        if (textConfig.mirror !== undefined) {
          const chk = $("chkMirrorGlyphs");
          if (chk) chk.checked = !!textConfig.mirror;
        }
        if (textConfig.compact !== undefined) {
          const chk = $("chkCompactGlyphs");
          if (chk) chk.checked = !!textConfig.compact;
        }
        if (textConfig.reverse !== undefined) {
          const select = $("selectTextOrder");
          if (select) {
            select.value = String(textConfig.reverse ? 1 : 0);
          }
        }
        if (textConfig.speed !== undefined) {
          applyBindingValue("textSpeed", textConfig.speed);
        }
        if (textConfig.primary !== undefined) {
          setValue("inputTextPrimary", textConfig.primary);
        }
        if (textConfig.left !== undefined) {
          setValue("inputTextLeft", textConfig.left);
        }
        if (textConfig.right !== undefined) {
          setValue("inputTextRight", textConfig.right);
        }
      }
    } catch (err) {
      log(`Text config fetch failed: ${err.message}`);
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
    await loadPanelConfig();
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
  initializePanelPreview();
  openLogSocket();

  await loadAnimations();
  await loadPalettes();
  await loadSettings();
  await loadPresets();

  toggleGameOfLifeControls(currentAnimationIndex === GAME_OF_LIFE_INDEX);
  toggleAutomataControls(currentAnimationIndex === STRANGE_LOOP_INDEX);
  toggleTextScrollerControls(currentAnimationIndex === TEXT_TICKER_INDEX);
  setPreviewModeFromAnimation(currentAnimationIndex);
  toggleCommonControlsForAnimation(currentAnimationIndex);
  updatePreviewParameters();

  bindCryptoTickerControls();

  log("Control panel ready!");
}

document.addEventListener("DOMContentLoaded", () => {
  initializeControlPanel();
});
