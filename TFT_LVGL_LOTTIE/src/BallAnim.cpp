#include "BallAnim.h"

BallAnim::BallAnim(TFT_eSprite& sprite, int w, int h)
  : spr(sprite), W(w), H(h) {
  r = 12;
  x = random(r, W - r);
  y = r + 5;
  vy = 0.0f;
}

void BallAnim::setposition(float nx, float ny) {
  x = nx; y = ny;
}

void BallAnim::update() {
  // gravity
  float gravity = 0.6f;
  y += vy;
  vy += gravity;
  if (y > H - r) {
    y = H - r;
    vy = -vy * 0.8f;  // bounce with damping
  }


  // draw into shared sprite
  spr.fillSmoothCircle((int)x, (int)y, r, TFT_WHITE);
}