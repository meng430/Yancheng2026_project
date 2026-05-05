#include <Arduino.h>
#include <FastLED.h>

// ============================================================
// 硬體腳位設定 (依實際接線修改)
// ============================================================
#define NUM_LEDS     300         // 每條燈條的 LED 數量
#define NUM_STRIPS   4           // 燈條數量

#define LED_PIN_1    2
#define LED_PIN_2    4
#define LED_PIN_3    16
#define LED_PIN_4    17

#define BTN_PIN_1    32
#define BTN_PIN_2    33
#define BTN_PIN_3    25
#define BTN_PIN_4    26

#define BRIGHTNESS_MAX 200       // 整體最大亮度 (0-255)，過亮會過熱

// 暖白色 (R, G, B)
#define WARM_WHITE   CRGB(255, 147, 41)

// ============================================================
// 全域變數
// ============================================================
CRGB leds[NUM_STRIPS][NUM_LEDS]; //四條LED燈上所有燈泡的顏色狀態
const uint8_t btnPins[NUM_STRIPS] = {BTN_PIN_1, BTN_PIN_2, BTN_PIN_3, BTN_PIN_4}; //四個按鈕的腳位


// 按鈕觸發狀態 (按住才生效: 放開即關閉)
bool btnActive[NUM_STRIPS]      = {false, false, false, false};
int  lastBtnReading[NUM_STRIPS] = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[NUM_STRIPS] = {0, 0, 0, 0};
const unsigned long DEBOUNCE_MS = 25;

// 模式變化追蹤 (用來啟動轉場動畫)
int  lastActiveCount = -1;
unsigned long modeStartMs = 0;
bool retractFinished = false;     // mode 1 收尾動畫是否完成

// ============================================================
// 工具函式
// ============================================================
void clearAll() {
  for (int s = 0; s < NUM_STRIPS; s++) {
    fill_solid(leds[s], NUM_LEDS, CRGB::Black);
  }
}

int countActive() {
  int n = 0;
  for (int s = 0; s < NUM_STRIPS; s++) if (btnActive[s]) n++;
  return n;
}

// ============================================================
// 按鈕讀取 (toggle + debounce)
// ============================================================
void readButtons() {
  static int stableState[NUM_STRIPS] = {HIGH, HIGH, HIGH, HIGH};
  for (int i = 0; i < NUM_STRIPS; i++) {
    int reading = digitalRead(btnPins[i]);
    if (reading != lastBtnReading[i]) {
      lastDebounceTime[i] = millis();
    }
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_MS) {
      stableState[i] = reading;
      // INPUT_PULLUP: LOW = 按住中 = 觸發
      btnActive[i] = (stableState[i] == LOW);
    }
    lastBtnReading[i] = reading;
  }
}

// ============================================================
// 模式 0: 暖白色呼吸燈 (四條同步)
// ============================================================
void modeBreathing() {
  // 呼吸亮度: 約 4 秒一個週期，30~255
  uint8_t b = beatsin8(15, 30, 255);
  CRGB c = WARM_WHITE;
  c.nscale8(b);
  for (int s = 0; s < NUM_STRIPS; s++) {
    fill_solid(leds[s], NUM_LEDS, c);
  }
}

// ============================================================
// 模式 1: 收回 + 單條從第 1 顆跑到第 300 顆
// ============================================================
void modeOne(int activeStrip) {
  unsigned long elapsed = millis() - modeStartMs;
  const unsigned long RETRACT_MS = 1200;   // 收回動畫時間

  if (elapsed < RETRACT_MS) {
    // 階段 A: 四條從末端收回 (剩餘長度 = NUM_LEDS * (1 - t))
    float t = (float)elapsed / (float)RETRACT_MS;
    int remain = (int)(NUM_LEDS * (1.0f - t));
    if (remain < 0) remain = 0;

    // 維持暖白 (此時不再呼吸，固定亮度)
    CRGB c = WARM_WHITE;
    c.nscale8(220);

    for (int s = 0; s < NUM_STRIPS; s++) {
      fill_solid(leds[s], NUM_LEDS, CRGB::Black);
      for (int i = 0; i < remain; i++) {
        leds[s][i] = c;
      }
    }
    retractFinished = false;
  } else {
    // 階段 B: 只剩被觸發那條，從 0 跑到 NUM_LEDS-1
    if (!retractFinished) {
      retractFinished = true;
      // 重設此階段的起算時間
      modeStartMs = millis();
    }
    unsigned long t2 = millis() - modeStartMs;
    const unsigned long FILL_MS = 3000;     // 跑完 300 顆要花多久
    int progress = (int)((float)t2 / (float)FILL_MS * NUM_LEDS);
    if (progress >= NUM_LEDS) progress = NUM_LEDS;

    clearAll();
    if (activeStrip >= 0) {
      CRGB c = WARM_WHITE;
      for (int i = 0; i < progress; i++) {
        leds[activeStrip][i] = c;
      }
      // 跑完後維持全亮
      if (progress >= NUM_LEDS) {
        // 用呼吸維持「跑完」的視覺
        uint8_t b = beatsin8(20, 120, 255);
        CRGB hold = WARM_WHITE;
        hold.nscale8(b);
        fill_solid(leds[activeStrip], NUM_LEDS, hold);
      }
    }
  }
}

// ============================================================
// 模式 2: 兩條 — 不規則亮暗暗亮
// ============================================================
void modeTwo(const int *activeStrips, int n) {
  // 用 noise 製造不規則閃爍
  uint16_t timeBase = millis() / 30;
  for (int k = 0; k < n; k++) {
    int s = activeStrips[k];
    for (int i = 0; i < NUM_LEDS; i++) {
      // 不同條給不同 z 軸，效果不同
      uint8_t v = inoise8(i * 18, timeBase, k * 2000);
      // 強化對比 (亮、暗、暗、亮)
      if (v > 180)      v = 255;
      else if (v < 90)  v = 0;
      else              v = v / 3;     // 中段壓暗
      CRGB c = WARM_WHITE;
      c.nscale8(v);
      leds[s][i] = c;
    }
  }
  // 沒被觸發的條: 暗
  for (int s = 0; s < NUM_STRIPS; s++) {
    bool active = false;
    for (int k = 0; k < n; k++) if (activeStrips[k] == s) { active = true; break; }
    if (!active) fill_solid(leds[s], NUM_LEDS, CRGB::Black);
  }
}

// ============================================================
// 模式 3: 三條 — 拖尾燈 (彗星) 大約 5 顆一組流過去
// ============================================================
void modeThree(const int *activeStrips, int n) {
  const int COMET_LEN = 5;
  const int SPEED_MS  = 12;            // 每顆移動的時間
  // 先讓殘影衰減
  for (int k = 0; k < n; k++) {
    int s = activeStrips[k];
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[s][i].nscale8(220);          // 拖尾衰減速度
    }
  }

  unsigned long t = millis();
  for (int k = 0; k < n; k++) {
    int s = activeStrips[k];
    // 每條彗星位置略有錯位
    int head = ((t / SPEED_MS) + k * 80) % (NUM_LEDS + COMET_LEN * 4);
    for (int j = 0; j < COMET_LEN; j++) {
      int p = head - j;
      if (p >= 0 && p < NUM_LEDS) {
        // 頭最亮、尾較暗
        uint8_t scale = 255 - (j * (200 / COMET_LEN));
        CRGB c = WARM_WHITE;
        c.nscale8(scale);
        leds[s][p] += c;
      }
    }
  }
  // 沒被觸發的條: 暗
  for (int s = 0; s < NUM_STRIPS; s++) {
    bool active = false;
    for (int k = 0; k < n; k++) if (activeStrips[k] == s) { active = true; break; }
    if (!active) fill_solid(leds[s], NUM_LEDS, CRGB::Black);
  }
}

// ============================================================
// 模式 4: 四顆全按 — 華麗炫酷 (彩虹波 + 閃爍 + 對撞)
// ============================================================
void modeFour() {
  uint32_t t = millis();
  uint8_t  hueBase = t / 12;

  for (int s = 0; s < NUM_STRIPS; s++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      // 1) 彩虹流動
      uint8_t hue = hueBase + i * 2 + s * 40;
      // 2) 正弦亮度波 (每條相位不同)
      uint8_t bri = sin8((i * 6) + (t / 4) + s * 64);
      // 3) 對撞效果: 從兩端往中間掃光
      int mid = NUM_LEDS / 2;
      int wavePos = (t / 6) % NUM_LEDS;
      int distFromWave = abs(i - wavePos);
      int distFromMirror = abs((NUM_LEDS - 1 - i) - wavePos);
      uint8_t add = 0;
      if (distFromWave < 8)   add = 255 - distFromWave * 30;
      if (distFromMirror < 8) add = max((int)add, 255 - distFromMirror * 30);

      CRGB c = CHSV(hue, 240, qadd8(bri, add));
      leds[s][i] = c;
    }
  }

  // 隨機白色閃爍 (sparkle)
  if (random8() < 80) {
    int s = random8(NUM_STRIPS);
    int i = random16(NUM_LEDS);
    leds[s][i] = CRGB::White;
  }
}

// ============================================================
// setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);

  // FastLED 註冊四條燈條 (RGB 順序若顏色不對請改成 RGB / BRG 等)
  FastLED.addLeds<WS2812B, LED_PIN_1, GRB>(leds[0], NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_PIN_2, GRB>(leds[1], NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_PIN_3, GRB>(leds[2], NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_PIN_4, GRB>(leds[3], NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_MAX);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 4000);   // 限流保護: 5V 4A

  // 按鈕: 一端接腳位、一端接 GND，使用內部上拉
  for (int i = 0; i < NUM_STRIPS; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }

  clearAll();
  FastLED.show();
  modeStartMs = millis();
}

void loop() {
  readButtons();
  int active = countActive();

  // 模式切換偵測 — 重設動畫起算時間
  if (active != lastActiveCount) {
    lastActiveCount   = active;
    modeStartMs       = millis();
    retractFinished   = false;
    clearAll();
  }

  // 整理目前哪些條被觸發
  int activeStrips[NUM_STRIPS];
  int idx = 0;
  for (int s = 0; s < NUM_STRIPS; s++) {
    if (btnActive[s]) activeStrips[idx++] = s;
  }

  switch (active) {
    case 0: modeBreathing();                           break;
    case 1: modeOne(activeStrips[0]);                  break;
    case 2: modeTwo(activeStrips, 2);                  break;
    case 3: modeThree(activeStrips, 3);                break;
    case 4: modeFour();                                break;
  }

  FastLED.show();
  FastLED.delay(8);   // ~120 FPS 上限
}
