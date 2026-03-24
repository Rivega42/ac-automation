#include "mqtt.h"

#include <ArduinoJson.h>
#include "config.h"

namespace {

AsyncMqttClient* s_client = nullptr;
bool s_wifiConnected = false;
bool s_mqttConnected = false;
uint32_t s_lastReconnectAttempt = 0;

String stateJson(const ACState& state) {
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

  String out;
  serializeJson(doc, out);
  return out;
}

void publishDiscovery() {
  if (!s_client || !s_mqttConnected) {
    return;
  }

  StaticJsonDocument<1024> climate;
  climate["name"] = "Mystery AC";
  climate["unique_id"] = "mystery_ac_001";
  JsonArray modes = climate["modes"].to<JsonArray>();
  modes.add("off");
  modes.add("auto");
  modes.add("cool");
  modes.add("fan_only");

  JsonArray fanModes = climate["fan_modes"].to<JsonArray>();
  fanModes.add("high");
  fanModes.add("low");

  climate["min_temp"] = 16;
  climate["max_temp"] = 30;
  climate["temp_step"] = 1;
  climate["mode_command_topic"] = String(MQTT_PREFIX) + "/set/mode";
  climate["temperature_command_topic"] = String(MQTT_PREFIX) + "/set/temp";
  climate["fan_mode_command_topic"] = String(MQTT_PREFIX) + "/set/speed";
  climate["mode_state_topic"] = String(MQTT_PREFIX) + "/state";
  climate["mode_state_template"] = "{{ value_json.mode }}";
  climate["current_temperature_topic"] = String(MQTT_PREFIX) + "/temperature";
  climate["availability_topic"] = String(MQTT_PREFIX) + "/available";

  JsonObject dev = climate["device"].to<JsonObject>();
  JsonArray ids = dev["identifiers"].to<JsonArray>();
  ids.add("mystery_ac");
  dev["name"] = "Mystery AC MSS-09R07M";
  dev["model"] = "MSS-09R07M";
  dev["manufacturer"] = "Mystery";

  String climatePayload;
  serializeJson(climate, climatePayload);
  s_client->publish("homeassistant/climate/mystery_ac/config", 0, true, climatePayload.c_str());

  StaticJsonDocument<512> tempSensor;
  tempSensor["name"] = "Mystery AC Temperature";
  tempSensor["unique_id"] = "mystery_ac_temperature";
  tempSensor["device_class"] = "temperature";
  tempSensor["unit_of_measurement"] = "°C";
  tempSensor["state_topic"] = String(MQTT_PREFIX) + "/temperature";
  tempSensor["availability_topic"] = String(MQTT_PREFIX) + "/available";
  tempSensor["device"] = dev;

  String tempPayload;
  serializeJson(tempSensor, tempPayload);
  s_client->publish("homeassistant/sensor/mystery_temp/config", 0, true, tempPayload.c_str());

  StaticJsonDocument<512> waterSensor;
  waterSensor["name"] = "Mystery AC Full Water";
  waterSensor["unique_id"] = "mystery_ac_full_water";
  waterSensor["device_class"] = "problem";
  waterSensor["state_topic"] = String(MQTT_PREFIX) + "/fullwater";
  waterSensor["payload_on"] = "true";
  waterSensor["payload_off"] = "false";
  waterSensor["availability_topic"] = String(MQTT_PREFIX) + "/available";
  waterSensor["device"] = dev;

  String waterPayload;
  serializeJson(waterSensor, waterPayload);
  s_client->publish("homeassistant/binary_sensor/mystery_water/config", 0, true, waterPayload.c_str());
}

void applySetPower(const String& value) {
  const ACState state = snapshotState();
  const bool wantOn = value.equalsIgnoreCase("ON");
  if (state.power != wantOn) {
    enqueueButtonAction(ButtonAction::Power);
  }
}

void applySetMode(const String& value) {
  if (value == "auto" || value == "cool" || value == "fan") {
    enqueueButtonAction(ButtonAction::Mode);
    if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_state.mode = value;
      xSemaphoreGive(g_stateMutex);
    }
    publishStateToOutputs();
  }
}

void applySetSpeed(const String& value) {
  if (value == "hi" || value == "low") {
    enqueueButtonAction(ButtonAction::Speed);
    if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      g_state.speed = value;
      xSemaphoreGive(g_stateMutex);
    }
    publishStateToOutputs();
  }
}

void applySetTemp(const String& value) {
  int requested = value.toInt();
  requested = constrain(requested, 16, 30);

  ACState state = snapshotState();
  int delta = requested - state.targetTemp;
  ButtonAction action = delta > 0 ? ButtonAction::TempUp : ButtonAction::TempDown;
  int presses = abs(delta);
  for (int i = 0; i < presses; ++i) {
    if (!enqueueButtonAction(action)) {
      break;
    }
  }
}

void onMqttConnect(bool sessionPresent) {
  (void)sessionPresent;
  s_mqttConnected = true;

  const String setPrefix = String(MQTT_PREFIX) + "/set/";
  const String buttonTopic = String(MQTT_PREFIX) + "/button";

  s_client->subscribe((setPrefix + "power").c_str(), 0);
  s_client->subscribe((setPrefix + "mode").c_str(), 0);
  s_client->subscribe((setPrefix + "temp").c_str(), 0);
  s_client->subscribe((setPrefix + "speed").c_str(), 0);
  s_client->subscribe(buttonTopic.c_str(), 0);

  s_client->publish((String(MQTT_PREFIX) + "/available").c_str(), 0, true, "online");
  publishDiscovery();
  mqttPublishState(snapshotState(), true);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  (void)reason;
  s_mqttConnected = false;
}

void onMqttMessage(char* topic,
                   char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len,
                   size_t index,
                   size_t total) {
  (void)properties;
  (void)index;
  (void)total;

  String topicStr(topic);
  String payloadStr;
  payloadStr.reserve(len + 1);
  for (size_t i = 0; i < len; ++i) {
    payloadStr += payload[i];
  }
  payloadStr.trim();

  if (topicStr.endsWith("/set/power")) {
    applySetPower(payloadStr);
  } else if (topicStr.endsWith("/set/mode")) {
    applySetMode(payloadStr);
  } else if (topicStr.endsWith("/set/temp")) {
    applySetTemp(payloadStr);
  } else if (topicStr.endsWith("/set/speed")) {
    applySetSpeed(payloadStr);
  } else if (topicStr.endsWith("/button")) {
    bool ok = false;
    const ButtonAction action = actionFromName(payloadStr, &ok);
    if (ok) {
      enqueueButtonAction(action);
    }
  }
}

}  // namespace

void setupMqtt(AsyncMqttClient& client) {
  s_client = &client;

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCredentials(MQTT_USER, MQTT_PASS);
  client.setWill((String(MQTT_PREFIX) + "/available").c_str(), 0, true, "offline");

  client.onConnect(onMqttConnect);
  client.onDisconnect(onMqttDisconnect);
  client.onMessage(onMqttMessage);
}

void mqttSetWifiConnected(bool connected) {
  s_wifiConnected = connected;
}

void mqttLoop() {
  if (!s_client || !s_wifiConnected || s_mqttConnected) {
    return;
  }

  const uint32_t now = millis();
  if (now - s_lastReconnectAttempt >= MQTT_RECONNECT_MS) {
    s_lastReconnectAttempt = now;
    s_client->connect();
  }
}

void mqttPublishState(const ACState& state, bool retain) {
  if (!s_client || !s_mqttConnected) {
    return;
  }

  const String payload = stateJson(state);
  s_client->publish((String(MQTT_PREFIX) + "/state").c_str(), 0, retain, payload.c_str());
}

void mqttPublishTelemetry(const ACState& state) {
  if (!s_client || !s_mqttConnected) {
    return;
  }

  char tempBuf[16] = {0};
  char humBuf[16] = {0};
  if (!isnan(state.roomTemp)) {
    dtostrf(state.roomTemp, 0, 1, tempBuf);
    s_client->publish((String(MQTT_PREFIX) + "/temperature").c_str(), 0, true, tempBuf);
  }

  if (!isnan(state.roomHumidity)) {
    dtostrf(state.roomHumidity, 0, 1, humBuf);
    s_client->publish((String(MQTT_PREFIX) + "/humidity").c_str(), 0, true, humBuf);
  }

  s_client->publish((String(MQTT_PREFIX) + "/fullwater").c_str(), 0, true, state.fullWater ? "true" : "false");
}
