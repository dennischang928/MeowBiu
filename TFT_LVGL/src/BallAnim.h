#pragma once
#include <TFT_eSPI.h>

class BallAnim {
public:
  BallAnim(TFT_eSprite& spr, int w, int h);

  void update();
  
  void setposition(float nx, float ny);

private:
  TFT_eSprite& spr;   // reference to external sprite (shared)

  int W, H;
  float x, y, vy;
  int r;
};