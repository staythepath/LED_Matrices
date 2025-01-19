#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RotaryEncoder.h"

/**
 * We have a single menu system with 7 items in "HOME" mode:
 *   0) "Select Animation"
 *   1) "Brightness"
 *   2) "Param1"   (was Fade)
 *   3) "Param2"   (was Tail)
 *   4) "Param3"   (was SpawnRate)
 *   5) "Param4"   (was MaxFlakes)
 *   6) "Speed" 
 *
 * We also have a sub-menu "Select Animation" with 3 animations:
 *   0) Traffic
 *   1) Blink
 *   2) RainbowWave
 *
 * We show 4 lines at once => partial scrolling.
 * If user single-clicks one of the items #1..6 => "edit mode"
 *   => top bar becomes partial highlight. Rotating encoder changes that param.
 * If user single-clicks or double-clicks => exit edit mode.
 *
 * If user picks index 0 => sub-menu "Select Animation" (MODE_SELECT_ANIM).
 */

class Menu {
public:
    Menu();

    void begin();
    void update(RotaryEncoder &encoder);
    void draw(U8G2 &u8g2);

private:
    // Number of items in "home" mode
    static const int HOME_COUNT = 7;  // 0..6
    // 3 animations to choose from
    static const int ANIM_COUNT = 3; 
    // We show 4 lines at once
    static const int VISIBLE_LINES = 4;

    // Enums for different modes: home, editing, or selecting animation
    enum MenuMode { 
        MODE_HOME,
        MODE_EDIT,
        MODE_SELECT_ANIM
    };
    MenuMode currentMode;

    // The main menu selection & offset
    int selection;
    int offset;

    // If editing => which item is being edited
    int editSettingIndex;

    // Single/double-click logic
    bool pendingSingleClick;
    unsigned long lastPressTime;
    static const unsigned long doubleClickThreshold = 400;
    int lastEncoderPos;

    // Sub-menu for animations
    int animSelection;
    int animOffset;

    // The home items & animation names
    static const char* homeItems[HOME_COUNT];
    static const char* animNames[ANIM_COUNT];

    // Helper methods
    void handleSingleClick(int rawPos);

    // For MODE_HOME
    void updateHomeSelection(int delta);
    void drawHomeList(U8G2 &u8g2);

    // For "editing" an item #1..6
    void updateEditSetting(int delta);
    void drawEditBar(U8G2 &u8g2);

    // For "Select Animation"
    void updateSelectAnim(int delta);
    void drawSelectAnim(U8G2 &u8g2);

    // Partial highlight for the top bar
    void drawPartialBar(U8G2 &u8g2, const String &label, float ratio);

    // Utility clamp
    float clampFloat(float x, float mn, float mx);
    unsigned long clampULong(unsigned long x, unsigned long mn, unsigned long mx);
};

#endif // MENU_H
