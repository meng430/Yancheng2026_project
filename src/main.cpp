#include <Arduino.h>
#include <FastLED.h>

// --- 設定區 ---
#define NUM_LEDS     30    // 每條燈條的燈珠數量
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

// ESP32 引腳設定 (請根據你的實際接線修改)
const int LED_PINS[] = {13, 14, 27, 26}; 
const int BTN_PINS[] = {5, 18, 19, 21};

// 建立 4 條燈條的記憶體空間
CRGB leds[4][NUM_LEDS];

void setup() {
    Serial.begin(115200);

    // 初始化 4 條燈條
    FastLED.addLeds<LED_TYPE, 13, COLOR_ORDER>(leds[0], NUM_LEDS);
    FastLED.addLeds<LED_TYPE, 14, COLOR_ORDER>(leds[1], NUM_LEDS);
    FastLED.addLeds<LED_TYPE, 27, COLOR_ORDER>(leds[2], NUM_LEDS);
    FastLED.addLeds<LED_TYPE, 26, COLOR_ORDER>(leds[3], NUM_LEDS);

    // 初始化按鈕 (使用上拉電阻)
    for (int i = 0; i < 4; i++) {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }
}

void loop() {
    bool anyButtonPressed = false;

    // 檢查是否有任何按鈕被按下 (LOW 表示按下)
    for (int i = 0; i < 4; i++) {
        if (digitalRead(BTN_PINS[i]) == LOW) {
            anyButtonPressed = true;
            // 這裡可以加入按鈕觸發後的特定邏輯
            // 例如：fill_solid(leds[i], NUM_LEDS, CRGB::Blue); 
        }
    }

    if (!anyButtonPressed) {
        // --- 純白色呼吸燈邏輯 ---
        // beatsin8(每分鐘呼吸次數, 最小亮度, 最大亮度)
        uint8_t brightness = beatsin8(15, 20, 200); 

        for (int i = 0; i < 4; i++) {
            // 將整條燈條設為白色，並套用當前呼吸亮度
            fill_solid(leds[i], NUM_LEDS, CRGB::White);
        }
        FastLED.setBrightness(brightness);
    } else {
        // 如果有按鈕按下，亮度恢復正常或自訂
        FastLED.setBrightness(255);
    }

    FastLED.show();
    delay(10); // 稍微延遲讓系統穩定
}