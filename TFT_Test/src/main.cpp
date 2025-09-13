#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BallAnim.h"
#define nBalls 10

TFT_eSPI tft;
TFT_eSprite spr(&tft);

BallAnim* ball[nBalls];


void setup() {

  for(int i=0; i<nBalls; i++) {
    ball[i] = new BallAnim(spr, 240, 240);
    ball[i]->setposition(random(12, 240-12), random(12, 240-12));
  }
  tft.init();
  tft.setRotation(0);
  spr.createSprite(240, 240);  // adjust to your TFT size
  spr.fillSprite(TFT_BLACK);
  
  // ball.begin();
  // ball2.begin();
  // ball.setStartPosition(100, 20);
  // ball2.setStartPosition(30, 30);
}

int states = 0; // 0: idle, 1: waking, 2: woken, 3: sleeping

void loop() {
  spr.fillSprite(TFT_BLACK);

  for(int i=0; i<nBalls; i++) {
    ball[i]->update();
  }

  spr.pushSprite(0, 0);
}


