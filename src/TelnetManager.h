// TelnetManager.h
#ifndef TELNETMANAGER_H
#define TELNETMANAGER_H

#include <WiFi.h>
#include <vector>
#include "LEDManager.h"

class TelnetManager {
public:
  TelnetManager(uint16_t port, LEDManager* ledManager);
  void begin();
  void handle();

private:
  WiFiServer _telnetServer;
  WiFiClient _telnetClient;
  unsigned long _lastTelnetActivity;
  const unsigned long TELNET_TIMEOUT = 300000; // 5 minutes in milliseconds

  LEDManager* _ledManager;
  uint16_t _port;

  void processCommand(String input);
  void listPalettes();
  void listPaletteDetails();
  void setPalette(int paletteNumber);
  void getPalette();
  void setBrightness(int value);
  void getBrightness();
  void setTailLength(int value);
  void getTailLength();
  void setSpawnRate(float value);
  void getSpawnRate();
  void setMaxFlakes(int value);
  void getMaxFlakes();
  void swapPanels();
  void setPanelOrder(String order);
  void rotatePanel(String panel, int angle);
  void getRotation(String panel);
  void setSpeed(unsigned long speed);
  void getSpeed();
  void showHelp();
  void identifyPanels(); 
};

#endif // TELNETMANAGER_H
