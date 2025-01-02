// TelnetManager.cpp
#include "TelnetManager.h"

TelnetManager::TelnetManager(uint16_t port, LEDManager* ledManager)
    : _telnetServer(port), _ledManager(ledManager), _port(port) {}

void TelnetManager::begin(){
    _telnetServer.begin();
    _telnetServer.setNoDelay(true);
    Serial.println("Telnet server started on port " + String(_port));
}

void TelnetManager::handle(){
    if (!_telnetClient || !_telnetClient.connected()) {
        if (_telnetServer.hasClient()) {
            if (_telnetClient) {
                _telnetClient.stop();
                Serial.println("Previous Telnet client disconnected.");
            }
            _telnetClient = _telnetServer.available();
            _telnetClient.println("Connected to ESP32 Telnet Server.");
            _telnetClient.println("Type HELP for a list of commands.");
            _telnetClient.print("> ");
            Serial.println("New Telnet client connected.");
        }
        return;
    }

    while (_telnetClient.available()) {
        String input = _telnetClient.readStringUntil('\n');
        input.trim();
        processCommand(input);
        _telnetClient.print("> ");
    }
}

void TelnetManager::processCommand(String input){
    input.trim();
    String command = input;
    command.toUpperCase(); // Make command case-insensitive

    if(command.startsWith("LIST PALETTES")){
        listPalettes();
    }
    else if(command.startsWith("LIST PALETTE DETAILS")){
        listPaletteDetails();
    }
    else if(command.startsWith("SET PALETTE")){
        // Extract palette number
        int paletteNumber = input.substring(strlen("SET PALETTE")).toInt();
        setPalette(paletteNumber);
    }
    else if(command.startsWith("GET PALETTE")){
        getPalette();
    }
    else if(command.startsWith("SET BRIGHTNESS")){
        int value = input.substring(strlen("SET BRIGHTNESS")).toInt();
        setBrightness(value);
    }
    else if(command.startsWith("GET BRIGHTNESS")){
        getBrightness();
    }
    else if(command.startsWith("SET TAIL LENGTH")){
        int value = input.substring(strlen("SET TAIL LENGTH")).toInt();
        setTailLength(value);
    }
    else if(command.startsWith("GET TAIL LENGTH")){
        getTailLength();
    }
    else if(command.startsWith("SET SPAWN RATE")){
        float rate = input.substring(strlen("SET SPAWN RATE")).toFloat();
        setSpawnRate(rate);
    }
    else if(command.startsWith("GET SPAWN RATE")){
        getSpawnRate();
    }
    else if(command.startsWith("SET MAX FLAKES")){
        int max = input.substring(strlen("SET MAX FLAKES")).toInt();
        setMaxFlakes(max);
    }
    else if(command.startsWith("GET MAX FLAKES")){
        getMaxFlakes();
    }
    else if(command.startsWith("SWAP PANELS")){
        swapPanels();
    }
    else if(command.startsWith("SET PANEL ORDER")){
        String order = input.substring(strlen("SET PANEL ORDER"));
        order.trim();
        setPanelOrder(order);
    }
    else if(command.startsWith("ROTATE PANEL1")){
        int angle = input.substring(strlen("ROTATE PANEL1")).toInt();
        rotatePanel("PANEL1", angle);
    }
    else if(command.startsWith("ROTATE PANEL2")){
        int angle = input.substring(strlen("ROTATE PANEL2")).toInt();
        rotatePanel("PANEL2", angle);
    }
    else if(command.startsWith("GET ROTATION PANEL1")){
        getRotation("PANEL1");
    }
    else if(command.startsWith("GET ROTATION PANEL2")){
        getRotation("PANEL2");
    }
    else if(command.startsWith("SPEED")){
        unsigned long speed = input.substring(strlen("SPEED")).toInt();
        setSpeed(speed);
    }
    else if(command.startsWith("GET SPEED")){
        getSpeed();
    }
    else if(command.startsWith("HELP")){
        showHelp();
    }
    else{
        _telnetClient.println("Unknown command. Type HELP for a list of commands.");
    }
}

void TelnetManager::listPalettes(){
    _telnetClient.println("Available Palettes:");
    for (size_t i = 0; i < _ledManager->getPaletteCount(); ++i) {
        _telnetClient.printf("  %zu: %s\n", i + 1, _ledManager->getPaletteNameAt(i).c_str());
    }
}

void TelnetManager::listPaletteDetails(){
    _telnetClient.println("Palette Details:");
    for (size_t i = 0; i < _ledManager->getPaletteCount(); ++i) {
        _telnetClient.printf("  %zu: %s\n", i + 1, _ledManager->getPaletteNameAt(i).c_str());
        // Add color details if available
    }
}

void TelnetManager::setPalette(int paletteNumber){
    if(paletteNumber >=1 && paletteNumber <= _ledManager->getPaletteCount()){
        _ledManager->setPalette(paletteNumber);
        _telnetClient.printf("Palette %d (%s) selected.\n", paletteNumber, _ledManager->getPaletteNameAt(paletteNumber -1).c_str());
        Serial.printf("Palette %d (%s) selected.\n", paletteNumber, _ledManager->getPaletteNameAt(paletteNumber -1).c_str());
    }
    else{
        _telnetClient.println("Invalid palette number. Enter a number between 1 and " + String(_ledManager->getPaletteCount()) + ".");
    }
}

void TelnetManager::getPalette(){
    int current = _ledManager->getCurrentPalette() + 1;
    String name = _ledManager->getPaletteNameAt(_ledManager->getCurrentPalette());
    _telnetClient.printf("Current Palette: %d (%s)\n", current, name.c_str());
}

void TelnetManager::setBrightness(int value){
    if(value >=0 && value <=255){
        _ledManager->setBrightness((uint8_t)value);
        _telnetClient.printf("Brightness set to %d.\n", value);
        Serial.printf("Brightness set to %d.\n", value);
    }
    else{
        _telnetClient.println("Invalid brightness value. Enter a number between 0 and 255.");
    }
}

void TelnetManager::getBrightness(){
    uint8_t brightness = _ledManager->getBrightness();
    _telnetClient.printf("Current Brightness: %d\n", brightness);
}

void TelnetManager::setTailLength(int value){
    if(value >=1 && value <=30){
        _ledManager->setFadeAmount(value);
        _telnetClient.printf("Tail length set to %d.\n", value);
        Serial.printf("Tail length set to %d.\n", value);
    }
    else{
        _telnetClient.println("Invalid tail length. Enter a number between 1 and 30.");
    }
}

void TelnetManager::getTailLength(){
    int fade = _ledManager->getFadeAmount();
    _telnetClient.printf("Current Tail Length: %d\n", fade);
}

void TelnetManager::setSpawnRate(float rate){
    if(rate >=0.0 && rate <=1.0){
        _ledManager->setSpawnRate(rate);
        _telnetClient.printf("Spawn rate set to %.2f.\n", rate);
        Serial.printf("Spawn rate set to %.2f.\n", rate);
    }
    else{
        _telnetClient.println("Invalid spawn rate. Enter a value between 0.0 and 1.0.");
    }
}

void TelnetManager::getSpawnRate(){
    float rate = _ledManager->getSpawnRate();
    _telnetClient.printf("Current Spawn Rate: %.2f\n", rate);
}

void TelnetManager::setMaxFlakes(int max){
    if(max >=10 && max <=500){
        _ledManager->setMaxFlakes(max);
        _telnetClient.printf("Maximum flakes set to %d.\n", max);
        Serial.printf("Maximum flakes set to %d.\n", max);
    }
    else{
        _telnetClient.println("Invalid max flakes. Enter a number between 10 and 500.");
    }
}

void TelnetManager::getMaxFlakes(){
    int max = _ledManager->getMaxFlakes();
    _telnetClient.printf("Current Maximum Flakes: %d\n", max);
}

void TelnetManager::swapPanels(){
    _ledManager->swapPanels();
    _telnetClient.println("Panels swapped successfully.");
    Serial.println("Panels swapped successfully.");
}

void TelnetManager::setPanelOrder(String order){
    if(order.equalsIgnoreCase("left")){
        _ledManager->setPanelOrder("left");
        _telnetClient.println("Panel order set to left first.");
        Serial.println("Panel order set to left first.");
    }
    else if(order.equalsIgnoreCase("right")){
        _ledManager->setPanelOrder("right");
        _telnetClient.println("Panel order set to right first.");
        Serial.println("Panel order set to right first.");
    }
    else{
        _telnetClient.println("Invalid panel order. Use 'left' or 'right'.");
    }
}

void TelnetManager::rotatePanel(String panel, int angle){
    if(panel.equalsIgnoreCase("PANEL1") || panel.equalsIgnoreCase("PANEL2")){
        if(angle ==0 || angle ==90 || angle ==180 || angle ==270){
            _ledManager->rotatePanel(panel, angle);
            _telnetClient.printf("Rotation angle for %s set to %d degrees.\n", panel.c_str(), angle);
            Serial.printf("Rotation angle for %s set to %d degrees.\n", panel.c_str(), angle);
        }
        else{
            _telnetClient.printf("Invalid rotation angle for %s. Use 0, 90, 180, or 270 degrees.\n", panel.c_str());
        }
    }
    else{
        _telnetClient.println("Unknown panel identifier. Use 'PANEL1' or 'PANEL2'.");
    }
}

void TelnetManager::getRotation(String panel){
    if(panel.equalsIgnoreCase("PANEL1")){
        int angle = _ledManager->getRotation(panel);
        _telnetClient.printf("Current Rotation Angle for Panel 1: %d degrees\n", angle);
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        int angle = _ledManager->getRotation("PANEL2");
        _telnetClient.printf("Current Rotation Angle for Panel 2: %d degrees\n", angle);
    }
    else{
        _telnetClient.println("Unknown panel identifier. Use 'PANEL1' or 'PANEL2'.");
    }
}



void TelnetManager::setSpeed(unsigned long speed){
    if(speed >=10 && speed <=60000){
        _ledManager->setUpdateSpeed(speed);
        _telnetClient.printf("LED update speed set to %lu ms.\n", speed);
        Serial.printf("LED update speed set to %lu ms.\n", speed);
    }
    else{
        _telnetClient.println("Invalid speed value. Enter a number between 10 and 60000.");
    }
}

void TelnetManager::getSpeed(){
    unsigned long speed = _ledManager->getUpdateSpeed();
    _telnetClient.printf("Current LED update speed: %lu ms\n", speed);
}

void TelnetManager::showHelp(){
    _telnetClient.println("Available commands:");
    _telnetClient.println("  LIST PALETTES - List all palettes");
    _telnetClient.println("  LIST PALETTE DETAILS - List palette details");
    _telnetClient.println("  SET PALETTE <number> - Set palette");
    _telnetClient.println("  GET PALETTE - Get current palette");
    _telnetClient.println("  SET BRIGHTNESS <value> - Set brightness");
    _telnetClient.println("  GET BRIGHTNESS - Get brightness");
    _telnetClient.println("  SET TAIL LENGTH <value> - Set tail length");
    _telnetClient.println("  GET TAIL LENGTH - Get tail length");
    _telnetClient.println("  SET SPAWN RATE <value> - Set spawn rate");
    _telnetClient.println("  GET SPAWN RATE - Get spawn rate");
    _telnetClient.println("  SET MAX FLAKES <value> - Set max flakes");
    _telnetClient.println("  GET MAX FLAKES - Get max flakes");
    _telnetClient.println("  SWAP PANELS - Swap panels");
    _telnetClient.println("  SET PANEL ORDER <left/right> - Set panel order");
    _telnetClient.println("  ROTATE PANEL1 <0/90/180/270> - Rotate Panel1");
    _telnetClient.println("  ROTATE PANEL2 <0/90/180/270> - Rotate Panel2");
    _telnetClient.println("  GET ROTATION PANEL1 - Get Panel1 rotation");
    _telnetClient.println("  GET ROTATION PANEL2 - Get Panel2 rotation");
    _telnetClient.println("  SPEED <ms> - Set LED update speed");
    _telnetClient.println("  GET SPEED - Get LED update speed");
    _telnetClient.println("  HELP - Show this help message");
}
