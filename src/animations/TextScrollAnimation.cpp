#include "TextScrollAnimation.h"

#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <ctime>

#include <time.h>

extern CRGB leds[];

namespace {
    constexpr uint8_t FONT_HEIGHT = 8;

    // 8x8 font derived from font8x8_basic (ASCII 0x20-0x7F)
    constexpr TextScrollAnimation::GlyphDef FONT_TABLE[] = {
        { ' ', 8, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
        { '!', 8, { 0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00 } },
        { '\"',8, { 0x36,0x36,0x24,0x00,0x00,0x00,0x00,0x00 } },
        { '#', 8, { 0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00 } },
        { '$', 8, { 0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00 } },
        { '%', 8, { 0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00 } },
        { '&', 8, { 0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00 } },
        { '\'',8, { 0x06,0x06,0x04,0x00,0x00,0x00,0x00,0x00 } },
        { '(', 8, { 0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00 } },
        { ')', 8, { 0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00 } },
        { '*', 8, { 0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00 } },
        { '+', 8, { 0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00 } },
        { ',', 8, { 0x00,0x00,0x00,0x00,0x0C,0x0C,0x18,0x00 } },
        { '-', 8, { 0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00 } },
        { '.', 8, { 0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00 } },
        { '/', 8, { 0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00 } },
        { '0', 8, { 0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00 } },
        { '1', 8, { 0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00 } },
        { '2', 8, { 0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00 } },
        { '3', 8, { 0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00 } },
        { '4', 8, { 0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00 } },
        { '5', 8, { 0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00 } },
        { '6', 8, { 0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00 } },
        { '7', 8, { 0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00 } },
        { '8', 8, { 0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00 } },
        { '9', 8, { 0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00 } },
        { ':', 8, { 0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,0x00 } },
        { ';', 8, { 0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x18,0x00 } },
        { '<', 8, { 0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00 } },
        { '=', 8, { 0x00,0x00,0x3F,0x00,0x3F,0x00,0x00,0x00 } },
        { '>', 8, { 0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00 } },
        { '?', 8, { 0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00 } },
        { '@', 8, { 0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00 } },
        { 'A', 8, { 0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00 } },
        { 'B', 8, { 0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00 } },
        { 'C', 8, { 0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00 } },
        { 'D', 8, { 0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00 } },
        { 'E', 8, { 0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00 } },
        { 'F', 8, { 0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00 } },
        { 'G', 8, { 0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00 } },
        { 'H', 8, { 0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00 } },
        { 'I', 8, { 0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 } },
        { 'J', 8, { 0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00 } },
        { 'K', 8, { 0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00 } },
        { 'L', 8, { 0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00 } },
        { 'M', 8, { 0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00 } },
        { 'N', 8, { 0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00 } },
        { 'O', 8, { 0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00 } },
        { 'P', 8, { 0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00 } },
        { 'Q', 8, { 0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00 } },
        { 'R', 8, { 0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00 } },
        { 'S', 8, { 0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00 } },
        { 'T', 8, { 0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 } },
        { 'U', 8, { 0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00 } },
        { 'V', 8, { 0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00 } },
        { 'W', 8, { 0x63,0x63,0x6B,0x7F,0x7F,0x77,0x63,0x00 } },
        { 'X', 8, { 0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00 } },
        { 'Y', 8, { 0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00 } },
        { 'Z', 8, { 0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00 } },
        { '[', 8, { 0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00 } },
        { '\\',8, { 0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00 } },
        { ']', 8, { 0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00 } },
        { '^', 8, { 0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00 } },
        { '_', 8, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF } },
        { '`', 8, { 0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00 } },
        { 'a', 8, { 0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00 } },
        { 'b', 8, { 0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00 } },
        { 'c', 8, { 0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00 } },
        { 'd', 8, { 0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00 } },
        { 'e', 8, { 0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00 } },
        { 'f', 8, { 0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00 } },
        { 'g', 8, { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F } },
        { 'h', 8, { 0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00 } },
        { 'i', 8, { 0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00 } },
        { 'j', 8, { 0x30,0x00,0x38,0x30,0x30,0x33,0x33,0x1E } },
        { 'k', 8, { 0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00 } },
        { 'l', 8, { 0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 } },
        { 'm', 8, { 0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00 } },
        { 'n', 8, { 0x00,0x00,0x1B,0x37,0x33,0x33,0x33,0x00 } },
        { 'o', 8, { 0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00 } },
        { 'p', 8, { 0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F } },
        { 'q', 8, { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78 } },
        { 'r', 8, { 0x00,0x00,0x1B,0x36,0x36,0x0E,0x06,0x00 } },
        { 's', 8, { 0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00 } },
        { 't', 8, { 0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00 } },
        { 'u', 8, { 0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00 } },
        { 'v', 8, { 0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00 } },
        { 'w', 8, { 0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00 } },
        { 'x', 8, { 0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00 } },
        { 'y', 8, { 0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F } },
        { 'z', 8, { 0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00 } },
        { '{', 8, { 0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00 } },
        { '|', 8, { 0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00 } },
        { '}', 8, { 0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00 } },
        { '~', 8, { 0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00 } }
    };

    constexpr size_t FONT_TABLE_SIZE = sizeof(FONT_TABLE) / sizeof(FONT_TABLE[0]);
}

TextScrollAnimation::TextScrollAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount)
    , _width(std::max(16, panelCount * 16))
    , _height(16)
    , _mode(Mode::FullMatrix)
    , _scrollInterval(120)
    , _lastShiftMs(0)
    , _lastDynamicRefresh(0)
    , _textColor(CRGB::White)
    , _panelOrder(0)
    , _rotationAngle1(0)
    , _rotationAngle2(0)
    , _rotationAngle3(0)
    , _reverseScroll(false)
    , _mirrorGlyphs(false)
    , _compactHeight(false)
    , _reverseTextOrder(false)
{
    _channels[static_cast<size_t>(Slot::Primary)].raw = "[TIME]";
    _channels[static_cast<size_t>(Slot::Left)].raw = "LEFT";
    _channels[static_cast<size_t>(Slot::Right)].raw = "[DATE]";

    for (auto& ch : _channels) {
        ch.offset = 0;
        ch.usesTime = ch.raw.indexOf("[TIME]") >= 0;
        ch.usesDate = ch.raw.indexOf("[DATE]") >= 0;
    }
}

void TextScrollAnimation::begin() {
    fill_solid(leds, _numLeds, CRGB::Black);
    for (int i = 0; i < 3; ++i) {
        rebuildColumns(static_cast<Slot>(i));
        _channels[i].offset = 0;
    }
    _lastShiftMs = millis();
    _lastDynamicRefresh = millis();
}

void TextScrollAnimation::update() {
    refreshDynamicContent();

    const uint32_t now = millis();
    if (now - _lastShiftMs >= _scrollInterval) {
        if (_mode == Mode::FullMatrix) {
            auto& channel = _channels[static_cast<size_t>(Slot::Primary)];
            if (!channel.columns.empty()) {
                const size_t size = channel.columns.size();
                channel.offset = _reverseScroll
                    ? ((channel.offset + size - 1) % size)
                    : ((channel.offset + 1) % size);
            }
        } else {
            auto& left = _channels[static_cast<size_t>(Slot::Left)];
            auto& right = _channels[static_cast<size_t>(Slot::Right)];
            if (!left.columns.empty()) {
                const size_t size = left.columns.size();
                left.offset = _reverseScroll
                    ? ((left.offset + size - 1) % size)
                    : ((left.offset + 1) % size);
            }
            if (!right.columns.empty()) {
                const size_t size = right.columns.size();
                right.offset = _reverseScroll
                    ? ((right.offset + size - 1) % size)
                    : ((right.offset + 1) % size);
            }
        }
        _lastShiftMs = now;
    }

    if (_mode == Mode::FullMatrix) {
        renderFullMatrix();
    } else {
        renderDualColumns();
    }
}

void TextScrollAnimation::setMode(Mode mode) {
    if (_mode == mode) {
        return;
    }
    _mode = mode;
    for (int i = 0; i < 3; ++i) {
        rebuildColumns(static_cast<Slot>(i));
        _channels[i].offset = 0;
    }
}

void TextScrollAnimation::setScrollSpeed(uint16_t intervalMs) {
    if (intervalMs < 15) {
        intervalMs = 15;
    }
    _scrollInterval = intervalMs;
}

void TextScrollAnimation::setTextColor(const CRGB& color) {
    _textColor = color;
}

void TextScrollAnimation::setScrollDirection(bool reverse) {
    _reverseScroll = reverse;
}

void TextScrollAnimation::setMirrorGlyphs(bool mirror) {
    _mirrorGlyphs = mirror;
    for (int i = 0; i < 3; ++i) {
        rebuildColumns(static_cast<Slot>(i));
    }
}

void TextScrollAnimation::setCompactHeight(bool compact) {
    _compactHeight = compact;
    for (int i = 0; i < 3; ++i) {
        rebuildColumns(static_cast<Slot>(i));
    }
}

void TextScrollAnimation::setReverseTextOrder(bool reverse) {
    _reverseTextOrder = reverse;
    for (int i = 0; i < 3; ++i) {
        rebuildColumns(static_cast<Slot>(i));
    }
}

void TextScrollAnimation::setMessage(Slot slot, const String& message) {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= 3) return;

    String sanitized = message;
    sanitized.trim();
    if (sanitized.length() > 96) {
        sanitized = sanitized.substring(0, 96);
    }
    _channels[idx].raw = sanitized;
    _channels[idx].usesTime = sanitized.indexOf("[TIME]") >= 0;
    _channels[idx].usesDate = sanitized.indexOf("[DATE]") >= 0;
    rebuildColumns(slot);
    _channels[idx].offset = 0;
}

void TextScrollAnimation::setPanelOrder(int order) {
    _panelOrder = (order == 0) ? 0 : 1;
}

void TextScrollAnimation::setRotationAngle1(int angle) {
    _rotationAngle1 = angle;
}

void TextScrollAnimation::setRotationAngle2(int angle) {
    _rotationAngle2 = angle;
}

void TextScrollAnimation::setRotationAngle3(int angle) {
    _rotationAngle3 = angle;
}

void TextScrollAnimation::rebuildColumns(Slot slot) {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= 3) return;

    const String resolved = resolvePlaceholders(_channels[idx]);
    const int widthMultiplier = 1;
    _channels[idx].columns = buildColumnsForString(resolved, std::max(1, widthMultiplier));
    if (_channels[idx].columns.empty()) {
        _channels[idx].columns.assign(FONT_HEIGHT, 0);
    }
    if (_channels[idx].offset >= _channels[idx].columns.size()) {
        _channels[idx].offset = 0;
    }
}

void TextScrollAnimation::refreshDynamicContent() {
    const uint32_t now = millis();
    if (now - _lastDynamicRefresh < 1000) {
        return;
    }
    bool refreshed = false;
    for (int i = 0; i < 3; ++i) {
        if (_channels[i].usesTime || _channels[i].usesDate) {
            rebuildColumns(static_cast<Slot>(i));
            refreshed = true;
        }
    }
    if (refreshed) {
        _lastDynamicRefresh = now;
    }
}

String TextScrollAnimation::resolvePlaceholders(const ChannelState& channel) const {
    String resolved = channel.raw;
    if (resolved.isEmpty()) {
        return " ";
    }

    struct tm timeinfo{};
    bool hasTime = getLocalTime(&timeinfo, 100) == true;
    if (resolved.indexOf("[TIME]") >= 0) {
        char buf[6] = "--:--";
        if (hasTime) {
            strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        }
        resolved.replace("[TIME]", String(buf));
    }
    if (resolved.indexOf("[DATE]") >= 0) {
        char buf[11] = "--/--/----";
        if (hasTime) {
            strftime(buf, sizeof(buf), "%m/%d/%y", &timeinfo);
        }
        resolved.replace("[DATE]", String(buf));
    }
    return resolved;
}

std::vector<uint16_t> TextScrollAnimation::buildColumnsForString(const String& text, int widthMultiplier) const {
    std::vector<uint16_t> columns;
    columns.reserve(text.length() * 8);

    constexpr int DISPLAY_WIDTH = 6; // squeeze characters to fit more on 32x16
    for (size_t i = 0; i < text.length(); ++i) {
        size_t idx = _reverseTextOrder ? (text.length() - 1 - i) : i;
        char c = text.charAt(idx);
        bool isLower = (c >= 'a' && c <= 'z');
        const GlyphDef* glyph = glyphForChar(c);
        if (!glyph) continue;
        const int glyphWidth = std::max<uint8_t>(1, glyph->width);
        const int effectiveWidth = std::min(glyphWidth, DISPLAY_WIDTH);
        const int charWidthMultiplier = isLower ? std::max(1, widthMultiplier - 1) : widthMultiplier;
        const int scaledWidth = effectiveWidth * charWidthMultiplier;
        std::vector<uint16_t> glyphColumns;
        glyphColumns.reserve(static_cast<size_t>(scaledWidth));
        for (int tx = 0; tx < scaledWidth; ++tx) {
            int sampleX = (tx * glyphWidth) / std::max(1, scaledWidth);
            if (sampleX >= glyphWidth) sampleX = glyphWidth - 1;
            uint8_t srcColumn = 0;
            for (int row = 0; row < FONT_HEIGHT; ++row) {
                if (glyph->rows[row] & (1 << sampleX)) {
                    srcColumn |= (1 << row);
                }
            }
            uint16_t scaled = scaleColumn(srcColumn, FONT_HEIGHT, _height);
            glyphColumns.push_back(scaled);
        }
        if (_mirrorGlyphs) {
            std::reverse(glyphColumns.begin(), glyphColumns.end());
        }
        columns.insert(columns.end(), glyphColumns.begin(), glyphColumns.end());
        int spacing = 1;
        for (int s = 0; s < spacing; ++s) {
            columns.push_back(0);
        }
    }

    if (columns.empty()) {
        columns.assign(FONT_HEIGHT, 0);
    }
    return columns;
}

uint16_t TextScrollAnimation::scaleColumn(uint8_t columnBits, int sourceHeight, int targetHeight) const {
    uint16_t result = 0;
    int desiredHeight = _compactHeight ? std::max(1, targetHeight / 2) : sourceHeight * 2;
    if (desiredHeight > targetHeight) {
        desiredHeight = targetHeight;
    }
    int verticalOffset = _compactHeight ? 0 : (targetHeight - desiredHeight) / 2;

    for (int ty = 0; ty < desiredHeight; ++ty) {
        int sy = (ty * sourceHeight) / desiredHeight;
        if (sy >= sourceHeight) sy = sourceHeight - 1;
        bool on = (columnBits >> sy) & 0x01;
        if (on) {
            result |= (1 << (ty + verticalOffset));
        }
    }
    return result;
}

const TextScrollAnimation::GlyphDef* TextScrollAnimation::glyphForChar(char c) const {
    for (const auto& glyph : FONT_TABLE) {
        if (glyph.character == c) {
            return &glyph;
        }
    }
    return &FONT_TABLE[FONT_TABLE_SIZE - 1];
}

void TextScrollAnimation::renderFullMatrix() {
    clearLeds();
    auto& channel = _channels[static_cast<size_t>(Slot::Primary)];
    if (channel.columns.empty()) {
        return;
    }
    const size_t size = channel.columns.size();
    for (int x = 0; x < _width; ++x) {
        const size_t idx = (channel.offset + x) % size;
        writeColumnToLeds(x, channel.columns[idx]);
    }
}

void TextScrollAnimation::renderDualColumns() {
    clearLeds();
    int half = std::max(1, _width / 2);
    auto& left = _channels[static_cast<size_t>(Slot::Left)];
    auto& right = _channels[static_cast<size_t>(Slot::Right)];
    const size_t leftSize = left.columns.size();
    const size_t rightSize = right.columns.size();
    for (int x = 0; x < half; ++x) {
        uint16_t column = 0;
        if (leftSize) {
            const size_t idx = (left.offset + x) % leftSize;
            column = left.columns[idx];
        }
        writeColumnToLeds(x, column);
    }
    for (int x = half; x < _width; ++x) {
        uint16_t column = 0;
        if (rightSize) {
            int localX = x - half;
            const size_t idx = (right.offset + localX) % rightSize;
            column = right.columns[idx];
        }
        writeColumnToLeds(x, column);
    }
}

void TextScrollAnimation::writeColumnToLeds(int x, uint16_t columnBits) {
    for (int y = 0; y < _height; ++y) {
        int idx = mapXYtoLED(x, y);
        if (idx < 0 || idx >= (int)_numLeds) continue;
        leds[idx] = (columnBits & (1 << y)) ? _textColor : CRGB::Black;
    }
}

void TextScrollAnimation::clearLeds() {
    fill_solid(leds, _numLeds, CRGB::Black);
}

int TextScrollAnimation::mapXYtoLED(int x, int y) const {
    if (x < 0 || y < 0 || y >= _height) return -1;
    const int panel = x / 16;
    if (panel < 0 || panel >= _panelCount) return -1;

    int localX = x % 16;
    int localY = y;

    int angle = 0;
    switch (panel) {
        case 0: angle = _rotationAngle1; break;
        case 1: angle = _rotationAngle2; break;
        case 2: angle = _rotationAngle3; break;
        default: angle = 0; break;
    }
    angle = ((angle % 360) + 360) % 360;

    int rx = localX;
    int ry = localY;
    switch (angle) {
        case 90:  rx = 15 - localY; ry = localX; break;
        case 180: rx = 15 - localX; ry = 15 - localY; break;
        case 270: rx = localY; ry = 15 - localX; break;
        default: break;
    }

    int mappedPanel = panel;
    if (_panelOrder != 0) {
        mappedPanel = (_panelCount - 1) - panel;
    }

    const bool reverse = (ry & 1);
    const int serpX = reverse ? (15 - rx) : rx;
    const int index = mappedPanel * 16 * 16 + ry * 16 + serpX;
    if (index < 0 || index >= (int)_numLeds) {
        return -1;
    }
    return index;
}
