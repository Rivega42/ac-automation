#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <AsyncMqttClient.h>

#include "config.h"
#include "state.h"
#include "api.h"
#include "mqtt.h"
#include "nvs_state.h"

ACState g_state;
QueueHandle_t g_buttonQueue = nullptr;
SemaphoreHandle_t g_stateMutex = nullptr;

namespace {

AsyncWebServer server(80);
AsyncMqttClient mqttClient;
DHT dht(PIN_DHT, DHT22);

uint32_t lastSensorUpdate = 0;
uint32_t lastIndicatorUpdate = 0;
uint32_t lastButtonExecution = 0;
bool mdnsStarted = false;

const uint8_t kButtonPins[] = {
  PIN_POWER,
  PIN_MODE,
  PIN_SLEEP,
  PIN_SPEED,
  PIN_TIMER,
  PIN_TEMP_UP,
  PIN_TEMP_DOWN,
};

const uint8_t kIndicatorPins[] = {
  PIN_IND_COMP,
  PIN_IND_AUTO,
  PIN_IND_COOL,
  PIN_IND_FAN,
  PIN_IND_HI,
  PIN_IND_LOW,
  PIN_IND_FULL_WATER,
};

void applyActionToState(ButtonAction action) {
  if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  switch (action) {
    case ButtonAction::Power:
      g_state.power = !g_state.power;
      break;
    case ButtonAction::Mode:
      if (g_state.mode == "auto") {
        g_state.mode = "cool";
      } else if (g_state.mode == "cool") {
        g_state.mode = "fan";
      } else {
        g_state.mode = "auto";
      }
      break;
    case ButtonAction::Speed:
      g_state.speed = g_state.speed == "hi" ? "low" : "hi";
      break;
    case ButtonAction::Timer:
      g_state.timerActive = !g_state.timerActive;
      break;
    case ButtonAction::Sleep:
      g_state.sleepMode = !g_state.sleepMode;
      break;
    case ButtonAction::TempUp:
      g_state.targetTemp = min(g_state.targetTemp + 1, 30);
      break;
    case ButtonAction::TempDown:
      g_state.targetTemp = max(g_state.targetTemp - 1, 16);
      break;
  }

  xSemaphoreGive(g_stateMutex);
}

void buttonTask(void* param) {
  (void)param;

  ButtonPress press{};
  while (true) {
    if (xQueueReceive(g_buttonQueue, &press, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    const uint32_t now = millis();
    const uint32_t elapsed = now - lastButtonExecution;
    if (elapsed < PRESS_MIN_INTERVAL_MS) {
      vTaskDelay(pdMS_TO_TICKS(PRESS_MIN_INTERVAL_MS - elapsed));
    }

    digitalWrite(press.pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(press.durationMs));
    digitalWrite(press.pin, LOW);
    vTaskDelay(pdMS_TO_TICKS(PRESS_POST_DELAY_MS));

    lastButtonExecution = millis();
    applyActionToState(press.action);
    publishStateToOutputs();
  }
}

void setupPins() {
  for (uint8_t pin : kButtonPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  for (uint8_t pin : kIndicatorPins) {
    pinMode(pin, INPUT);
  }
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t deadline = millis() + WIFI_RECONNECT_MS;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(250);
  }

  mqttSetWifiConnected(WiFi.status() == WL_CONNECTED);
}

void setupMdns() {
  if (!mdnsStarted && WiFi.status() == WL_CONNECTED) {
    mdnsStarted = MDNS.begin(DEVICE_HOSTNAME);
  }
}

void setupOta() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.begin();
}

void setupFiles() {
  LittleFS.begin(true);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

bool updateIndicators() {
  bool changed = false;

  const bool autoMode = digitalRead(PIN_IND_AUTO) == HIGH;
  const bool coolMode = digitalRead(PIN_IND_COOL) == HIGH;
  const bool fanMode = digitalRead(PIN_IND_FAN) == HIGH;
  const bool speedHi = digitalRead(PIN_IND_HI) == HIGH;
  const bool speedLow = digitalRead(PIN_IND_LOW) == HIGH;
  const bool fullWater = digitalRead(PIN_IND_FULL_WATER) == HIGH;
  const bool comp = digitalRead(PIN_IND_COMP) == HIGH;

  if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  String newMode = g_state.mode;
  if (autoMode) {
    newMode = "auto";
  } else if (coolMode) {
    newMode = "cool";
  } else if (fanMode) {
    newMode = "fan";
  }

  String newSpeed = g_state.speed;
  if (speedHi) {
    newSpeed = "hi";
  } else if (speedLow) {
    newSpeed = "low";
  }

  if (g_state.mode != newMode) {
    g_state.mode = newMode;
    changed = true;
  }

  if (g_state.speed != newSpeed) {
    g_state.speed = newSpeed;
    changed = true;
  }

  if (g_state.fullWater != fullWater) {
    g_state.fullWater = fullWater;
    changed = true;
  }

  if (g_state.compRunning != comp) {
    g_state.compRunning = comp;
    changed = true;
  }

  xSemaphoreGive(g_stateMutex);
  return changed;
}

bool updateSensor() {
  const float t = dht.readTemperature();
  const float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    return false;
  }

  bool changed = false;
  if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (isnan(g_state.roomTemp) || fabs(g_state.roomTemp - t) > 0.01f) {
      g_state.roomTemp = t;
      changed = true;
    }

    if (isnan(g_state.roomHumidity) || fabs(g_state.roomHumidity - h) > 0.01f) {
      g_state.roomHumidity = h;
      changed = true;
    }

    xSemaphoreGive(g_stateMutex);
  }

  return changed;
}

}  // namespace

uint8_t pinForAction(ButtonAction action) {
  switch (action) {
    case ButtonAction::Power:
      return PIN_POWER;
    case ButtonAction::Mode:
      return PIN_MODE;
    case ButtonAction::Speed:
      return PIN_SPEED;
    case ButtonAction::Timer:
      return PIN_TIMER;
    case ButtonAction::Sleep:
      return PIN_SLEEP;
    case ButtonAction::TempUp:
      return PIN_TEMP_UP;
    case ButtonAction::TempDown:
      return PIN_TEMP_DOWN;
  }
  return PIN_POWER;
}

const char* actionToName(ButtonAction action) {
  switch (action) {
    case ButtonAction::Power:
      return "power";
    case ButtonAction::Mode:
      return "mode";
    case ButtonAction::Speed:
      return "speed";
    case ButtonAction::Timer:
      return "timer";
    case ButtonAction::Sleep:
      return "sleep";
    case ButtonAction::TempUp:
      return "temp_up";
    case ButtonAction::TempDown:
      return "temp_down";
  }
  return "power";
}

ButtonAction actionFromName(const String& name, bool* ok) {
  if (ok) {
    *ok = true;
  }

  if (name == "power") return ButtonAction::Power;
  if (name == "mode") return ButtonAction::Mode;
  if (name == "speed") return ButtonAction::Speed;
  if (name == "timer") return ButtonAction::Timer;
  if (name == "sleep") return ButtonAction::Sleep;
  if (name == "temp_up") return ButtonAction::TempUp;
  if (name == "temp_down") return ButtonAction::TempDown;

  if (ok) {
    *ok = false;
  }
  return ButtonAction::Power;
}

bool enqueueButtonAction(ButtonAction action) {
  if (!g_buttonQueue) {
    return false;
  }

  ButtonPress press{};
  press.action = action;
  press.pin = pinForAction(action);
  press.durationMs = PRESS_DURATION_MS;

  return xQueueSend(g_buttonQueue, &press, 0) == pdTRUE;
}

ACState snapshotState() {
  ACState out;
  if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    out = g_state;
    xSemaphoreGive(g_stateMutex);
  }
  return out;
}

void publishStateToOutputs() {
  const ACState state = snapshotState();
  nvsSaveState(state);  // сохраняем в NVS при каждом изменении
  mqttPublishState(state, true);
  mqttPublishTelemetry(state);
}

void setup() {
  Serial.begin(115200);

  g_stateMutex = xSemaphoreCreateMutex();
  g_buttonQueue = xQueueCreate(16, sizeof(ButtonPress));

  // Восстанавливаем состояние из NVS до инициализации периферии
  nvsRestoreState();

  setupPins();
  dht.begin();

  setupWifi();
  setupMdns();
  setupOta();

  setupMqtt(mqttClient);
  setupApi(server);
  setupFiles();
  server.begin();

  xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 4096, nullptr, 1, nullptr, 1);

  publishStateToOutputs();
}

void loop() {
  ArduinoOTA.handle();

  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  mqttSetWifiConnected(wifiOk);

  if (!wifiOk) {
    static uint32_t lastWifiRetry = 0;
    const uint32_t now = millis();
    if (now - lastWifiRetry > WIFI_RECONNECT_MS) {
      lastWifiRetry = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  } else {
    setupMdns();
  }

  mqttLoop();

  const uint32_t now = millis();
  if (now - lastIndicatorUpdate >= INDICATOR_UPDATE_MS) {
    lastIndicatorUpdate = now;
    if (updateIndicators()) {
      publishStateToOutputs();
    }
  }

  if (now - lastSensorUpdate >= SENSOR_UPDATE_MS) {
    lastSensorUpdate = now;
    if (updateSensor()) {
      publishStateToOutputs();
    }
  }

  delay(5);
}
