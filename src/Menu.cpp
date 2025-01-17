#include "Menu.h"

// The 5 top-level items for "Home"
const char* Menu::level0Folders[LEVEL0_COUNT] = {
  "Red",
  "Blue",
  "Green",
  "Orange",
  "Purple"
};

Menu::Menu()
  : currentLevel(0),
    waitingForSecondClick(false),
    lastPressTime(0),
    lastEncoderRaw(0)
{
    // Initialize selection & offset
    for(int lvl=0; lvl<3; lvl++){
        selection[lvl] = 0;
        offset[lvl] = 0;
        folderNames[lvl] = "";
    }
    // top-level name is "Home"
    folderNames[0] = "Home";
}

void Menu::begin(){
    // nothing special
}

int Menu::wrapIndex(int idx, int maxVal){
    if(idx<0) idx = maxVal-1;
    if(idx>=maxVal) idx = 0;
    return idx;
}

// We'll fill folderNames[1] with whichever color was chosen at level0
// and folderNames[2] with e.g. "Red 3" if subfolder=2 => #=3
void Menu::setFolderNames(){
    // level0 = "Home"
    folderNames[0] = "Home";
    // level1 => the color user selected at level0
    int colIdx = selection[0]; // 0..4
    folderNames[1] = level0Folders[colIdx]; // e.g. "Red"

    // level2 => e.g. "Red 3" if selection[1] = 2 => 3
    int subNum = selection[1]+1; // 1..7
    folderNames[2] = folderNames[1] + " " + String(subNum);
}

// single/double click detection
int Menu::checkButtonEvents(RotaryEncoder &encoder){
    if(encoder.isButtonPressed()){
        unsigned long now = millis();
        if(!waitingForSecondClick){
            waitingForSecondClick=true;
            lastPressTime= now;
            return 0;
        } else {
            if((now - lastPressTime)<= doubleClickThreshold){
                waitingForSecondClick=false;
                return 2; // double
            } else {
                waitingForSecondClick=true;
                lastPressTime= now;
                return 1; // single
            }
        }
    }

    if(waitingForSecondClick && (millis()-lastPressTime)>doubleClickThreshold){
        waitingForSecondClick=false;
        return 1; // single
    }
    return 0;
}

void Menu::update(RotaryEncoder &encoder) {
    // 1) Read raw encoder, find delta
    encoder.update();
    int raw = encoder.getPosition();
    int delta = raw - lastEncoderRaw;
    if (delta != 0) {
        lastEncoderRaw = raw;

        // figure out how many items in this level
        int maxItems;
        if (currentLevel == 0) maxItems = LEVEL0_COUNT;
        else if (currentLevel == 1) maxItems = LEVEL1_COUNT;
        else maxItems = LEVEL2_COUNT;

        // Scrolling logic: wrap selection, then clamp offset
        if (delta > 0) {
            selection[currentLevel]++;
        } else {
            selection[currentLevel]--;
        }
        // wrap
        if (selection[currentLevel] < 0) {
            selection[currentLevel] = maxItems - 1;
        }
        if (selection[currentLevel] >= maxItems) {
            selection[currentLevel] = 0;
        }

        // Keep selection visible in [offset..offset+3]
        if (selection[currentLevel] < offset[currentLevel]) {
            offset[currentLevel] = selection[currentLevel];
        }
        if (selection[currentLevel] >= offset[currentLevel] + VISIBLE_LINES) {
            offset[currentLevel] = selection[currentLevel] - (VISIBLE_LINES - 1);
        }

        // If maxItems < 4 => offset=0 always
        if (maxItems < VISIBLE_LINES) {
            offset[currentLevel] = 0;
        } else {
            // clamp offset in [0..maxItems - VISIBLE_LINES]
            if (offset[currentLevel] < 0) offset[currentLevel] = 0;
            if (offset[currentLevel] > (maxItems - VISIBLE_LINES)) {
                offset[currentLevel] = maxItems - VISIBLE_LINES;
            }
        }
    }

    // 2) Check single/double-click
    int ev = checkButtonEvents(encoder);
    if (ev == 1) {
        // single => go deeper if currentLevel < 2
        if (currentLevel < 2) {
            currentLevel++;

            // Start new level at item=0
            selection[currentLevel] = 0;
            offset[currentLevel]    = 0;

            // CRUCIAL fix => reset lastEncoderRaw to raw
            // so no immediate "jump" from leftover delta.
            lastEncoderRaw = raw;

            // If you update folder names or do other init:
            setFolderNames();
        }
    } 
    else if (ev == 2) {
        // double => go back if >0
        if (currentLevel > 0) {
            currentLevel--;

            // Also reset so we don't jump
            lastEncoderRaw = raw;
        }
    }
}

void Menu::draw(U8G2 &u8g2){
    u8g2.clearBuffer();

    // figure out how many items for this level
    int maxItems;
    if(currentLevel==0) maxItems= LEVEL0_COUNT;
    else if(currentLevel==1) maxItems= LEVEL1_COUNT;
    else maxItems= LEVEL2_COUNT;

    // topName:
    if(currentLevel==0){
        folderNames[0] = "Home";
    }
    else if(currentLevel==1){
        int idx0 = selection[0];
        folderNames[1] = level0Folders[idx0];
    }
    else {
        // e.g. "Red 3"
        int idx0 = selection[0];
        const char* color = level0Folders[idx0];
        int subNum = selection[1]+1; // 1..7
        folderNames[2] = String(color)+" "+String(subNum);
    }

    String topName = folderNames[currentLevel];
    u8g2.setFont(u8g2_font_6x12_tr);
    int tw = u8g2.getStrWidth(topName.c_str());
    int x = (128 - tw)/2;
    int y = 12;
    u8g2.setCursor(x,y);
    u8g2.print(topName);

    // draw 4 lines from offset..offset+3
    for(int line=0; line<VISIBLE_LINES; line++){
        int idx = offset[currentLevel] + line;
        // check bounds
        if(idx>= maxItems) break; // no more

        // build line name
        String lineName;
        if(currentLevel==0){
            // from level0Folders
            lineName= level0Folders[idx];
        } else if(currentLevel==1){
            // e.g. "Red 2" if idx=1 => "Red 2"
            const char* color = level0Folders[ selection[0] ];
            int sub= idx+1;
            lineName= String(color)+" "+String(sub);
        } else {
            // e.g. "Red 3 A"
            const char* color = level0Folders[ selection[0] ];
            int subNum= selection[1]+1;
            char c= 'A'+ idx;
            lineName= String(color)+" "+String(subNum)+" "+c;
        }

        int drawY= 24+ line*12;
        if(idx== selection[currentLevel]){
            // highlight
            u8g2.drawBox(0, drawY-10, 128,12);
            u8g2.setDrawColor(0);
            u8g2.setCursor(2, drawY);
            u8g2.print(lineName);
            u8g2.setDrawColor(1);
        } else {
            u8g2.setCursor(2, drawY);
            u8g2.print(lineName);
        }
    }

    u8g2.sendBuffer();
}
