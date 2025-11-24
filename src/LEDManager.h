#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>
#include <Preferences.h>

// Up to 8 panels of 16×16
static const int MAX_LEDS = 16 * 16 * 8;
extern CRGB leds[MAX_LEDS];

class BaseAnimation;

class LEDManager {
public:
    LEDManager();

    void begin();
    void update();
    void show();

    // Loading animation
    void showLoadingAnimation();
    void finishInitialization();

    // Panel count
    void setPanelCount(int count);
    int  getPanelCount() const;

    // Identify panels (stops animation, draws arrow+digit, waits 10s, restores)
    void identifyPanels();

    // Animations
    void setAnimation(int animIndex);
    int  getAnimation() const;
    size_t getAnimationCount() const;
    String getAnimationName(int animIndex) const;
    void setSpeed(int speed);
    int getSpeed() const { return _speed; }

    // Brightness
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // Palette
    void setPalette(int paletteIndex);
    int  getCurrentPalette() const;
    size_t getPaletteCount() const;
    String getPaletteNameAt(int index) const;
    const std::vector<CRGB>& getCurrentPaletteColors() const;
    bool setCustomPaletteHex(const std::vector<String>& hexColors);
    std::vector<String> getCustomPaletteHex() const;
    int getCustomPaletteIndex() const;
    bool addUserPalette(const String& name, const std::vector<String>& hexColors, int& newIndexOut);

    // Spawn
    void setSpawnRate(float rate);
    float getSpawnRate() const;

    // Max "cars"
    void setMaxFlakes(int max);
    int  getMaxFlakes() const;

    // Tail + fade
    void setTailLength(int length);
    int  getTailLength() const;
    void setFadeAmount(uint8_t amount);
    uint8_t getFadeAmount() const;

    // Panel (not currently used in HTML, but kept here)
    void swapPanels();
    void setPanelOrder(String order);
    void rotatePanel(String panel, int angle);
    int  getRotation(String panel) const;

    // Speed
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;
    
    // Game of Life specific controls
    void setColumnSkip(int skip);
    int getColumnSkip() const;
    void setGoLHighlightWidth(uint8_t width);
    uint8_t getGoLHighlightWidth() const;
    bool setGoLHighlightColorHex(const String& hex);
    String getGoLHighlightColorHex() const;
    void setGoLHighlightColorRGB(uint8_t r, uint8_t g, uint8_t b);
    void setGoLUpdateMode(uint8_t mode);
    uint8_t getGoLUpdateMode() const;
    void setGoLNeighborMode(uint8_t mode);
    uint8_t getGoLNeighborMode() const;
    void setGoLWrapEdges(bool wrap);
    bool getGoLWrapEdges() const;
    bool setGoLRules(const String& rule);
    void setGoLSeedDensity(uint8_t percent);
    uint8_t getGoLSeedDensity() const;
    void setGoLMutationChance(uint8_t percent);
    uint8_t getGoLMutationChance() const;
    String getGoLRules() const;
    void setGoLClusterColorMode(uint8_t mode);
    uint8_t getGoLClusterColorMode() const;
    void setGoLUniformBirths(bool enabled);
    bool getGoLUniformBirths() const;
    bool setGoLBirthColorHex(const String& hex);
    String getGoLBirthColorHex() const;
    void setGoLBirthColorRGB(uint8_t r, uint8_t g, uint8_t b);

    // Strange loop / automata controls
    void setAutomataMode(uint8_t mode);
    uint8_t getAutomataMode() const;
    void setAutomataPrimary(uint8_t value);
    uint8_t getAutomataPrimary() const;
    void setAutomataSecondary(uint8_t value);
    uint8_t getAutomataSecondary() const;
    void resetAutomataPattern();

    // Text scroller controls
    void setTextScrollMode(uint8_t mode);
    uint8_t getTextScrollMode() const;
    void setTextScrollSpeed(uint8_t percent);
    uint8_t getTextScrollSpeed() const;
    void setTextScrollDirection(uint8_t direction);
    uint8_t getTextScrollDirection() const;
    void setTextMirrorGlyphs(bool mirror);
    bool getTextMirrorGlyphs() const;
    void setTextCompactHeight(bool compact);
    bool getTextCompactHeight() const;
    void setTextReverseOrder(bool reverse);
    bool getTextReverseOrder() const;
    void setTextScrollMessage(uint8_t slot, const String& text);
    String getTextScrollMessage(uint8_t slot) const;

    // Presets
    std::vector<String> listPresets();
    bool savePreset(const String& name, String& errorMessage);
    bool loadPreset(const String& name, String& errorMessage);
    bool deletePreset(const String& name, String& errorMessage);

    int getPanelOrder() const { return panelOrder; }

private:
    // Re-init FastLED if panelCount changes
    void reinitFastLED();
    void cleanupAnimation();
    void createPalettes();
    void configureCurrentAnimation();
    void showBootPattern();

    // The bigger arrow + digit methods
    void drawUpArrow(int baseIndex);
    void drawLargeDigit(int baseIndex, int digit);

    struct PresetSnapshot {
        int panelCount;
        int animationIndex;
        int paletteIndex;
        uint8_t brightness;
        uint8_t fadeAmount;
        int tailLength;
        int speed;
        float spawnRate;
        int maxFlakes;
        int columnSkip;
        uint8_t highlightWidth;
        String highlightColorHex;
        bool uniformBirths;
        String birthColorHex;
        uint8_t golUpdateMode;
        uint8_t golNeighborMode;
        bool golWrapEdges;
        uint8_t golClusterColorMode;
        uint8_t golSeedDensity;
        uint8_t golMutationChance;
        String golRuleString;
        int panelOrder;
        int rotation1;
        int rotation2;
        int rotation3;
        uint8_t automataMode;
        uint8_t automataPrimary;
        uint8_t automataSecondary;
        uint8_t textMode;
        uint8_t textSpeed;
        uint8_t textDirection;
        bool textMirror;
        bool textCompact;
        bool textReverse;
        String textPrimary;
        String textLeft;
        String textRight;
    };

    PresetSnapshot captureCurrentPreset() const;
    String serializePreset(const PresetSnapshot& snapshot) const;
    bool deserializePreset(const String& payload, PresetSnapshot& snapshot) const;
    bool ensurePrefsReady();
    String sanitizePresetName(const String& name) const;
    std::vector<String> loadPresetNameList();
    void storePresetNameList(const std::vector<String>& names);
    void applyPreset(const PresetSnapshot& snapshot);
    String makePresetStorageKey(const String& name) const;
    String legacyPresetStorageKey(const String& name) const;

private:
    bool _isInitializing;  // Flag to indicate system is still initializing
    int      _panelCount; // default: 3
    uint16_t _numLeds;

    uint8_t  _brightness;
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String>            PALETTE_NAMES;
    int                            currentPalette;
    std::vector<CRGB>              _customPalette;
    int                            _customPaletteIndex;
    std::vector<std::vector<CRGB>> _userPalettes;
    std::vector<String>            _userPaletteNames;

    BaseAnimation* _currentAnimation;
    int            _currentAnimationIndex;
    std::vector<String> _animationNames;

    float   spawnRate;
    int     maxFlakes;
    int     tailLength;
    uint8_t fadeAmount;

    int panelOrder;
    int rotationAngle1;
    int rotationAngle2;
    int rotationAngle3;

    unsigned long ledUpdateInterval;
   unsigned long lastLedUpdate;
   int _speed;
    int _columnSkip; // For Game of Life animation
    uint8_t _golHighlightWidth;
    CRGB _golHighlightColor;
    uint8_t _golUpdateMode;
    uint8_t _golNeighborMode;
    bool _golWrapEdges;
    uint8_t _golClusterColorMode;
    bool _golUniformBirths;
    CRGB _golBirthColor;
    uint8_t _golSeedDensity;
    uint8_t _golMutationChance;
    uint16_t _golBirthMask;
    uint16_t _golSurviveMask;
    String _golRuleString;
    uint8_t _automataMode;
    uint8_t _automataPrimary;
    uint8_t _automataSecondary;
    uint8_t _textScrollMode;
    uint8_t _textScrollSpeed;
    uint8_t _textScrollDirection;
    bool _textMirrorGlyphs;
    bool _textCompactHeight;
    bool _textReverseOrder;
    String _textPrimary;
    String _textLeft;
    String _textRight;
    Preferences _prefs;
    bool _prefsReady;

    void loadCustomPaletteFromPrefs();
    void saveCustomPaletteToPrefs();
    void loadUserPalettesFromPrefs();
    void saveUserPalettesToPrefs();
    bool parseHexColor(const String& hex, CRGB& outColor) const;
    String colorToHex(const CRGB& color) const;
    String sanitizePaletteName(const String& name) const;

    static constexpr size_t MAX_CUSTOM_PALETTE_COLORS = 16;
    static constexpr size_t MAX_USER_PALETTES = 24;
};

#endif // LEDMANAGER_H


