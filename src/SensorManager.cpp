#include "SensorManager.h"

// Constructor
SensorManager::SensorManager(uint8_t pin, uint8_t type)
    : _dht(pin, type) {}

// Begin the sensor
void SensorManager::begin() {
    _dht.begin();
}

// Read sensor values
bool SensorManager::readSensor(float &temperature, float &humidity) {
    temperature = _dht.readTemperature();
    humidity = _dht.readHumidity();

    // Check if any reads failed
    if (isnan(temperature) || isnan(humidity)) {
        return false;
    }
    return true;
}
