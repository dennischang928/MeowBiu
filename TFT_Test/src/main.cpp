// =========================================================================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// ---------- Pins (XIAO ESP32-C3) ----------
#define TFT_DC  2      // D0 -> GPIO2
#define TFT_CS  3      // D1 -> GPIO3
// Optional backlight pin if your breakout has one:
// #define TFT_BL 6

// ---------- Display ----------
Adafruit_GC9A01A tft(TFT_CS, TFT_DC);
constexpr int W = 240, H = 240;

// ---------- Optional off-screen buffer (uncomment for flicker-free) ----------
// Uses ~115KB RAM (OK on ESP32-C3). Comment out to draw directly to TFT.
// #define USE_CANVAS
#ifdef USE_CANVAS
  GFXcanvas16 canvas(W, H);
  Adafruit_GFX &G = canvas;
#else
  Adafruit_GFX &G = tft;
#endif

// ---------- Simple easing & tween ----------
using EaseFn = float(*)(float);

float easeLinear(float t) { return t; }
float easeInOutQuad(float t) { return (t<0.5f) ? 2*t*t : 1 - powf(-2*t+2, 2)/2; }

struct Tween {
  float from=0, to=0;
  uint32_t start=0, dur=1000; // ms
  EaseFn ease = easeLinear;
  void startNow(float f, float t, uint32_t d, EaseFn e=easeLinear) {
    from=f; to=t; dur=d; ease=e; start=millis();
  }
  float value() const {
    uint32_t now = millis();
    if (now <= start) return from;
    uint32_t dt = now - start;
    if (dt >= dur) return to;
    float u = ease((float)dt/(float)dur);
    return from + (to - from) * u;
  }
  bool done() const { return (millis() - start) >= dur; }
};

// ---------- Idle/interrupt ----------
volatile bool IsIdle = true;
volatile uint32_t lastTriggerTime = 0;
const uint32_t idleResetDelay = 3000;

void IRAM_ATTR ISR_pin() {
  IsIdle = false;
  lastTriggerTime = millis();
}

// ---------- Scene state ----------
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  #ifdef USE_CANVAS
    return Adafruit_GFX::color565(r,g,b);
  #else
    return tft.color565(r,g,b);
  #endif
}

Tween ringProg;    // 0..1 progress for spinner
Tween dotX, dotY;  // position of a bouncing dot
int   dotR = 8;

void startAnimations() {
  // loop spinner 0->1 continuously
  ringProg.startNow(0.f, 1.f, 1500, easeInOutQuad);

  // bounce the dot across the screen diagonally
  dotX.startNow(20, W-20, 1800, easeInOutQuad);
  dotY.startNow(20, H-20, 1200, easeInOutQuad);
}

void restartFinishedTweens() {
  // spinner: wrap
  if (ringProg.done()) ringProg.startNow(0.f, 1.f, 1500, easeInOutQuad);

  // dot: ping-pong on each axis
  auto pingpong = [](Tween &tw){
    if (tw.done()) {
      float a = tw.to, b = tw.from; // swap ends
      tw.startNow(a, b, tw.dur, tw.ease);
    }
  };
  pingpong(dotX);
  pingpong(dotY);
}

// ---------- Drawing helpers ----------
void drawSpinner(float p) {
  // p in [0..1] -> angle sweep
  const int cx = W/2, cy = H/2, rOuter=100, rInner=86;
  // Clear frame
  G.fillScreen(rgb(0,0,0));

  // Track circles
  G.drawCircle(cx, cy, rOuter, rgb(60,60,60));
  G.drawCircle(cx, cy, rInner, rgb(60,60,60));

  // Fill arc (from -90deg)
  int startDeg = -90;
  int endDeg   = startDeg + (int)(p * 360.0f);
  uint16_t col = rgb(0,220,180);
  for (int d = startDeg; d <= endDeg; d += 2) {
    float rad = d * (float)M_PI / 180.0f;
    int x1 = cx + (int)roundf(rInner * cosf(rad));
    int y1 = cy + (int)roundf(rInner * sinf(rad));
    int x2 = cx + (int)roundf(rOuter * cosf(rad));
    int y2 = cy + (int)roundf(rOuter * sinf(rad));
    G.drawLine(x1,y1,x2,y2,col);
  }
  // Clean inner
  G.fillCircle(cx, cy, rInner-1, rgb(0,0,0));
  // Title
  G.setTextSize(2);
  G.setTextColor(rgb(255,255,255), rgb(0,0,0));
  G.setCursor(70, 12);
  G.print("GC9A01A Demo");
}

void drawBouncingDot(int x, int y) {
  G.fillCircle(x, y, dotR, rgb(255,120,0));
}

// Your existing benchmark, now using ‘G’ target and no blocking delays
unsigned long testFilledRoundRects() {
  uint32_t start = micros();
  int cx = W/2 - 1, cy = H/2 - 1;
  int lim = min(W,H);
  for (int i = lim; i > 20; i -= 50) {
    int i2 = i/2;
    G.fillRoundRect(cx-i2, cy-i2, i, i, i/8, rgb(i,i,i));
    yield();
  }
  for (int i = 21; i < lim; i += 50) {
    int i2 = i/2;
    G.fillRoundRect(cx-i2, cy-i2, i, i, i/8, rgb(i,i,i));
    yield();
  }
  return micros() - start;
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  Serial.println("GC9A01A Anim UI");

  pinMode(9, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(9), ISR_pin, FALLING);

  tft.begin(80000000);
  #ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  #endif

  #ifdef USE_CANVAS
    canvas.fillScreen(0);
    tft.drawRGBBitmap(0,0,(uint16_t*)canvas.getBuffer(), W, H);
  #else
    tft.fillScreen(0);
  #endif

  startAnimations();
}

void loop() {
  // auto-reset to idle after no triggers
  if (!IsIdle && (millis() - lastTriggerTime >= idleResetDelay)) {
    IsIdle = true;
    Serial.println("IsIdle -> true");
  }

  if (IsIdle) {
    // Idle screen
    G.fillScreen(rgb(0,0,0));
    G.setTextSize(3);
    G.setTextColor(rgb(255,255,255), rgb(0,0,0));
    G.setCursor(90, 110);
    G.print("IDLE");
  } else {
    // Animate
    restartFinishedTweens();

    drawSpinner(ringProg.value());
    drawBouncingDot((int)dotX.value(), (int)dotY.value());

    // (Optional) run your rect test in active mode
    // testFilledRoundRects();
  }

  // Push buffer (if used)
  #ifdef USE_CANVAS
    tft.drawRGBBitmap(0,0,(uint16_t*)canvas.getBuffer(), W, H);
  #endif

  // Frame pacing ~60 FPS
  // delay(16);
  yield();
}