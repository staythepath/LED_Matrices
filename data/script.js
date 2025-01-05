window.addEventListener("load", function () {
  // Grab references to our sliders and the <span> elements that show their values
  let spawnSlider = document.getElementById("spawnSlider");
  let spawnValue = document.getElementById("spawnValue");
  let fadeSlider = document.getElementById("fadeSlider");
  let fadeValue = document.getElementById("fadeValue");

  // Update the displayed text whenever the spawn slider is moved
  spawnSlider.addEventListener("input", function () {
    spawnValue.innerText = this.value;
    // We can immediately send the update to the ESP32
    sendUpdateToESP32();
  });

  // Update the displayed text whenever the fade slider is moved
  fadeSlider.addEventListener("input", function () {
    fadeValue.innerText = this.value;
    // We can immediately send the update to the ESP32
    sendUpdateToESP32();
  });

  // Function to send the updated slider values to the ESP32
  function sendUpdateToESP32() {
    let spawn = spawnSlider.value;
    let fade = fadeSlider.value;
    console.log("Sending spawn=" + spawn + ", fade=" + fade);

    // Example endpoint: /setParams?spawn=xxx&fade=xxx
    // You need a matching route in your code (e.g., in setupRoutes())
    fetch(`/setParams?spawn=${spawn}&fade=${fade}`)
      .then((res) => res.text())
      .then((data) => {
        console.log("Response from ESP32:", data);
      })
      .catch((err) => {
        console.error("Error:", err);
      });
  }
});

function loadAnimationList() {
  fetch("/api/listAnimations")
    .then((r) => r.json())
    .then((data) => {
      selAnimation.innerHTML = "";
      data.forEach((animName, idx) => {
        const opt = document.createElement("option");
        opt.value = idx;
        opt.innerText = `${idx}: ${animName}`;
        selAnimation.appendChild(opt);
      });
    })
    .catch((err) => log("Error fetching animations: " + err));
}

/********************************************************
 * 1) Define a log scale mapping.
 *    We'll map slider range [0..100] -> speed [10..60000].
 ********************************************************/
function sliderToSpeed(sliderVal) {
  // sliderVal = 0..100
  // let's define a log scale from ln(10) to ln(60000).
  const minSpeed = 10;
  const maxSpeed = 60000;
  const minLog = Math.log(minSpeed); // ln(10) ~ 2.302585
  const maxLog = Math.log(maxSpeed); // ln(60000) ~ 11.0021
  // fraction = sliderVal / 100 => 0..1
  const fraction = sliderVal / 100.0;
  // Now do log interpolation:
  const scale = minLog + (maxLog - minLog) * fraction;
  // exponentiate back:
  const speed = Math.round(Math.exp(scale));
  return speed; // e.g. 80 -> ~ some ms
}

function speedToSliderVal(speed) {
  // speed in [10..60000], map back to [0..100].
  const minSpeed = 10;
  const maxSpeed = 60000;
  const minLog = Math.log(minSpeed);
  const maxLog = Math.log(maxSpeed);

  // clamp speed just in case
  if (speed < minSpeed) speed = minSpeed;
  if (speed > maxSpeed) speed = maxSpeed;

  const scale = Math.log(speed);
  const fraction = (scale - minLog) / (maxLog - minLog);
  return Math.round(fraction * 100.0);
}

/********************************************************
 * 2) Setup code for the slider + text box
 ********************************************************/
// references to the slider, text box, and label
const sSpeed = document.getElementById("sliderSpeed");
const tSpeed = document.getElementById("txtSpeed");
const lSpeed = document.getElementById("lblSpeed");

// A function to update the text box + label
function updateSpeedUI(speed) {
  // update text box
  tSpeed.value = speed;
  // update label
  lSpeed.innerText = speed;
  // update slider
  sSpeed.value = speedToSliderVal(speed);
}

/********************************************************
 * 3) When the slider changes, convert slider->speed,
 *    update text box + label, call /api/setSpeed
 ********************************************************/
sSpeed.addEventListener("change", () => {
  const sliderVal = parseInt(sSpeed.value);
  const spd = sliderToSpeed(sliderVal);
  updateSpeedUI(spd);

  // Now we do the fetch to set speed
  fetch(`/api/setSpeed?val=${spd}`)
    .then((r) => r.text())
    .then((txt) => console.log(txt))
    .catch((err) => console.log("setSpeed error:", err));
});

/********************************************************
 * 4) When the user edits the text box, we can do two approaches:
 *    - On "change" event
 *    - On "keypress" for Enter
 *    We'll do "change" for simplicity:
 ********************************************************/
tSpeed.addEventListener("change", () => {
  let typedVal = parseInt(tSpeed.value);
  if (isNaN(typedVal)) typedVal = 80; // some default
  // clamp
  if (typedVal < 10) typedVal = 10;
  if (typedVal > 60000) typedVal = 60000;

  // update slider + label
  updateSpeedUI(typedVal);

  // call the server
  fetch(`/api/setSpeed?val=${typedVal}`)
    .then((r) => r.text())
    .then((txt) => console.log(txt))
    .catch((err) => console.log("setSpeed error:", err));
});

/********************************************************
 * 5) On page load, you probably do:
 *    fetch("/api/getSpeed") -> we get an integer e.g. 80
 *    then call updateSpeedUI(80)
 ********************************************************/
// Example snippet in your "load" event:
window.addEventListener("load", () => {
  fetch("/api/getSpeed")
    .then((r) => r.text())
    .then((val) => {
      let spd = parseInt(val);
      if (isNaN(spd)) spd = 80;
      updateSpeedUI(spd);
    })
    .catch((err) => console.log("getSpeed error:", err));
});
