#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RotaryEncoder.h"

/*
   Single-screen menu with 8 items:
     0) "Start Animation"
     1) "Brightness"  
     2) "Fade Amount" 
     3) "Tail Length"
     4) "Spawn Rate"  
     5) "Max Flakes"  
     6) "Speed"       
     7) "Select Animation"

   - We only have space for 4 lines visible at once (after the top 12px bar).
   - We keep "offset" so if 'selection' is 5, for example, we scroll down to show items 4..7.

   - If user single-clicks item #0 => calls setAnimation(...) or something.
   - If user single-clicks item #1..6 => we do "edit mode" on that setting, but remain on the same screen:
       * The top bar changes from "Home" to a partial highlight that shows the setting name.
       * The partial highlight is the portion from x=0..(ratio*128).
       * The text is reversed exactly in that portion (clip-based approach).
       * Rotating the encoder changes that setting in real time.

   - Single-click or double-click while editing => exit edit mode, top bar reverts to "Home".

   - If user single-clicks item #7 => "Select Animation" => do your logic or open another sub-menu if you want.

   - Partial Scrolling:
       * We display 4 lines from offset..(offset+3).
       * If selection < offset => offset=selection
       * If selection >= offset+4 => offset=selection - 3
*/

class Menu {
public:
    Menu();

    // Called once in setup()
    void begin();

    // Called repeatedly in loop() with the encoder
    void update(RotaryEncoder &encoder);

    // Draw the entire menu on the U8G2
    void draw(U8G2 &u8g2);

private:
    // 8 items
    static const int HOME_COUNT = 8;
    static const char* homeItems[HOME_COUNT];

    // We can show 4 lines at once
    static const int VISIBLE_LINES = 4;

    // current 'selection' in [0..7]
    int selection;

    // offset so we only show items from offset..(offset+3)
    int offset;

    // If editSettingIndex != -1 => we are editing that setting
    // (1=Brightness,2=Fade,3=Tail,4=Spawn,5=Flakes,6=Speed)
    int editSettingIndex;

    // single/double-click
    bool pendingSingleClick;
    unsigned long lastPressTime;
    static const unsigned long doubleClickThreshold = 400;
    int lastEncoderPos;

    // Finalize single-click
    void handleSingleClick(int rawPos);

    // If we are editing => update that setting in real time
    void updateEditSetting(int delta);

    // draw partial top bar if editing, otherwise "Home"
    void drawTopBar(U8G2 &u8g2);

    // draw the menu items with partial scrolling
    void drawMenuList(U8G2 &u8g2);

    // partial text reversing:
    void drawPartialBar(U8G2 &u8g2, const String &label, float ratio);

    // clamp helpers
    float clampFloat(float x, float mn, float mx);
    unsigned long clampULong(unsigned long x, unsigned long mn, unsigned long mx);
};

#endif // MENU_H
