#pragma once

#include <Arduino.h>

// Wi-Fi — задаётся в credentials.h (не коммитить!)
#if __has_include("credentials.h")
#  include "credentials.h"
#else
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_HOST[] = "192.168.1.100";
constexpr char MQTT_USER[] = "mqtt_user";
constexpr char MQTT_PASS[] = "mqtt_password";
#endif

constexpr uint16_t MQTT_PORT = 1883;
constexpr char DEVICE_HOSTNAME[] = "ac-control";
constexpr char MQTT_PREFIX[] = "home/ac/mystery";

// GPIO map — ESP32-CAM (docs/ТЗ.md §2.2)
// Кнопки → PC817 оптопара → HIGH = нажать
constexpr uint8_t PIN_POWER    =  2;
constexpr uint8_t PIN_MODE     =  4;
constexpr uint8_t PIN_SPEED    = 12;
constexpr uint8_t PIN_TIMER    = 13;
constexpr uint8_t PIN_SLEEP    = 14;
constexpr uint8_t PIN_TEMP_UP  = 15;
constexpr uint8_t PIN_TEMP_DOWN = 16;

// DHT22 — GPIO3 (RX0) конфликт с Serial!
// Используем GPIO1 (TX0) только если Serial отключён,
// иначе перенести на свободный пин (напр. GPIO33 или GPIO0).
// ВНИМАНИЕ: уточнить физическое подключение перед прошивкой!
constexpr uint8_t PIN_DHT = 3;

constexpr uint8_t PIN_IND_COMP = 26;
constexpr uint8_t PIN_IND_AUTO = 32;
constexpr uint8_t PIN_IND_COOL = 33;
constexpr uint8_t PIN_IND_FAN = 34;
constexpr uint8_t PIN_IND_HI = 35;
constexpr uint8_t PIN_IND_LOW = 36;
constexpr uint8_t PIN_IND_FULL_WATER = 39;

constexpr uint16_t PRESS_DURATION_MS = 150;
constexpr uint16_t PRESS_POST_DELAY_MS = 300;
constexpr uint16_t PRESS_MIN_INTERVAL_MS = 500;

constexpr uint32_t SENSOR_UPDATE_MS = 30000;
constexpr uint32_t INDICATOR_UPDATE_MS = 2000;
constexpr uint32_t WIFI_RECONNECT_MS = 10000;
constexpr uint32_t MQTT_RECONNECT_MS = 5000;
