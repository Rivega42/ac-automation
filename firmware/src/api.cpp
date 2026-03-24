#include "api.h"

#include <ArduinoJson.h>
#include "state.h"
#include "credentials.h"
#include "mqtt.h"

namespace {

String buildStateJson(const ACState& state) {
  StaticJsonDocument<512> doc;
  doc["power"] = state.power;
  doc["mode"] = state.mode;
  doc["speed"] = state.speed;
  doc["targetTemp"] = state.targetTemp;
  doc["timerActive"] = state.timerActive;
  doc["sleepMode"] = state.sleepMode;

  if (isnan(state.roomTemp)) {
    doc["roomTemp"] = nullptr;
  } else {
    doc["roomTemp"] = state.roomTemp;
  }

  if (isnan(state.roomHumidity)) {
    doc["roomHumidity"] = nullptr;
  } else {
    doc["roomHumidity"] = state.roomHumidity;
  }

  doc["fullWater"] = state.fullWater;
  doc["compRunning"] = state.compRunning;

  String json;
  serializeJson(doc, json);
  return json;
}

String buildSensorJson(const ACState& state) {
  StaticJsonDocument<128> doc;
  if (isnan(state.roomTemp)) {
    doc["temperature"] = nullptr;
  } else {
    doc["temperature"] = state.roomTemp;
  }

  if (isnan(state.roomHumidity)) {
    doc["humidity"] = nullptr;
  } else {
    doc["humidity"] = state.roomHumidity;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

// Debounce: игнорируем повторное нажатие той же кнопки < 600 мс
static uint32_t lastPressTime[7] = {};
static constexpr uint16_t DEBOUNCE_MS = 600;

void sendButtonResponse(AsyncWebServerRequest* request, ButtonAction action) {
  const uint8_t idx = static_cast<uint8_t>(action);
  const uint32_t now = millis();

  if (now - lastPressTime[idx] < DEBOUNCE_MS) {
    StaticJsonDocument<128> doc;
    doc["ok"] = false;
    doc["action"] = actionToName(action);
    doc["reason"] = "debounce";
    String payload; serializeJson(doc, payload);
    request->send(429, "application/json", payload);
    return;
  }
  lastPressTime[idx] = now;

  const bool queued = enqueueButtonAction(action);
  if (queued) {
    mqttPublishLog(actionToName(action), "web");
  }

  StaticJsonDocument<128> doc;
  doc["ok"] = queued;
  doc["action"] = actionToName(action);

  String payload;
  serializeJson(doc, payload);
  request->send(queued ? 200 : 503, "application/json", payload);
}

// Проверка Basic Auth — применяется ко всем маршрутам кроме /api/state
static bool checkAuth(AsyncWebServerRequest* request) {
  if (!request->authenticate(WEB_USER, WEB_PASS)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

void registerButtonEndpoint(AsyncWebServer& server, const char* path, ButtonAction action) {
  server.on(path, HTTP_POST, [action](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) return;
    sendButtonResponse(request, action);
  });
}

}  // namespace

void setupApi(AsyncWebServer& server) {
  registerButtonEndpoint(server, "/api/button/power", ButtonAction::Power);
  registerButtonEndpoint(server, "/api/button/mode", ButtonAction::Mode);
  registerButtonEndpoint(server, "/api/button/speed", ButtonAction::Speed);
  registerButtonEndpoint(server, "/api/button/timer", ButtonAction::Timer);
  registerButtonEndpoint(server, "/api/button/sleep", ButtonAction::Sleep);
  registerButtonEndpoint(server, "/api/button/temp_up", ButtonAction::TempUp);
  registerButtonEndpoint(server, "/api/button/temp_down", ButtonAction::TempDown);

  // /api/state — без авторизации (для MQTT-мониторинга и Home Assistant)
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildStateJson(snapshotState()));
  });

  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildSensorJson(snapshotState()));
  });
}
