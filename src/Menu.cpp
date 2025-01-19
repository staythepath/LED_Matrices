#include "Menu.h"
#include "LEDManager.h"

extern LEDManager ledManager; // We'll call getBrightness(), etc.

// We now have 7 items total:
const char* Menu::homeItems[HOME_COUNT] = {
    "Select Animation", // index=0
    "Brightness",       // index=1
    "Param1",           // index=2 (was Fade Amount)
    "Param2",           // index=3 (was Tail Length)
    "Param3",           // index=4 (was Spawn Rate)
    "Param4",           // index=5 (was Max Flakes)
    "Speed"             // index=6
};

// Animations (same 3 as before)
const char* Menu::animNames[ANIM_COUNT] = {
    "Traffic",
    "Blink",
    "RainbowWave"
};

// --------------------------------------------------------
// Constructor
// --------------------------------------------------------
Menu::Menu()
  : currentMode(MODE_HOME),
    selection(0),
    offset(0),
    editSettingIndex(-1),
    pendingSingleClick(false),
    lastPressTime(0),
    lastEncoderPos(0),
    animSelection(0),
    animOffset(0)
{
}

void Menu::begin(){
    // no special init
}

// --------------------------------------------------------
// update()
// --------------------------------------------------------
void Menu::update(RotaryEncoder &encoder) {
    encoder.update();
    int rawPos = encoder.getPosition();
    int delta  = rawPos - lastEncoderPos;
    bool pressed = encoder.isButtonPressed();

    // 1) single/double click detection
    if(pressed) {
        unsigned long now = millis();
        if(!pendingSingleClick) {
            // first press
            pendingSingleClick = true;
            lastPressTime = now;
        } else {
            // second press => double-click
            pendingSingleClick = false;
            // If in MODE_EDIT => exit edit mode
            if(currentMode == MODE_EDIT) {
                currentMode = MODE_HOME;
                editSettingIndex = -1;
            }
            // If in MODE_SELECT_ANIM => return to home
            else if(currentMode == MODE_SELECT_ANIM) {
                currentMode = MODE_HOME;
            }
        }
    }
    // If enough time passed => finalize single-click
    if(pendingSingleClick && (millis() - lastPressTime > doubleClickThreshold)) {
        pendingSingleClick = false;
        handleSingleClick(rawPos);
    }

    // 2) interpret encoder delta by mode
    switch(currentMode) {
        case MODE_HOME:
        {
            // if not editing => move selection
            if(editSettingIndex == -1 && delta != 0) {
                updateHomeSelection(delta);
            }
            // if editing => update that setting
            else if(editSettingIndex != -1 && delta != 0) {
                updateEditSetting(delta);
            }
            break;
        }
        case MODE_EDIT:
        {
            // We typically handle editing in MODE_HOME as well,
            // but if you want a separate mode, do so here:
            if(delta != 0) {
                updateEditSetting(delta);
            }
            break;
        }
        case MODE_SELECT_ANIM:
        {
            if(delta != 0) {
                updateSelectAnim(delta);
            }
            break;
        }
    }

    lastEncoderPos = rawPos;
}

// --------------------------------------------------------
// handleSingleClick()
//   Called after we confirm no double-click arrived
// --------------------------------------------------------
void Menu::handleSingleClick(int rawPos) {
    switch(currentMode) {
        case MODE_HOME:
        {
            // if we were editing => a single-click ends editing
            if(editSettingIndex != -1) {
                editSettingIndex = -1;
                return;
            }
            // else interpret the selection
            int idx = selection;
            switch(idx) {
                case 0: // "Select Animation"
                    currentMode = MODE_SELECT_ANIM;
                    animSelection = 0;
                    animOffset = 0;
                    break;

                case 1: // Brightness
                case 2: // Param1 (was fade)
                case 3: // Param2 (was tail)
                case 4: // Param3 (was spawn)
                case 5: // Param4 (was flakes)
                case 6: // Speed
                    // Enter "edit mode" on the same screen
                    currentMode = MODE_HOME; 
                    editSettingIndex = idx;
                    break;
            }
            break;
        }
        case MODE_EDIT:
        {
            // single-click => done editing
            currentMode = MODE_HOME;
            editSettingIndex = -1;
            break;
        }
        case MODE_SELECT_ANIM:
        {
            // user picks an animation from animSelection
            int aIdx = animSelection;
            if(aIdx >= 0 && aIdx < ANIM_COUNT) {
                ledManager.setAnimation(aIdx);
                Serial.printf("Selected anim %s\n", animNames[aIdx]);
            }
            currentMode = MODE_HOME;
            break;
        }
    }
}

// --------------------------------------------------------
// updateHomeSelection()
//   partial scrolling for the 7 items
// --------------------------------------------------------
void Menu::updateHomeSelection(int delta) {
    selection += (delta > 0 ? 1 : -1);
    if(selection < 0) selection = HOME_COUNT - 1;
    if(selection >= HOME_COUNT) selection = 0;

    // partial scrolling
    if(selection < offset) {
        offset = selection;
    }
    if(selection >= offset + VISIBLE_LINES) {
        offset = selection - (VISIBLE_LINES - 1);
    }

    if(offset < 0) offset = 0;
    if(offset > HOME_COUNT - VISIBLE_LINES) {
        offset = HOME_COUNT - VISIBLE_LINES;
    }
}

// --------------------------------------------------------
// updateEditSetting()
//   We are adjusting item #1..6 in real time
//   #2 => Param1 => setFadeAmount
//   #3 => Param2 => setTailLength
//   #4 => Param3 => setSpawnRate
//   #5 => Param4 => setMaxFlakes
//   #6 => Speed => 0..100 => map to setUpdateSpeed
//   with more granularity for 50..100
// --------------------------------------------------------
void Menu::updateEditSetting(int delta) {
    switch(editSettingIndex) {
        case 1: // brightness 0..255
        {
            uint8_t val = ledManager.getBrightness();
            int newVal = val + (delta * 5);
            if(newVal < 0)   newVal = 0;
            if(newVal > 255) newVal = 255;
            ledManager.setBrightness((uint8_t)newVal);
            break;
        }
        case 2: // Param1 => was "Fade Amount" 0..255
        {
            uint8_t f = ledManager.getFadeAmount();
            int newF = f + (delta * 5);
            if(newF < 0)   newF = 0;
            if(newF > 255) newF = 255;
            ledManager.setFadeAmount((uint8_t)newF);
            break;
        }
        case 3: // Param2 => was tailLength 0..30
        {
            int t = ledManager.getTailLength();
            t += delta;
            if(t < 0)  t = 0;
            if(t > 30) t = 30;
            ledManager.setTailLength(t);
            break;
        }
        case 4: // Param3 => was spawnRate 0..1
        {
            float sr = ledManager.getSpawnRate();
            sr += (delta * 0.05f);
            sr = clampFloat(sr, 0.0f, 1.0f);
            ledManager.setSpawnRate(sr);
            break;
        }
        case 5: // Param4 => was maxFlakes 0..500
        {
            int mf = ledManager.getMaxFlakes();
            mf += (delta * 10);
            if(mf < 0)   mf = 0;
            if(mf > 500) mf = 500;
            ledManager.setMaxFlakes(mf);
            break;
        }
        case 6: // Speed => want more granularity 50..100, less 0..50
        {
            // We'll read ledManager.getSpeed() in [0..100]
            int speedVal = ledManager.getSpeed();

            // If speedVal >= 50 => smaller steps => e.g. delta*1
            // If speedVal < 50 => bigger steps => e.g. delta*5
            int step = (speedVal >= 50) ? 1 : 5;

            int newSpeed = speedVal + (delta * step);
            if(newSpeed < 0)   newSpeed = 0;
            if(newSpeed > 100) newSpeed = 100;
            ledManager.setSpeed(newSpeed); 
            break;
        }
    }
}

// --------------------------------------------------------
// updateSelectAnim()
//   partial scrolling if we had more than 4 animations
// --------------------------------------------------------
void Menu::updateSelectAnim(int delta) {
    animSelection += (delta > 0 ? 1 : -1);
    if(animSelection < 0) animSelection = ANIM_COUNT - 1;
    if(animSelection >= ANIM_COUNT) animSelection = 0;

    if(animSelection < animOffset) {
        animOffset = animSelection;
    }
    if(animSelection >= animOffset + VISIBLE_LINES) {
        animOffset = animSelection - (VISIBLE_LINES - 1);
    }

    if(animOffset < 0) animOffset = 0;
    if(animOffset > ANIM_COUNT - VISIBLE_LINES) {
        animOffset = ANIM_COUNT - VISIBLE_LINES;
    }
    if(animOffset < 0) animOffset = 0; // if ANIM_COUNT < 4
}

// --------------------------------------------------------
// draw()
// --------------------------------------------------------
void Menu::draw(U8G2 &u8g2) {
    u8g2.clearBuffer();

    if(currentMode == MODE_SELECT_ANIM) {
        drawSelectAnim(u8g2);
    }
    else {
        // either MODE_HOME or editing a setting on home
        // 1) draw top bar
        if(editSettingIndex != -1) {
            // we show partial bar
            drawEditBar(u8g2);
        } else {
            // show "Home"
            u8g2.setFont(u8g2_font_6x12_tr);
            String title = "Home";
            int tw = u8g2.getStrWidth(title.c_str());
            int xx = (128 - tw)/2;
            u8g2.setCursor(xx, 12);
            u8g2.print(title);
        }
        // 2) draw the home menu items
        drawHomeList(u8g2);
    }

    u8g2.sendBuffer();
}

// --------------------------------------------------------
// drawHomeList()
//   Show items offset..offset+3
//   If item is #1..6 => show "Name      XX%"
// --------------------------------------------------------
void Menu::drawHomeList(U8G2 &u8g2){
    u8g2.setFont(u8g2_font_6x12_tr);
    for(int line=0; line<VISIBLE_LINES; line++){
        int idx = offset + line;
        if(idx >= HOME_COUNT) break;

        // Build the line
        String lineStr;
        // items #1..6 => show name + percentage
        if(idx >= 1 && idx <= 6) {
            String name = homeItems[idx];
            float ratio = 0.0f;
            switch(idx){
                case 1: // Brightness
                {
                    uint8_t b = ledManager.getBrightness();
                    ratio = (float)b / 255.0f;
                    break;
                }
                case 2: // Param1 => was fade
                {
                    uint8_t f = ledManager.getFadeAmount();
                    ratio = (float)f / 255.0f;
                    break;
                }
                case 3: // Param2 => was tail
                {
                    int t = ledManager.getTailLength();
                    ratio = (float)t / 30.0f;
                    break;
                }
                case 4: // Param3 => was spawn
                {
                    float sr = ledManager.getSpawnRate();
                    ratio = clampFloat(sr, 0.0f, 1.0f);
                    break;
                }
                case 5: // Param4 => was maxFlakes
                {
                    int mf = ledManager.getMaxFlakes();
                    ratio = (float)mf / 500.0f;
                    break;
                }
                case 6: // Speed 0..100 => map to ratio
                {
                    int sVal = ledManager.getSpeed(); // in [0..100]
                    ratio = (float)sVal / 100.0f;
                    break;
                }
            }
            int pct = (int)(ratio * 100.0f + 0.5f);
            lineStr = name + "    " + String(pct) + "%";
        }
        else {
            // item #0 => "Select Animation" or anything else
            lineStr = homeItems[idx];
        }

        int drawY = 24 + line*12;
        bool highlight = (idx == selection && editSettingIndex == -1);
        if(highlight){
            u8g2.drawBox(0, drawY - 10, 128, 12);
            u8g2.setDrawColor(0);
        }
        u8g2.setCursor(2, drawY);
        u8g2.print(lineStr);
        if(highlight){
            u8g2.setDrawColor(1);
        }
    }
}

// --------------------------------------------------------
// drawSelectAnim()
//   We have 3 animations, partial scrolling if needed
// --------------------------------------------------------
void Menu::drawSelectAnim(U8G2 &u8g2){
    u8g2.setFont(u8g2_font_6x12_tr);
    String title = "Select Animation";
    int tw = u8g2.getStrWidth(title.c_str());
    int xx = (128 - tw)/2;
    u8g2.setCursor(xx, 12);
    u8g2.print(title);

    for(int line = 0; line < VISIBLE_LINES; line++){
        int idx = animOffset + line;
        if(idx >= ANIM_COUNT) break;
        int drawY = 24 + line*12;
        bool hl = (idx == animSelection);
        if(hl){
            u8g2.drawBox(0, drawY - 10, 128, 12);
            u8g2.setDrawColor(0);
        }
        u8g2.setCursor(2, drawY);
        u8g2.print(animNames[idx]);
        if(hl) u8g2.setDrawColor(1);
    }
}

// --------------------------------------------------------
// drawEditBar()
//   partial highlight bar with reversed text for the setting
// --------------------------------------------------------
void Menu::drawEditBar(U8G2 &u8g2){
    String label = homeItems[editSettingIndex];
    float ratio  = 0.0f;
    switch(editSettingIndex){
        case 1: { // brightness
            uint8_t b = ledManager.getBrightness();
            ratio = (float)b / 255.0f;
            break;
        }
        case 2: { // Param1 => fade
            uint8_t f = ledManager.getFadeAmount();
            ratio = (float)f / 255.0f;
            break;
        }
        case 3: { // Param2 => tail
            int t = ledManager.getTailLength();
            ratio = (float)t / 30.0f;
            break;
        }
        case 4: { // Param3 => spawn
            float sr = ledManager.getSpawnRate();
            ratio = clampFloat(sr, 0.0f, 1.0f);
            break;
        }
        case 5: { // Param4 => maxFlakes
            int mf = ledManager.getMaxFlakes();
            if(mf<0) mf=0;
            if(mf>500) mf=500;
            ratio = (float)mf / 500.0f;
            break;
        }
        case 6: { // speed => 0..100
            int sVal = ledManager.getSpeed();
            ratio = (float)sVal / 100.0f;
            break;
        }
    }

    drawPartialBar(u8g2, label, ratio);
}

// --------------------------------------------------------
// drawPartialBar()
//   We do a clip-based approach so the text is reversed in [0..(ratio*128)]
// --------------------------------------------------------
void Menu::drawPartialBar(U8G2 &u8g2, const String &label, float ratio){
    if(ratio<0.0f) ratio=0.0f;
    if(ratio>1.0f) ratio=1.0f;

    int highlightWidth= (int)(ratio * 128);

    // fill left portion with white
    if(highlightWidth>0) {
        u8g2.drawBox(0, 0, highlightWidth, 12);
    }

    u8g2.setFont(u8g2_font_6x12_tr);
    int tw = u8g2.getStrWidth(label.c_str());
    int tx = (128 - tw)/2;
    int baseY = 10;

    // left portion => black text
    if(highlightWidth>0) {
        u8g2.setDrawColor(0);
        u8g2.setClipWindow(0, 0, highlightWidth, 12);
        u8g2.setCursor(tx, baseY);
        u8g2.print(label);
        // reset
        u8g2.setClipWindow(0,0,128,64);
        u8g2.setDrawColor(1);
    }

    // right portion => white text
    if(highlightWidth < 128) {
        u8g2.setDrawColor(1);
        u8g2.setClipWindow(highlightWidth,0,128,12);
        u8g2.setCursor(tx, baseY);
        u8g2.print(label);
        u8g2.setClipWindow(0,0,128,64);
    }
}

// --------------------------------------------------------
// clampFloat, clampULong
// --------------------------------------------------------
float Menu::clampFloat(float x, float mn, float mx){
    if(x<mn) x=mn;
    if(x>mx) x=mx;
    return x;
}
unsigned long Menu::clampULong(unsigned long x, unsigned long mn, unsigned long mx){
    if(x<mn) x=mn;
    if(x>mx) x=mx;
    return x;
}
