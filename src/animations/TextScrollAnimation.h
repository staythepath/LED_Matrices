#pragma once

#include "BaseAnimation.h"
#include <vector>

class TextScrollAnimation : public BaseAnimation {
public:
    struct GlyphDef {
        char character;
        uint8_t width;
        uint8_t rows[8];
    };

    enum class Mode : uint8_t {
        FullMatrix = 0,
        DualColumns = 1
    };

    enum class Slot : uint8_t {
        Primary = 0,
        Left = 1,
        Right = 2
    };

    TextScrollAnimation(uint16_t numLeds, uint8_t brightness, int panelCount);

    void begin() override;
    void update() override;

    bool isTextScroller() const override { return true; }

    void setMode(Mode mode);
    void setScrollSpeed(uint16_t intervalMs);
    void setTextColor(const CRGB& color);
    void setMessage(Slot slot, const String& message);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);
    void setScrollDirection(bool reverse);
    void setMirrorGlyphs(bool mirror);
    void setCompactHeight(bool compact);
    void setReverseTextOrder(bool reverse);

private:
    struct ChannelState {
        String raw;
        std::vector<uint16_t> columns;
        size_t offset;
        bool usesTime;
        bool usesDate;

        ChannelState()
            : raw("")
            , offset(0)
            , usesTime(false)
            , usesDate(false) {}
    };

    void rebuildColumns(Slot slot);
    void refreshDynamicContent();
    String resolvePlaceholders(const ChannelState& channel) const;
    std::vector<uint16_t> buildColumnsForString(const String& resolved, int widthMultiplier) const;
    uint16_t scaleColumn(uint8_t columnBits, int sourceHeight, int targetHeight) const;
    const GlyphDef* glyphForChar(char c) const;
    void renderFullMatrix();
    void renderDualColumns();
    int mapXYtoLED(int x, int y) const;
    void writeColumnToLeds(int x, uint16_t columnBits);
    void clearLeds();

    int _width;
    int _height;
    Mode _mode;
    uint16_t _scrollInterval;
    uint32_t _lastShiftMs;
    uint32_t _lastDynamicRefresh;
    CRGB _textColor;
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;
    bool _reverseScroll;
    bool _mirrorGlyphs;
    bool _compactHeight;
    bool _reverseTextOrder;
    ChannelState _channels[3];
};
