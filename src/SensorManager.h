// SensorManager.h
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
};

#endif // SENSORMANAGER_H
