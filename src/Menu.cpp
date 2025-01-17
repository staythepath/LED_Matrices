#include "Menu.h"
#include "LEDManager.h"

extern LEDManager ledManager;

// The 8 items for the home menu
const char* Menu::homeItems[HOME_COUNT] = {
    "Start Animation", 
    "Brightness", 
    "Fade Amount", 
    "Tail Length", 
    "Spawn Rate", 
    "Max Flakes", 
    "Speed", 
    "Select Anim"
};

// --------------------------------------------------------
// Constructor
// --------------------------------------------------------
Menu::Menu()
  : selection(0),
    offset(0),
    editSettingIndex(-1),
    pendingSingleClick(false),
    lastPressTime(0),
    lastEncoderPos(0)
{
}

void Menu::begin() {
    // no special init
}

// --------------------------------------------------------
// update()
// --------------------------------------------------------
void Menu::update(RotaryEncoder &encoder) {
    encoder.update();
    int rawPos = encoder.getPosition();
    int delta = rawPos - lastEncoderPos;
    bool pressed = encoder.isButtonPressed();

    // Single/double-click detection
    if(pressed) {
        unsigned long now = millis();
        if(!pendingSingleClick) {
            // first press
            pendingSingleClick = true;
            lastPressTime = now;
        } else {
            // second press => double-click
            pendingSingleClick = false;
            // if editing => stop
            if(editSettingIndex != -1) {
                editSettingIndex = -1;
            }
        }
    }

    if(pendingSingleClick && (millis()-lastPressTime > doubleClickThreshold)) {
        pendingSingleClick = false;
        handleSingleClick(rawPos);
    }

    // If we are editing => interpret delta as "change setting"
    if(editSettingIndex != -1 && delta!=0) {
        updateEditSetting(delta);
    }
    // else interpret as "move selection up/down"
    else if(editSettingIndex==-1 && delta!=0) {
        // move selection
        selection += (delta>0?1:-1);
        if(selection<0) selection=HOME_COUNT-1;
        if(selection>=HOME_COUNT) selection=0;

        // partial scrolling
        if(selection<offset) offset=selection;
        if(selection>= offset+VISIBLE_LINES) offset= selection-(VISIBLE_LINES-1);
        if(offset<0) offset=0;
        if(offset>HOME_COUNT - VISIBLE_LINES) offset= HOME_COUNT - VISIBLE_LINES;
    }

    lastEncoderPos= rawPos;
}

// --------------------------------------------------------
// handleSingleClick()
// --------------------------------------------------------
void Menu::handleSingleClick(int rawPos) {
    if(editSettingIndex != -1) {
        // done editing
        editSettingIndex = -1;
        return;
    }
    // not editing => interpret selection
    switch(selection){
        case 0: // Start Animation
        {
            // just re-start the current anim or do logic
            ledManager.setAnimation(ledManager.getAnimation());
            Serial.println("Start Animation clicked");
            break;
        }
        case 1: // brightness
        case 2: // fade
        case 3: // tail
        case 4: // spawn
        case 5: // flakes
        case 6: // speed
        {
            editSettingIndex = selection; // now we show partial bar, remain on same screen
            break;
        }
        case 7: // "Select Anim"
        {
            Serial.println("Select Anim clicked => do your logic");
            // maybe open sub-sub-menu, or do something else
            break;
        }
    }
}

// --------------------------------------------------------
// updateEditSetting()
//   rotate => modifies the relevant setting
// --------------------------------------------------------
void Menu::updateEditSetting(int delta) {
    switch(editSettingIndex){
        case 1: // brightness 0..255
        {
            uint8_t b= ledManager.getBrightness();
            int newB= b + (delta*5);
            if(newB<0) newB=0;
            if(newB>255) newB=255;
            ledManager.setBrightness((uint8_t)newB);
            break;
        }
        case 2: // fade 0..255
        {
            uint8_t f= ledManager.getFadeAmount();
            int newF= f + (delta*5);
            if(newF<0) newF=0;
            if(newF>255) newF=255;
            ledManager.setFadeAmount((uint8_t)newF);
            break;
        }
        case 3: // tail 0..30
        {
            int t= ledManager.getTailLength();
            t += delta;
            if(t<0) t=0;
            if(t>30) t=30;
            ledManager.setTailLength(t);
            break;
        }
        case 4: // spawn 0..1
        {
            float sr= ledManager.getSpawnRate();
            sr += (delta*0.05f);
            sr= clampFloat(sr,0.0f,1.0f);
            ledManager.setSpawnRate(sr);
            break;
        }
        case 5: // flakes 0..500
        {
            int mf= ledManager.getMaxFlakes();
            mf += (delta*10);
            if(mf<0) mf=0;
            if(mf>500) mf=500;
            ledManager.setMaxFlakes(mf);
            break;
        }
        case 6: // speed 10..60000
        {
            unsigned long spd= ledManager.getUpdateSpeed();
            long step= (delta*500);
            long newSpd= (long)spd + step;
            if(newSpd<0) newSpd=0;
            unsigned long finalSpd= clampULong((unsigned long)newSpd,10,60000);
            ledManager.setUpdateSpeed(finalSpd);
            break;
        }
    }
}

// --------------------------------------------------------
// draw()
// --------------------------------------------------------
void Menu::draw(U8G2 &u8g2) {
    u8g2.clearBuffer();

    // top bar
    drawTopBar(u8g2);

    // menu list
    drawMenuList(u8g2);

    u8g2.sendBuffer();
}

// --------------------------------------------------------
// drawTopBar()
//   If editSettingIndex != -1 => partial bar with the setting name
//   else => "Home"
// --------------------------------------------------------
void Menu::drawTopBar(U8G2 &u8g2){
    if(editSettingIndex==-1) {
        // draw "Home"
        u8g2.setFont(u8g2_font_6x12_tr);
        String title="Home";
        int tw= u8g2.getStrWidth(title.c_str());
        int xx=(128 - tw)/2;
        u8g2.setCursor(xx,12);
        u8g2.print(title);
    }
    else {
        // partial bar
        String label= homeItems[editSettingIndex];
        float ratio=0.0f;
        switch(editSettingIndex){
            case 1: {
                // brightness
                uint8_t b= ledManager.getBrightness();
                ratio= (float)b/255.0f;
                break;
            }
            case 2: {
                uint8_t f= ledManager.getFadeAmount();
                ratio= (float)f/255.0f;
                break;
            }
            case 3: {
                int t= ledManager.getTailLength();
                ratio= (float)t/30.0f;
                break;
            }
            case 4: {
                float sr= ledManager.getSpawnRate();
                ratio= clampFloat(sr,0.0f,1.0f);
                break;
            }
            case 5: {
                int mf= ledManager.getMaxFlakes();
                if(mf<0) mf=0;
                if(mf>500) mf=500;
                ratio= (float)mf/500.0f;
                break;
            }
            case 6: {
                unsigned long spd= ledManager.getUpdateSpeed();
                if(spd<10) spd=10;
                if(spd>60000) spd=60000;
                ratio= (float)(spd-10)/(60000.0f-10.0f);
                // or simpler ratio= spd/60000.0f
                break;
            }
        }
        drawPartialBar(u8g2, label, ratio);
    }
}

// --------------------------------------------------------
// drawMenuList()
//   We show items offset..offset+3. The user sees 4 lines max
//   The selected line is highlighted if not in edit mode
// --------------------------------------------------------
void Menu::drawMenuList(U8G2 &u8g2){
    u8g2.setFont(u8g2_font_6x12_tr);
    for(int line=0; line<VISIBLE_LINES; line++){
        int idx= offset + line;
        if(idx>=HOME_COUNT) break;

        int drawY= 24 + line*12; 
        bool highlight= (idx==selection && editSettingIndex==-1);
        if(highlight){
            u8g2.drawBox(0, drawY-10, 128,12);
            u8g2.setDrawColor(0);
        }
        u8g2.setCursor(2, drawY);
        u8g2.print(homeItems[idx]);
        if(highlight){
            u8g2.setDrawColor(1);
        }
    }
}

// --------------------------------------------------------
// drawPartialBar()
//   We do a "clip" approach so that we partially reverse text
//   in the region [0..(ratio*128)].
// --------------------------------------------------------
void Menu::drawPartialBar(U8G2 &u8g2, const String &label, float ratio){
    if(ratio<0.0f) ratio=0.0f;
    if(ratio>1.0f) ratio=1.0f;

    int highlightWidth= (int)(ratio * 128);

    // 1) fill that region white
    if(highlightWidth>0) {
        u8g2.drawBox(0,0, highlightWidth,12);
    }

    // measure text
    u8g2.setFont(u8g2_font_6x12_tr);
    int tw= u8g2.getStrWidth(label.c_str());
    int tx= (128 - tw)/2;
    int ty=10;

    // We'll use clipping. 
    // (a) clip to left portion => black text
    if(highlightWidth>0) {
        u8g2.setDrawColor(0); // black text
        u8g2.setClipWindow(0,0, highlightWidth,12);  // left portion
        u8g2.setCursor(tx, ty);
        u8g2.print(label);
        u8g2.setClipWindow(0,0,128,64); // reset to entire
        u8g2.setDrawColor(1);          // restore white
    }

    // (b) clip to right portion => white text on black
    if(highlightWidth<128) {
        u8g2.setDrawColor(1); // white text
        u8g2.setClipWindow(highlightWidth,0,128,12);
        u8g2.setCursor(tx,ty);
        u8g2.print(label);
        u8g2.setClipWindow(0,0,128,64); // reset
    }
}

// --------------------------------------------------------
// clamp helpers
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
