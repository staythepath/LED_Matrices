#include "SensorManager.h"
#include <Arduino.h>

// Constructor: Store the sensor pin as well.
SensorManager::SensorManager(uint8_t pin, uint8_t type)
    : _dht(pin, type), _sensorPin(pin), _enabled(false) {}

// Begin the sensor
void SensorManager::begin() {
    // Don't actually initialize the sensor
    _enabled = false;
    Serial.println("DHT sensor disabled");
}

// Read sensor values
bool SensorManager::readSensor(float &temperature, float &humidity) {
    if (!_enabled) {
        // Return dummy values when sensor is disabled
        temperature = 25.0;  // 25°C
        humidity = 50.0;     // 50%
        return true;
    }

    Serial.print("Reading DHT sensor on pin ");
    Serial.println(_sensorPin);
    
    temperature = _dht.readTemperature();
    Serial.printf("Raw temperature: %f\n", temperature);
    
    humidity = _dht.readHumidity();
    Serial.printf("Raw humidity: %f\n", humidity);

    // Check if any reads failed
    if (isnan(temperature) || isnan(humidity)) {
        return false;
    }
    return true;
}
