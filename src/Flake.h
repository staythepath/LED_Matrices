// Flake.h
#ifndef FLAKE_H
#define FLAKE_H

#include <FastLED.h>

struct Flake {
  int x; // X position
  int y; // Y position
  int dx; // Movement in X
  int dy; // Movement in Y
  float frac; // Fraction for color interpolation
  CRGB startColor;
  CRGB endColor;
  bool bounce;
};

#endif // FLAKE_H
