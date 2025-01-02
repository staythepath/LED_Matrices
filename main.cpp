#include <WiFi.h>
#include <ArduinoOTA.h>

// Wi-Fi Credentials
const char* ssid     = "The Tardis";      // Replace with your Wi-Fi SSID
const char* password = "28242824";        // Replace with your Wi-Fi Password

// Optional: LED for OTA status (Visual indicator)
#define OTA_LED_PIN 2 // Replace with your LED pin if different

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  Serial.println("Starting Minimal OTA Sketch");

  // Initialize OTA LED (Optional)
  pinMode(OTA_LED_PIN, OUTPUT);
  digitalWrite(OTA_LED_PIN, LOW); // LED off by default

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWi-Fi connected. IP address: " + WiFi.localIP().toString());

  // Setup OTA
  ArduinoOTA.setHostname("ESP32_LEDMatrix"); // Optional: Set a unique hostname
  
  // Optional: Set OTA Password
  // ArduinoOTA.setPassword("your_password"); // Uncomment and set a password if desired

  // Define OTA event handlers
  ArduinoOTA
    .onStart([]() {
      digitalWrite(OTA_LED_PIN, HIGH); // Turn on LED during OTA
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      digitalWrite(OTA_LED_PIN, LOW); // Turn off LED after OTA
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      digitalWrite(OTA_LED_PIN, HIGH); // Keep LED on to indicate error
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA Initialized");
}

void loop() {
  ArduinoOTA.handle(); // Handle OTA updates
}
