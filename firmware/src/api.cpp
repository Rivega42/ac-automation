#include "api.h"

#include <ArduinoJson.h>
#include "state.h"

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

void sendButtonResponse(AsyncWebServerRequest* request, ButtonAction action) {
  const bool queued = enqueueButtonAction(action);

  StaticJsonDocument<128> doc;
  doc["ok"] = queued;
  doc["action"] = actionToName(action);

  String payload;
  serializeJson(doc, payload);
  request->send(queued ? 200 : 503, "application/json", payload);
}

void registerButtonEndpoint(AsyncWebServer& server, const char* path, ButtonAction action) {
  server.on(path, HTTP_POST, [action](AsyncWebServerRequest* request) {
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

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildStateJson(snapshotState()));
  });

  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", buildSensorJson(snapshotState()));
  });
}
