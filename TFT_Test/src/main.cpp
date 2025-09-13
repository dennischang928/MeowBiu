// Bouncing ball with non-linear motion (eased + gravity-like bounces)
// Uses TFT_eSPI and a Sprite for smooth, flicker-free animation.

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// --------- Timing ----------
static const uint16_t FPS = 60;           // target fps
static const uint32_t FRAME_MS = 1000 / FPS;

// --------- Ball ----------
struct Ball {
  float x, y;        // position
  float vx, vy;      // velocity
  float r;           // radius
  uint16_t color;
} ball;

// World (screen) bounds
int16_t W, H;
float floorY;        // y position of "floor"

// --------- Motion params ----------
float gravity = 1200.0f;        // px/s^2
float restitution = 0.75f;      // bounce energy retention
float friction = 0.999f;        // mild damping each frame

// Horizontal tween (non-linear ease for back-and-forth)
float easeInOutQuad(float t) {  // t in [0..1]
  return (t < 0.5f) ? 2.0f*t*t : 1.0f - powf(-2.0f*t + 2.0f, 2.0f) * 0.5f;
}

float pingPong01(float t) {     // 0→1→0 triangle wave
  float f = fmodf(t, 2.0f);
  return (f <= 1.0f) ? f : (2.0f - f);
}

// Map a 0..1 eased value to A..B
float map01(float u, float a, float b) { return a + (b - a) * u; }

// --------- Draw helpers ----------
void drawBackground() {
  spr.fillSprite(TFT_BLACK);

  // Simple gradient backdrop
  for (int y = 0; y < H; ++y) {
    uint8_t v = (uint8_t)map01((float)y / (float)H, 20, 70);
    spr.drawFastHLine(0, y, W, spr.color565(0, v, v + 40));
  }

  // Floor line
  spr.drawFastHLine(0, (int)floorY, W, TFT_DARKGREY);
}

// Uses anti-aliased circle if available
void drawBall(float x, float y, float r, uint16_t c) {
  // fillSmoothCircle blends edges against current background
  spr.fillSmoothCircle((int)x, (int)y, (int)r, c, TFT_BLACK);
}

bool triggered = false;
long lastTriggerTime = 0;

void ISR_pin() {
  triggered = true;
  if(lastTriggerTime && (millis() - lastTriggerTime) < 50) {
    // ignore if within 200ms of last trigger
    return;
  }
  lastTriggerTime = millis();
}

// --------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(9, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(9), ISR_pin, FALLING);

  tft.init();
  tft.setRotation(0);

  W = tft.width();
  H = tft.height();
  floorY = H - 12;         // little margin above screen bottom

  // Create a full-screen sprite (16-bit color)
  spr.createSprite(W, H);
  spr.setSwapBytes(true);

  // Ball initial state
  ball.r = 12;
  ball.x = W * 0.25f;
  ball.y = H * 0.25f;
  ball.vx = 0.0f;
  ball.vy = 0.0f;
  ball.color = TFT_YELLOW;

  // First frame
  drawBackground();
  drawBall(ball.x, ball.y, ball.r, ball.color);
  spr.pushSprite(0, 0);
  
}



// --------- Loop ----------
void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  uint32_t dt_ms = now - last;
  if (dt_ms < FRAME_MS) {
    delay(FRAME_MS - dt_ms);
    now = millis();
    dt_ms = now - last;
  }

  last = now;

  if(triggered) {
    ball.vy -= gravity * 1; // give a little "kick" on trigger
    triggered = false;
  }

  // seconds
  float dt = dt_ms / 1000.0f;

  // --- Horizontal non-linear motion ---
  //   Create a nice eased "back and forth" between margins using ping-pong + easing.
  //   Adjust cycleSec to change horizontal period.
  static float tHoriz = 0.0f;
  const float cycleSec = 2.5f;        // time to go there-and-back
  tHoriz += dt / cycleSec;            // normalized time
  float u = pingPong01(tHoriz);       // 0..1..0
  float eased = easeInOutQuad(u);     // smooooth
  float left = 20 + ball.r;
  float right = W - 20 - ball.r;
  ball.x = map01(eased, left, right);

  // --- Vertical gravity + bounces (non-linear acceleration) ---
  ball.vy += gravity * dt;
  ball.y += ball.vy * dt;

  // Floor collision
  float bottom = floorY - ball.r;
  if (ball.y > bottom) {
    ball.y = bottom;
    ball.vy = -ball.vy*restitution; // bounce up with energy loss
    // Mild horizontal “kick” on impact to make it feel lively
    float kick = (random(-25, 26)) * 3.0f;
    ball.x = constrain(ball.x + kick * dt, left, right);
  }

  // Ceiling collision (rare with gravity, but keeps things tidy)
  float top = 20 + ball.r;
  if (ball.y < top) {
    ball.y = top;
    ball.vy = -ball.vy * restitution;
  }

  // --- Render ---
  drawBackground();
  drawBall(ball.x, ball.y, ball.r, ball.color);

  // Push to screen
  spr.pushSprite(0, 0);

  // Yield for WiFi/BLE housekeeping if enabled
  yield();
}