#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

struct ACState {
  bool power = false;
  String mode = "auto";       // auto | cool | fan
  String speed = "hi";        // hi | low
  int targetTemp = 22;         // 16..30
  bool timerActive = false;
  bool sleepMode = false;
  float roomTemp = NAN;
  float roomHumidity = NAN;
  bool fullWater = false;
  bool compRunning = false;
};

enum class ButtonAction : uint8_t {
  Power,
  Mode,
  Speed,
  Timer,
  Sleep,
  TempUp,
  TempDown
};

struct ButtonPress {
  ButtonAction action;
  uint8_t pin;
  uint16_t durationMs;
};

extern ACState g_state;
extern QueueHandle_t g_buttonQueue;
extern SemaphoreHandle_t g_stateMutex;

bool enqueueButtonAction(ButtonAction action);
ACState snapshotState();
void publishStateToOutputs();
uint8_t pinForAction(ButtonAction action);
const char* actionToName(ButtonAction action);
ButtonAction actionFromName(const String& name, bool* ok = nullptr);
