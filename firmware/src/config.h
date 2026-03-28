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

// Basic Auth для веб-интерфейса
constexpr char WEB_USER[] = "admin";
constexpr char WEB_PASS[] = "admin";

// GPIO map — ESP32-CAM
// Кнопки → PC817 оптопара → HIGH = нажать
constexpr uint8_t PIN_POWER     =  2;
constexpr uint8_t PIN_MODE      =  4;
constexpr uint8_t PIN_SPEED     = 12;
constexpr uint8_t PIN_TIMER     = 13;
constexpr uint8_t PIN_SLEEP     = 14;
constexpr uint8_t PIN_TEMP_UP   = 15;
constexpr uint8_t PIN_TEMP_DOWN = 16;

// DHT22 — GPIO3 (RX0, но Serial не используется в production)
// Если нужен Serial для отладки — переключи на GPIO33
constexpr uint8_t PIN_DHT = 3;

// 74HC164 сдвиговый регистр — чтение состояния LED с платы кондея
// ESP32 подключается параллельно к DATA и CLK линиям CN1 (только чтение!)
// DATA — серийные данные от главной платы к 74HC164
// CLK  — тактовый сигнал (ISR срабатывает по нарастающему фронту)
constexpr uint8_t PIN_SR_DATA = 33;  // GPIO33 — вход DATA от CN1
constexpr uint8_t PIN_SR_CLK  = 32;  // GPIO32 — вход CLK от CN1

// Количество бит для захвата (два 74HC164 в цепочке = 16 бит)
// Бит 0-7: первый регистр (COOL, FAN, HEAT, HI, LOW, COMP, SLEEP, AUTO)
// Бит 8-15: второй регистр (TIME, W/F, сегменты дисплея)
constexpr uint8_t SR_BIT_COUNT = 16;

// Маски бит для LED (уточнить после прозвонки CN1!)
constexpr uint16_t SR_MASK_AUTO       = (1 << 0);
constexpr uint16_t SR_MASK_COOL       = (1 << 1);
constexpr uint16_t SR_MASK_FAN        = (1 << 2);
constexpr uint16_t SR_MASK_HEAT       = (1 << 3);
constexpr uint16_t SR_MASK_HI         = (1 << 4);
constexpr uint16_t SR_MASK_LOW        = (1 << 5);
constexpr uint16_t SR_MASK_COMP       = (1 << 6);
constexpr uint16_t SR_MASK_SLEEP      = (1 << 7);
constexpr uint16_t SR_MASK_TIME       = (1 << 8);
constexpr uint16_t SR_MASK_FULL_WATER = (1 << 9);

constexpr uint16_t PRESS_DURATION_MS    = 150;
constexpr uint16_t PRESS_POST_DELAY_MS  = 300;
constexpr uint16_t PRESS_MIN_INTERVAL_MS = 500;

constexpr uint32_t SENSOR_UPDATE_MS    = 30000;
constexpr uint32_t INDICATOR_UPDATE_MS =  2000;
constexpr uint32_t WIFI_RECONNECT_MS   = 10000;
constexpr uint32_t MQTT_RECONNECT_MS   =  5000;
