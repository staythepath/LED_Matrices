#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <DHT.h>

class SensorManager {
public:
    // Constructor
    SensorManager(uint8_t pin, uint8_t type);
    
    // Initialize the sensor
    void begin();
    
    // Read temperature and humidity
    bool readSensor(float &temperature, float &humidity);

private:
    DHT _dht; // DHT sensor instance
    uint8_t _sensorPin;  // NEW: store the sensor pin number
    bool _enabled;       // Flag to enable/disable actual sensor readings
};

#endif // SENSORMANAGER_H
