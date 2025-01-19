#include <WiFi.h>
#include <FastLED.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <vector>

// -------------------- Configuration --------------------

// LED Configuration
#define LED_PIN     7
#define NUM_LEDS    512
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define DEFAULT_BRIGHTNESS 30 // Initial brightness (0-255)

// DHT Sensor Configuration
#define DHTPIN      4
#define DHTTYPE     DHT11

// LCD Configuration: RS, E, D4, D5, D6, D7
const int rs = 19;
const int e  = 23;
const int d4 = 18;
const int d5 = 17;
const int d6 = 16;
const int d7 = 15;

// Optional: LED for OTA status
#define OTA_LED_PIN 2 // Replace with your LED pin if different

// Wi-Fi Credentials
const char* ssid     = "The Tardis";      // Replace with your Wi-Fi SSID
const char* password = "28242824";        // Replace with your Wi-Fi Password

// NTP Server Configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6 * 3600;    // Adjust GMT offset as needed
const int   daylightOffset_sec = 0;       // Adjust daylight offset as needed

// -------------------- FastLED Setup --------------------
CRGB leds[NUM_LEDS];
uint8_t brightness = DEFAULT_BRIGHTNESS;

// -------------------- DHT Setup --------------------
DHT dht(DHTPIN, DHTTYPE);

// -------------------- LCD Setup --------------------
LiquidCrystal lcd(rs, e, d4, d5, d6, d7);

// -------------------- Flake (Snow Effect) --------------------
struct Flake {
  int x; // Integer for precise positioning
  int y;
  int dx; // Direction in x
  int dy; // Direction in y
  float frac;
  CRGB startColor;
  CRGB endColor;
  bool bounce;
};

std::vector<Flake> flakes;

// Default parameters
int tailLength = 1 ; // Fixed tail length (e.g., 5 pixels)
float spawnRate = 0.6; // Default spawn rate (0.0-1.0)
int maxFlakes = 200; // Default maximum number of flakes
unsigned long UPDATE_INTERVAL = 10000; // 10 seconds (modifiable via Telnet)

// -------------------- Palette Definitions --------------------
std::vector<std::vector<CRGB>> ALL_PALETTES = {
  // Palette 1: blu_orange_green
  { CRGB(0, 128, 255), CRGB(255, 128, 0), CRGB(0, 200, 60), CRGB(64, 0, 128), CRGB(255, 255, 64) },
  
  // Palette 2: cool_sunset
  { CRGB(255, 100, 0), CRGB(255, 0, 102), CRGB(128, 0, 128), CRGB(0, 255, 128), CRGB(255, 255, 128) },
  
  // Palette 3: neon_tropical
  { CRGB(0, 255, 255), CRGB(255, 0, 255), CRGB(255, 255, 0), CRGB(0, 255, 0), CRGB(255, 127, 0) },
  
  // Palette 4: galaxy
  { CRGB(0, 0, 128), CRGB(75, 0, 130), CRGB(128, 0, 128), CRGB(0, 128, 128), CRGB(255, 0, 128) },
  
  // Palette 5: forest_fire
  { CRGB(34, 139, 34), CRGB(255, 69, 0), CRGB(139, 0, 139), CRGB(205, 133, 63), CRGB(255, 215, 0) },
  
  // Palette 6: cotton_candy
  { CRGB(255, 182, 193), CRGB(152, 251, 152), CRGB(135, 206, 250), CRGB(238, 130, 238), CRGB(255, 160, 122) },
  
  // Palette 7: sea_shore
  { CRGB(0, 206, 209), CRGB(127, 255, 212), CRGB(240, 230, 140), CRGB(255, 160, 122), CRGB(173, 216, 230) },
  
  // Palette 8: fire_and_ice
  { CRGB(255, 0, 0), CRGB(255, 140, 0), CRGB(255, 69, 0), CRGB(0, 255, 255), CRGB(0, 128, 255) },
  
  // Palette 9: retro_arcade
  { CRGB(255, 0, 128), CRGB(128, 0, 255), CRGB(0, 255, 128), CRGB(255, 255, 0), CRGB(255, 128, 0) },
  
  // Palette 10: royal_rainbow
  { CRGB(139, 0, 0), CRGB(218, 165, 32), CRGB(255, 0, 255), CRGB(75, 0, 130), CRGB(0, 100, 140) },
  
  // Palette 11: red
  { CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0) }
};

std::vector<String> PALETTE_NAMES = {
  "blu_orange_green",
  "cool_sunset",
  "neon_tropical",
  "galaxy",
  "forest_fire",
  "cotton_candy",
  "sea_shore",
  "fire_and_ice",
  "retro_arcade",
  "royal_rainbow",
  "red"
};

int currentPalette = 0; // Default palette index

// -------------------- Panel Order and Rotation Setup --------------------
int panelOrder = 0; // 0 = Left first, 1 = Right first

// Rotation angles for each panel
int rotationAngle1 = 90; // Rotation for Panel 1 (0, 90, 180, 270)
int rotationAngle2 = 90; // Rotation for Panel 2 (0, 90, 180, 270)

// -------------------- Telnet Server Setup --------------------

// Define Telnet Server on port 23
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// LED Update Speed Control
unsigned long ledUpdateInterval = 80; // Default: 80 ms
unsigned long lastLedUpdate = 0;

// Telnet Timeout Configuration
const unsigned long TELNET_TIMEOUT = 300000000; // 5 minutes in milliseconds
unsigned long lastTelnetActivity = 0;

// -------------------- New Variable for Tail Control --------------------
int fadeValue = 1400; // Initial fade value (controls tail length)

// -------------------- Function Prototypes --------------------
void setupWiFi();
bool getLocalTimeCustom(struct tm *info, uint32_t ms = 1000);
void updateLCD(int month, int mday, int wday, int hour, int minute, int temp_f, int hum);
void spawnFlake();
void performSnowEffect();
CRGB calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce);
int getLedIndex(int x, int y);
void rotateCoordinates(int &x, int &y, int angle);
void handleTelnet();

// -------------------- Setup Function --------------------
void setup() {
  Serial.begin(115200);
  Serial.println("WE ARE SETTING UP!@@#^!@%$)!*%^&!)#*%$*&!)_!@#)#*($%&@");
  delay(1000); // Allow time for Serial to initialize
  
  // Initialize OTA LED (Optional)
  pinMode(OTA_LED_PIN, OUTPUT);
  digitalWrite(OTA_LED_PIN, LOW); // Off by default
  
  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
  FastLED.clear();
  FastLED.show();
  
  // Initialize LCD
  lcd.begin(32, 16); // Adjusted to match 32x16 matrix
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hello, ESP32!");
  lcd.setCursor(0, 1);
  lcd.print("Arduino!");
  
  // Initialize DHT11
  dht.begin();
  
  // Connect to Wi-Fi
  setupWiFi();
  
  // Sync time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Set default palette (optional: could be random)
  currentPalette = 0; // Choose Palette 1 as default
  Serial.printf("Default Palette %d (%s) selected.\r\n", currentPalette +1, PALETTE_NAMES[currentPalette].c_str());
  
  // Initialize Telnet Server
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  Serial.println("Telnet server started on port 23\r");
  
  // Initialize Telnet Activity Timer
  lastTelnetActivity = millis();
}

// -------------------- Loop Function --------------------
void loop() {
  // Handle OTA updates
  Serial.print("WE'RE LOOPING THE OTA WORKED! ... I think...");

  ArduinoOTA.handle(); // Must be called frequently
  
  // Handle Telnet interactions
  handleTelnet();
  
  // Perform Snow Effect based on LED Update Interval
  unsigned long now = millis();
  if (now - lastLedUpdate >= ledUpdateInterval) {
    performSnowEffect();
    lastLedUpdate = now;
  }
  
  // Update LCD and Sensor Data Every UPDATE_INTERVAL
  static unsigned long lastUpdate = 0;
  
  if (now - lastUpdate >= UPDATE_INTERVAL) {
    struct tm timeinfo;
    if(!getLocalTimeCustom(&timeinfo, 1000)){
      Serial.println("Failed to obtain time\r\n");
      return;
    }
    
    int month = timeinfo.tm_mon + 1;
    int mday = timeinfo.tm_mday;
    int wday = timeinfo.tm_wday; // Sunday = 0
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    
    // Read DHT11 Sensor
    float temp_c = dht.readTemperature();
    float hum = dht.readHumidity();
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(temp_c) || isnan(hum)) {
      Serial.println("Failed to read from DHT sensor!\r\n");
      temp_c = 0;
      hum = 0;
    }
    
    // Convert to Fahrenheit
    int temp_f = (int)(temp_c * 9.0 / 5.0 + 32.0);
    int hum_int = (int)hum;
    
    // Update LCD
    updateLCD(month, mday, wday, hour, minute, temp_f, hum_int);
    
    lastUpdate = now;
  }
  
  // FastLED Show should be called after all updates
  FastLED.show();
}

// -------------------- Wi-Fi Setup --------------------
void setupWiFi(){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi\r");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Wi-Fi connected. IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup OTA after Wi-Fi connection
  ArduinoOTA.setHostname("ESP32_LEDMatrix"); // Optional: Set a unique hostname
  
  // Optional: Set OTA Password
  // ArduinoOTA.setPassword("your_password");
  
  ArduinoOTA
    .onStart([]() {
      digitalWrite(OTA_LED_PIN, HIGH); // Turn on LED during OTA
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type + "\r\n");
    })
    .onEnd([]() {
      digitalWrite(OTA_LED_PIN, LOW); // Turn off LED after OTA
      Serial.println("\r\nEnd\r\n");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      digitalWrite(OTA_LED_PIN, HIGH); // Keep LED on to indicate error
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed\r\n");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed\r\n");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed\r\n");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed\r\n");
      else if (error == OTA_END_ERROR) Serial.println("End Failed\r\n");
    });
  
  ArduinoOTA.begin();
  Serial.println("OTA Initialized\r\n");
}

// -------------------- Get Local Time --------------------
bool getLocalTimeCustom(struct tm *info, uint32_t ms){
  // Wait for time to be set
  uint32_t start = millis();
  while(!getLocalTime(info)){
    delay(100);
    if(millis() - start > ms){
      return false;
    }
  }
  return true;
}

// -------------------- Flake Functions --------------------

// Function to spawn a new flake
void spawnFlake(){
  Flake newFlake;
  
  // Decide spawn edge: 0=top, 1=bottom, 2=left, 3=right
  int edge = random(0,4);
  
  // Assign colors
  newFlake.startColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
  newFlake.endColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
  
  // Ensure endColor is different from startColor
  while(newFlake.endColor == newFlake.startColor){
    newFlake.endColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
  }
  
  newFlake.bounce = false; // Set to false unless you want flakes to bounce
  newFlake.frac = 0.0;
  
  // Initialize position and movement based on spawn edge
  switch(edge){
    case 0: // Top Edge
      newFlake.x = random(0,32);
      newFlake.y = 0;
      newFlake.dx = 0;
      newFlake.dy = 1;
      break;
    case 1: // Bottom Edge
      newFlake.x = random(0,32);
      newFlake.y = 15;
      newFlake.dx = 0;
      newFlake.dy = -1;
      break;
    case 2: // Left Edge
      newFlake.x = 0;
      newFlake.y = random(0,16);
      newFlake.dx = 1;
      newFlake.dy = 0;
      break;
    case 3: // Right Edge
      newFlake.x = 31;
      newFlake.y = random(0,16);
      newFlake.dx = -1;
      newFlake.dy = 0;
      break;
  }
  
  // Add to flakes vector
  flakes.push_back(newFlake);
  
  //Serial.printf("Spawned flake at (%d, %d) moving (%d, %d)\r\n", newFlake.x, newFlake.y, newFlake.dx, newFlake.dy);
}

// Function to perform the snow effect
void performSnowEffect(){
  // Fade all LEDs slightly to create a trailing effect
  fadeToBlackBy(leds, NUM_LEDS, fadeValue); // Adjusted to use fadeValue
  
  // Spawn new flakes based on spawn chance and max flakes
  if(random(0, 1000) < (int)(spawnRate * 1000) && flakes.size() < maxFlakes){
    spawnFlake();
  }
  
  // Update existing flakes
  for(auto it = flakes.begin(); it != flakes.end(); ){
    // Update position
    it->x += it->dx;
    it->y += it->dy;
    it->frac += 0.02;
    
    // Remove flake if out of bounds
    if(it->x < 0 || it->x >=32 || it->y < 0 || it->y >=16){
      it = flakes.erase(it);
      continue;
    }
    
    // Clamp fraction
    if(it->frac >1.0){
      it->frac =1.0;
    }
    
    // Calculate main color
    CRGB mainColor = calcColor(it->frac, it->startColor, it->endColor, it->bounce);
    mainColor.nscale8(brightness);
    
    // Set main flake color
    int ledIndex = getLedIndex(it->x, it->y);
    if(ledIndex >=0 && ledIndex < NUM_LEDS){
      leds[ledIndex] += mainColor;
    }
    
    // Set tails based on fixed tailLength
    for(int t = 1; t <= tailLength; t++){ // Fixed tailLength (e.g., 5 pixels)
      int tailX = it->x - t * it->dx;
      int tailY = it->y - t * it->dy;
      if(tailX >=0 && tailX <32 && tailY >=0 && tailY <16){
        int tailIndex = getLedIndex(tailX, tailY);
        if(tailIndex >=0 && tailIndex < NUM_LEDS){
          // Calculate brightness for the tail using linear scaling
          float scale = 1.0 - ((float)t / (float)(tailLength + 1));
          uint8_t tailBrightness = (uint8_t)(brightness * scale);
          
          // Ensure a minimum brightness for visibility
          if(tailBrightness < 10) tailBrightness = 10;
          
          CRGB tailColor = mainColor;
          tailColor.nscale8(tailBrightness);
          leds[tailIndex] += tailColor; // Use additive blending
          
         // Serial.printf("Rendered tail pixel at (%d, %d) with brightness %d\r\n", tailX, tailY, tailBrightness);
        }
      }
    }
    
    ++it;
  }
}

// Function to calculate color based on fraction and bounce
CRGB calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce){
  if(!bounce){
    return blend(startColor, endColor, frac * 255);
  }
  else{
    if(frac <= 0.5){
      return blend(startColor, endColor, frac * 2 * 255);
    }
    else{
      return blend(endColor, startColor, (frac - 0.5) * 2 * 255);
    }
  }
}

// Function to map (x,y) to LED index with rotation and panel order
int getLedIndex(int x, int y){
  // Determine which panel (0 or 1) based on x
  int panel = (x < 16) ? 0 : 1;
  
  // Determine local coordinates within the panel
  int localX = (x < 16) ? x : (x - 16);
  int localY = y;
  
  // Apply rotation based on panel
  rotateCoordinates(localX, localY, (panel == 0) ? rotationAngle1 : rotationAngle2);
  
  // Zigzag mapping: even rows left-to-right, odd rows right-to-left
  if(localY % 2 !=0){
    localX = 15 - localX;
  }
  
  // Calculate LED index based on panel order
  int ledIndex;
  if(panelOrder == 0){ // Left first
    ledIndex = panel * 256 + localY * 16 + localX;
  }
  else{ // Right first
    ledIndex = (1 - panel) * 256 + localY * 16 + localX;
  }
  
  // Ensure ledIndex is within bounds
  if(ledIndex < 0 || ledIndex >= NUM_LEDS){
    Serial.printf("Invalid LED index calculated: %d\r\n", ledIndex);
    return -1;
  }
  
  return ledIndex;
}

// Function to rotate coordinates based on angle
void rotateCoordinates(int &x, int &y, int angle){
  int tempX, tempY;
  switch(angle){
    case 0:
      // No rotation
      break;
    case 90:
      tempX = y;
      tempY = 15 - x;
      x = tempX;
      y = tempY;
      break;
    case 180:
      tempX = 15 - x;
      tempY = 15 - y;
      x = tempX;
      y = tempY;
      break;
    case 270:
      tempX = 15 - y;
      tempY = x;
      x = tempX;
      y = tempY;
      break;
    default:
      // Invalid angle, do nothing
      Serial.printf("Invalid rotation angle: %d\r\n", angle);
      break;
  }
}

// -------------------- Telnet Server Handler --------------------
void handleTelnet(){
  // Check if a new client has connected
  if (!telnetClient || !telnetClient.connected()) {
    if (telnetServer.hasClient()) {
      // If a new client connects, disconnect the previous one
      if (telnetClient) {
        telnetClient.stop();
        Serial.println("Previous Telnet client disconnected.\r\n");
      }
      telnetClient = telnetServer.available();
      telnetClient.println("Connected to ESP32 Telnet Server.\r");
      telnetClient.println("Available commands:");
      telnetClient.println("  LIST PALETTES - List all palettes with numbers and names");
      telnetClient.println("  LIST PALETTE DETAILS - List palettes with numbers, names, and color codes");
      telnetClient.println("  SET PALETTE <number> - Set color palette (1-11)");
      telnetClient.println("  GET PALETTE - Get current color palette");
      telnetClient.println("  SET BRIGHTNESS <value> - Set LED brightness (0-255)");
      telnetClient.println("  GET BRIGHTNESS - Get current LED brightness");
      telnetClient.println("  SET TAIL LENGTH <value> - Set tail length (1-30)");
      telnetClient.println("  GET TAIL LENGTH - Get current tail length");
      telnetClient.println("  SET SPAWN RATE <value> - Set spawn rate (0.0-1.0)");
      telnetClient.println("  GET SPAWN RATE - Get current spawn rate");
      telnetClient.println("  SET MAX FLAKES <value> - Set maximum number of flakes (10-500)");
      telnetClient.println("  GET MAX FLAKES - Get current maximum number of flakes");
      telnetClient.println("  SWAP PANELS - Swap the order of LED panels");
      telnetClient.println("  SET PANEL ORDER <left/right> - Set panel order");
      telnetClient.println("  ROTATE PANEL1 <0/90/180/270> - Rotate Panel 1 by specified degrees");
      telnetClient.println("  ROTATE PANEL2 <0/90/180/270> - Rotate Panel 2 by specified degrees");
      telnetClient.println("  GET ROTATION PANEL1 - Get current rotation angle of Panel 1");
      telnetClient.println("  GET ROTATION PANEL2 - Get current rotation angle of Panel 2");
      telnetClient.println("  SPEED <milliseconds> - Set LED update speed");
      telnetClient.println("  GET SPEED - Get current LED update speed");
      telnetClient.println("  HELP - Show this help message\r");
      telnetClient.print("> ");
      Serial.println("New Telnet client connected.\r\n");
      lastTelnetActivity = millis(); // Reset activity timer
    }
    return;
  }

  // Check for inactivity timeout
  unsigned long currentTime = millis();
  if (currentTime - lastTelnetActivity > TELNET_TIMEOUT) {
    telnetClient.println("Disconnected due to inactivity.\r");
    telnetClient.stop();
    Serial.println("Telnet client disconnected due to inactivity.\r\n");
    return;
  }

  // Handle incoming data from the client
  while (telnetClient.available()) {
    String input = telnetClient.readStringUntil('\n');
    input.trim(); // Remove any trailing whitespace
    Serial.printf("Telnet input: %s\r\n", input.c_str());
    
    lastTelnetActivity = millis(); // Update last activity time

    // Parse commands
    if (input.startsWith("SET PALETTE")) {
      // Example command: SET PALETTE 3
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String commandPart = input.substring(0, spaceIndex); // "SET"
        String argsPart = input.substring(spaceIndex + 1);  // "PALETTE 3"
        int secondSpace = argsPart.indexOf(' ');
        if (secondSpace != -1) {
          String command = argsPart.substring(0, secondSpace); // "PALETTE"
          String valueStr = argsPart.substring(secondSpace + 1); // "3"
          if (command.equalsIgnoreCase("PALETTE")) {
            int paletteNumber = valueStr.toInt();
            if (paletteNumber >=1 && paletteNumber <= ALL_PALETTES.size()){
              currentPalette = paletteNumber -1;
              telnetClient.printf("Palette %d (%s) selected.\r\n", paletteNumber, PALETTE_NAMES[currentPalette].c_str());
              Serial.printf("Palette %d (%s) selected.\r\n", paletteNumber, PALETTE_NAMES[currentPalette].c_str());
            }
            else {
              telnetClient.println("Invalid palette number. Enter a number between 1 and 11.\r");
            }
          }
          else {
            telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET PALETTE <number>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET PALETTE <number>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET PALETTE")) {
      telnetClient.printf("Current Palette: %d (%s)\r\n", currentPalette +1, PALETTE_NAMES[currentPalette].c_str());
    }
    else if (input.equalsIgnoreCase("LIST PALETTES")) {
      telnetClient.println("Available Palettes:\r");
      for(int i = 0; i < PALETTE_NAMES.size(); i++){
        telnetClient.printf("  %d) %s\r\n", i+1, PALETTE_NAMES[i].c_str());
      }
    }
    else if (input.equalsIgnoreCase("LIST PALETTE DETAILS")) {
      telnetClient.println("Available Palettes with Color Codes:\r");
      for(int i = 0; i < PALETTE_NAMES.size(); i++){
        telnetClient.printf("  %d) %s\r\n", i+1, PALETTE_NAMES[i].c_str());
        for(int j = 0; j < ALL_PALETTES[i].size(); j++){
          CRGB color = ALL_PALETTES[i][j];
          telnetClient.printf("      Color %d: R=%d, G=%d, B=%d\r\n", j+1, color.r, color.g, color.b);
        }
      }
    }
    else if (input.startsWith("SET BRIGHTNESS")) {
      // Example command: SET BRIGHTNESS 100
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String commandPart = input.substring(0, spaceIndex); // "SET"
        String argsPart = input.substring(spaceIndex + 1);  // "BRIGHTNESS 100"
        int secondSpace = argsPart.indexOf(' ');
        if (secondSpace != -1) {
          String command = argsPart.substring(0, secondSpace); // "BRIGHTNESS"
          String valueStr = argsPart.substring(secondSpace + 1); // "100"
          if (command.equalsIgnoreCase("BRIGHTNESS")) {
            int brightnessValue = valueStr.toInt();
            if (brightnessValue >=0 && brightnessValue <=255){
              brightness = brightnessValue;
              FastLED.setBrightness(brightness);
              telnetClient.printf("LED brightness set to %d.\r\n", brightness);
              Serial.printf("LED brightness set to %d.\r\n", brightness);
            }
            else {
              telnetClient.println("Invalid brightness value. Enter a number between 0 and 255.\r");
            }
          }
          else {
            telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET BRIGHTNESS <value>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET BRIGHTNESS <value>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET BRIGHTNESS")) {
      telnetClient.printf("Current LED Brightness: %d\r\n", brightness);
    }
    else if (input.startsWith("SET TAIL LENGTH")) {
      // Example command: SET TAIL LENGTH 15
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "TAIL LENGTH 15"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String subCommand = argsPart.substring(0, firstSpace); // "TAIL"
          String remaining = argsPart.substring(firstSpace + 1); // "LENGTH 15"
          int secondSpace = remaining.indexOf(' ');
          if (secondSpace != -1) {
            String command = remaining.substring(0, secondSpace); // "LENGTH"
            String valueStr = remaining.substring(secondSpace + 1); // "15"
            if (command.equalsIgnoreCase("LENGTH")) {
              int tailLengthInput = valueStr.toInt();
              if (tailLengthInput >=1 && tailLengthInput <=30){
                // Map tailLengthInput (1-30) to fadeValue (3000-10)
                fadeValue = map(tailLengthInput, 1, 30, 3000, 10);
                telnetClient.printf("Tail length set to %d (fade value: %d).\r\n", tailLengthInput, fadeValue);
                Serial.printf("Tail length set to %d (fade value: %d).\r\n", tailLengthInput, fadeValue);
              }
              else {
                telnetClient.println("Invalid tail length. Enter a number between 1 and 30.\r");
              }
            }
            else {
              telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
            }
          }
          else {
            telnetClient.println("Invalid command format. Use: SET TAIL LENGTH <value>\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET TAIL LENGTH <value>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET TAIL LENGTH <value>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET TAIL LENGTH")) {
      // To retrieve the current fadeValue mapped back to tailLengthInput
      // Reverse mapping: fadeValue = map(tailLengthInput, 1,30,3000,10)
      // Therefore, tailLengthInput = map(fadeValue, 3000,10,1,30)
      int tailLengthInput = map(fadeValue, 3000, 10, 1, 30);
      telnetClient.printf("Current Tail Length: %d\r\n", tailLengthInput);
    }
    else if (input.startsWith("SET SPAWN RATE")) {
      // Example command: SET SPAWN RATE 0.5
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "SPAWN RATE 0.5"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String subCommand = argsPart.substring(0, firstSpace); // "SPAWN"
          String remaining = argsPart.substring(firstSpace + 1); // "RATE 0.5"
          int secondSpace = remaining.indexOf(' ');
          if (secondSpace != -1) {
            String command = remaining.substring(0, secondSpace); // "RATE"
            String valueStr = remaining.substring(secondSpace + 1); // "0.5"
            if (command.equalsIgnoreCase("RATE")) {
              float spawnRateValue = valueStr.toFloat();
              if (spawnRateValue >=0.0 && spawnRateValue <=1.0){
                spawnRate = spawnRateValue;
                telnetClient.printf("Spawn rate set to %.2f.\r\n", spawnRate);
                Serial.printf("Spawn rate set to %.2f.\r\n", spawnRate);
              }
              else {
                telnetClient.println("Invalid spawn rate. Enter a value between 0.0 and 1.0.\r");
              }
            }
            else {
              telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
            }
          }
          else {
            telnetClient.println("Invalid command format. Use: SET SPAWN RATE <value>\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET SPAWN RATE <value>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET SPAWN RATE <value>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET SPAWN RATE")) {
      telnetClient.printf("Current Spawn Rate: %.2f\r\n", spawnRate);
    }
    else if (input.startsWith("SET MAX FLAKES")) {
      // Example command: SET MAX FLAKES 300
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "MAX FLAKES 300"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String subCommand = argsPart.substring(0, firstSpace); // "MAX"
          String remaining = argsPart.substring(firstSpace + 1); // "FLAKES 300"
          int secondSpace = remaining.indexOf(' ');
          if (secondSpace != -1) {
            String command = remaining.substring(0, secondSpace); // "FLAKES"
            String valueStr = remaining.substring(secondSpace + 1); // "300"
            if (command.equalsIgnoreCase("FLAKES")) {
              int maxFlakesValue = valueStr.toInt();
              if (maxFlakesValue >=10 && maxFlakesValue <=500){
                maxFlakes = maxFlakesValue;
                telnetClient.printf("Maximum number of flakes set to %d.\r\n", maxFlakes);
                Serial.printf("Maximum number of flakes set to %d.\r\n", maxFlakes);
              }
              else {
                telnetClient.println("Invalid max flakes. Enter a number between 10 and 500.\r");
              }
            }
            else {
              telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
            }
          }
          else {
            telnetClient.println("Invalid command format. Use: SET MAX FLAKES <value>\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET MAX FLAKES <value>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET MAX FLAKES <value>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET MAX FLAKES")) {
      telnetClient.printf("Current Maximum Number of Flakes: %d\r\n", maxFlakes);
    }
    else if (input.equalsIgnoreCase("SWAP PANELS")) {
      panelOrder = 1 - panelOrder; // Toggle between 0 and 1
      telnetClient.println("Panels swapped successfully.\r");
      Serial.println("Panels swapped successfully.\r");
    }
    else if (input.startsWith("SET PANEL ORDER")) {
      // Example command: SET PANEL ORDER left
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "PANEL ORDER left"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String subCommand = argsPart.substring(0, firstSpace); // "PANEL"
          String remaining = argsPart.substring(firstSpace + 1); // "ORDER left"
          int secondSpace = remaining.indexOf(' ');
          if (secondSpace != -1) {
            String command = remaining.substring(0, secondSpace); // "ORDER"
            String valueStr = remaining.substring(secondSpace + 1); // "left"
            if (command.equalsIgnoreCase("ORDER")) {
              if (valueStr.equalsIgnoreCase("left")) {
                panelOrder = 0;
                telnetClient.println("Panel order set to left first.\r");
                Serial.println("Panel order set to left first.\r");
              }
              else if (valueStr.equalsIgnoreCase("right")) {
                panelOrder = 1;
                telnetClient.println("Panel order set to right first.\r");
                Serial.println("Panel order set to right first.\r");
              }
              else {
                telnetClient.println("Invalid panel order. Use 'left' or 'right'.\r");
              }
            }
            else {
              telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
            }
          }
          else {
            telnetClient.println("Invalid command format. Use: SET PANEL ORDER <left/right>\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: SET PANEL ORDER <left/right>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SET PANEL ORDER <left/right>\r");
      }
    }
    else if (input.startsWith("ROTATE PANEL1")) {
      // Example command: ROTATE PANEL1 90
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "PANEL1 90"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String panelStr = argsPart.substring(0, firstSpace); // "PANEL1"
          String angleStr = argsPart.substring(firstSpace + 1); // "90"
          if (panelStr.equalsIgnoreCase("PANEL1")) {
            int angle = angleStr.toInt();
            if (angle == 0 || angle == 90 || angle == 180 || angle == 270){
              rotationAngle1 = angle;
              telnetClient.printf("Rotation angle for Panel 1 set to %d degrees.\r\n", rotationAngle1);
              Serial.printf("Rotation angle for Panel 1 set to %d degrees.\r\n", rotationAngle1);
            }
            else {
              telnetClient.println("Invalid rotation angle for Panel 1. Use 0, 90, 180, or 270 degrees.\r");
            }
          }
          else {
            telnetClient.println("Unknown panel identifier. Use PANEL1 or PANEL2.\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: ROTATE PANEL1 <0/90/180/270>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: ROTATE PANEL1 <0/90/180/270>\r");
      }
    }
    else if (input.startsWith("ROTATE PANEL2")) {
      // Example command: ROTATE PANEL2 180
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String argsPart = input.substring(spaceIndex + 1);  // "PANEL2 180"
        int firstSpace = argsPart.indexOf(' ');
        if (firstSpace != -1) {
          String panelStr = argsPart.substring(0, firstSpace); // "PANEL2"
          String angleStr = argsPart.substring(firstSpace + 1); // "180"
          if (panelStr.equalsIgnoreCase("PANEL2")) {
            int angle = angleStr.toInt();
            if (angle == 0 || angle == 90 || angle == 180 || angle == 270){
              rotationAngle2 = angle;
              telnetClient.printf("Rotation angle for Panel 2 set to %d degrees.\r\n", rotationAngle2);
              Serial.printf("Rotation angle for Panel 2 set to %d degrees.\r\n", rotationAngle2);
            }
            else {
              telnetClient.println("Invalid rotation angle for Panel 2. Use 0, 90, 180, or 270 degrees.\r");
            }
          }
          else {
            telnetClient.println("Unknown panel identifier. Use PANEL1 or PANEL2.\r");
          }
        }
        else {
          telnetClient.println("Invalid command format. Use: ROTATE PANEL2 <0/90/180/270>\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: ROTATE PANEL2 <0/90/180/270>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET ROTATION PANEL1")) {
      telnetClient.printf("Current Rotation Angle for Panel 1: %d degrees\r\n", rotationAngle1);
    }
    else if (input.equalsIgnoreCase("GET ROTATION PANEL2")) {
      telnetClient.printf("Current Rotation Angle for Panel 2: %d degrees\r\n", rotationAngle2);
    }
    else if (input.startsWith("ROTATE")) {
      telnetClient.println("Invalid ROTATE command. Use ROTATE PANEL1 <angle> or ROTATE PANEL2 <angle>.\r");
    }
    else if (input.startsWith("SPEED")) {
      // Example command: SPEED 500
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String valueStr = input.substring(spaceIndex + 1);
        valueStr.trim();
        unsigned long newSpeed = valueStr.toInt();
        if (newSpeed >= 10 && newSpeed <= 60000) { // Allowable range: 10 ms to 60 seconds
          ledUpdateInterval = newSpeed;
          telnetClient.printf("LED update speed set to %lu ms\r\n", ledUpdateInterval);
          Serial.printf("LED update speed set to %lu ms\r\n", ledUpdateInterval);
        }
        else {
          telnetClient.println("Invalid speed value. Enter a number between 10 and 60000.\r");
        }
      }
      else {
        telnetClient.println("Invalid command format. Use: SPEED <milliseconds>\r");
      }
    }
    else if (input.equalsIgnoreCase("GET SPEED")) {
      telnetClient.printf("Current LED update speed: %lu ms\r\n", ledUpdateInterval);
    }
    else if (input.equalsIgnoreCase("HELP")) {
      telnetClient.println("Available commands:\r");
      telnetClient.println("  LIST PALETTES - List all palettes with numbers and names");
      telnetClient.println("  LIST PALETTE DETAILS - List palettes with numbers, names, and color codes");
      telnetClient.println("  SET PALETTE <number> - Set color palette (1-11)");
      telnetClient.println("  GET PALETTE - Get current color palette");
      telnetClient.println("  SET BRIGHTNESS <value> - Set LED brightness (0-255)");
      telnetClient.println("  GET BRIGHTNESS - Get current LED brightness");
      telnetClient.println("  SET TAIL LENGTH <value> - Set tail length (1-30)");
      telnetClient.println("  GET TAIL LENGTH - Get current tail length");
      telnetClient.println("  SET SPAWN RATE <value> - Set spawn rate (0.0-1.0)");
      telnetClient.println("  GET SPAWN RATE - Get current spawn rate");
      telnetClient.println("  SET MAX FLAKES <value> - Set maximum number of flakes (10-500)");
      telnetClient.println("  GET MAX FLAKES - Get current maximum number of flakes");
      telnetClient.println("  SWAP PANELS - Swap the order of LED panels");
      telnetClient.println("  SET PANEL ORDER <left/right> - Set panel order");
      telnetClient.println("  ROTATE PANEL1 <0/90/180/270> - Rotate Panel 1 by specified degrees");
      telnetClient.println("  ROTATE PANEL2 <0/90/180/270> - Rotate Panel 2 by specified degrees");
      telnetClient.println("  GET ROTATION PANEL1 - Get current rotation angle of Panel 1");
      telnetClient.println("  GET ROTATION PANEL2 - Get current rotation angle of Panel 2");
      telnetClient.println("  SPEED <milliseconds> - Set LED update speed");
      telnetClient.println("  GET SPEED - Get current LED update speed");
      telnetClient.println("  HELP - Show this help message\r");
    }
    else {
      telnetClient.println("Unknown command. Type HELP for a list of commands.\r");
    }
    telnetClient.print("> ");
  }
}

// -------------------- Corrected LCD Update Function --------------------
void updateLCD(int month, int mday, int wday, int hour, int minute, int temp_f, int hum){
  // Compute day abbreviation based on wday (0 = Sun, 1 = Mon, ..., 6 = Sat)
  String day_abbrev;
  switch(wday){
    case 0: day_abbrev = "Sun"; break;
    case 1: day_abbrev = "Mon"; break;
    case 2: day_abbrev = "Tue"; break;
    case 3: day_abbrev = "Wed"; break;
    case 4: day_abbrev = "Thu"; break;
    case 5: day_abbrev = "Fri"; break;
    case 6: day_abbrev = "Sat"; break;
    default: day_abbrev = "N/A"; break;
  }

  // Determine AM/PM
  String ampm = (hour < 12) ? "AM" : "PM";

  // Convert to 12-hour format
  int hour12 = hour % 12;
  if(hour12 == 0) hour12 = 12;

  // Format: MM/DD Day HH:MMAM/PM
  // Example: 12/29 Tue 10:30PM
  String line1 = String(month) + "/" + String(mday) + " " + day_abbrev + " " + String(hour12) + ":" + (minute < 10 ? "0" : "") + String(minute) + ampm;
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  // Clear the rest of the first line by printing spaces
  if(line1.length() < 32){
    for(int i = line1.length(); i < 32; i++) {
      lcd.print(" ");
    }
  }

  // Format: T:xxF H:xx%
  String line2 = "T:" + String(temp_f) + "F H:" + String(hum) + "%";
  lcd.setCursor(0, 1);
  lcd.print(line2);
  
  // Clear the rest of the second line by printing spaces
  if(line2.length() < 32){
    for(int i = line2.length(); i < 32; i++) {
      lcd.print(" ");
    }
  }
}
