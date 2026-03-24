#include "scheduler.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include "state.h"
#include "mqtt.h"

static constexpr char kNvsNs[]   = "scheduler";
static constexpr char kNtpServer[] = "pool.ntp.org";
static constexpr long kTzOffset   = 3 * 3600;  // UTC+3 (Москва)

static ScheduleSlot s_slots[SCHEDULER_MAX_SLOTS];
static bool s_ntpSynced = false;
static uint8_t s_lastTriggeredMin = 255;  // защита от двойного срабатывания

// ---- NVS ----------------------------------------------------------------

static void loadSlots() {
  Preferences prefs;
  if (!prefs.begin(kNvsNs, true)) return;

  for (uint8_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    const String pfx = String(i) + "_";
    s_slots[i].enabled    = prefs.getBool((pfx + "en").c_str(),   false);
    s_slots[i].hour       = prefs.getUChar((pfx + "h").c_str(),   0);
    s_slots[i].minute     = prefs.getUChar((pfx + "m").c_str(),   0);
    s_slots[i].power      = prefs.getBool((pfx + "pw").c_str(),   false);
    s_slots[i].mode       = prefs.getString((pfx + "mo").c_str(), "auto");
    s_slots[i].targetTemp = prefs.getUChar((pfx + "t").c_str(),   22);
    s_slots[i].days       = prefs.getUChar((pfx + "d").c_str(),   0x7F);
  }
  prefs.end();
}

static void saveSlot(uint8_t index) {
  if (index >= SCHEDULER_MAX_SLOTS) return;
  Preferences prefs;
  if (!prefs.begin(kNvsNs, false)) return;

  const String pfx = String(index) + "_";
  const ScheduleSlot& s = s_slots[index];
  prefs.putBool((pfx + "en").c_str(),  s.enabled);
  prefs.putUChar((pfx + "h").c_str(),  s.hour);
  prefs.putUChar((pfx + "m").c_str(),  s.minute);
  prefs.putBool((pfx + "pw").c_str(),  s.power);
  prefs.putString((pfx + "mo").c_str(), s.mode);
  prefs.putUChar((pfx + "t").c_str(),  s.targetTemp);
  prefs.putUChar((pfx + "d").c_str(),  s.days);
  prefs.end();
}

// ---- Публичные ----------------------------------------------------------

void schedulerSetup() {
  loadSlots();
  configTime(kTzOffset, 0, kNtpServer);
}

String schedulerGetTimeStr() {
  struct tm t;
  if (!getLocalTime(&t, 100)) return "";
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return String(buf);
}

void schedulerTick() {
  struct tm t;
  if (!getLocalTime(&t, 50)) return;
  s_ntpSynced = true;

  const uint8_t currentMin = t.tm_hour * 60 + t.tm_min;
  if (currentMin == s_lastTriggeredMin) return;  // уже сработало в эту минуту

  // bit0 = Пн (tm_wday: 0=Вс, 1=Пн ... 6=Сб)
  const uint8_t dayBit = (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);

  for (uint8_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    const ScheduleSlot& slot = s_slots[i];
    if (!slot.enabled) continue;
    if (!(slot.days & (1 << dayBit))) continue;
    if (slot.hour != (uint8_t)t.tm_hour) continue;
    if (slot.minute != (uint8_t)t.tm_min) continue;

    // Срабатывание
    s_lastTriggeredMin = currentMin;

    const ACState state = snapshotState();

    if (slot.power && !state.power) {
      enqueueButtonAction(ButtonAction::Power);
    } else if (!slot.power && state.power) {
      enqueueButtonAction(ButtonAction::Power);
    }

    if (slot.power) {
      // Устанавливаем режим и температуру через мьютекс
      if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state.mode = slot.mode;
        g_state.targetTemp = slot.targetTemp;
        xSemaphoreGive(g_stateMutex);
      }
      publishStateToOutputs();
    }

    mqttPublishLog("schedule_trigger", "scheduler");
    break;  // только первое совпадение в одну минуту
  }
}

ScheduleSlot schedulerGetSlot(uint8_t index) {
  if (index >= SCHEDULER_MAX_SLOTS) return {};
  return s_slots[index];
}

void schedulerSetSlot(uint8_t index, const ScheduleSlot& slot) {
  if (index >= SCHEDULER_MAX_SLOTS) return;
  s_slots[index] = slot;
  saveSlot(index);
}
