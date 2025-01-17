#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RotaryEncoder.h"

/*
   A 3-level menu:
     Level 0 (Home): 5 color folders
     Level 1: each color has 7 subfolders (e.g. "Red 1..7")
     Level 2: each subfolder has 10 items (e.g. "Red 3 A..J")

   4 lines visible on screen at once (partial scrolling).
   Wrap-around scrolling top/bottom:
     - If selection < 0 => selection = maxItems-1
       If selection >= maxItems => selection=0
   offset[level] = which item index is on line 0
   If selection < offset => offset = selection
   If selection >= offset+4 => offset = selection-3
   (We clamp offset in [0..maxItems-4], if maxItems>=4).

   Single click => go deeper if not at level2
   Double click => go back if level>0
*/

class Menu {
public:
    Menu();
    void begin();
    void update(RotaryEncoder &encoder);
    void draw(U8G2 &u8g2);

private:
    // Data sizes:
    static const int LEVEL0_COUNT = 5;  // "Red","Blue","Green","Orange","Purple"
    static const int LEVEL1_COUNT = 7;  // e.g. "Red 1..7"
    static const int LEVEL2_COUNT = 10; // e.g. "Red 3 A..J"

    // 4 lines on screen
    static const int VISIBLE_LINES = 4;

    // currentLevel in [0..2]
    int currentLevel;

    // selection[level] = which item is selected in [0..n-1]
    int selection[3];
    // offset[level] = which item index is shown at line 0
    int offset[3];

    // We'll store the "folder name" for each level, e.g. "Home", "Red", "Red 3"
    String folderNames[3];

    // Hard-coded top-level folder names (level0)
    static const char* level0Folders[LEVEL0_COUNT];

    // Checking single/double click
    int checkButtonEvents(RotaryEncoder &encoder);
    bool waitingForSecondClick;
    unsigned long lastPressTime;
    static const unsigned long doubleClickThreshold = 400;

    // We store last raw encoder reading so we can detect delta
    int lastEncoderRaw;

    // helpers
    int wrapIndex(int idx, int maxVal);
    void setFolderNames();
};

#endif
