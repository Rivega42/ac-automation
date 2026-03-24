#pragma once

#include <AsyncMqttClient.h>
#include "state.h"

void setupMqtt(AsyncMqttClient& client);
void mqttSetWifiConnected(bool connected);
void mqttLoop();
void mqttPublishState(const ACState& state, bool retain = true);
void mqttPublishTelemetry(const ACState& state);
void mqttPublishLog(const char* action, const char* source);
