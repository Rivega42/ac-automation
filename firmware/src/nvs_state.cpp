#include "nvs_state.h"

#include <Preferences.h>
#include "state.h"

// Namespace в NVS — макс 15 символов
static constexpr char kNvsNamespace[] = "ac-state";

void nvsRestoreState() {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/true)) {
    // NVS пустой при первом запуске — оставляем дефолтные значения
    return;
  }

  if (xSemaphoreTake(g_stateMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    prefs.end();
    return;
  }

  g_state.power       = prefs.getBool("power",       false);
  g_state.mode        = prefs.getString("mode",       "auto");
  g_state.speed       = prefs.getString("speed",      "hi");
  g_state.targetTemp  = prefs.getInt("targetTemp",    22);
  g_state.sleepMode   = prefs.getBool("sleepMode",    false);
  g_state.timerActive = prefs.getBool("timerActive",  false);

  // Клампим targetTemp на случай мусора в NVS
  if (g_state.targetTemp < 16 || g_state.targetTemp > 30) {
    g_state.targetTemp = 22;
  }

  xSemaphoreGive(g_stateMutex);
  prefs.end();
}

// Кэш последнего сохранённого состояния — пишем в NVS только при реальных изменениях
// для снижения износа flash (ESP32 NVS ~100K циклов записи)
static bool     s_savedPower      = false;
static String   s_savedMode       = "auto";
static String   s_savedSpeed      = "hi";
static int      s_savedTargetTemp = 22;
static bool     s_savedSleepMode  = false;
static bool     s_savedTimerActive = false;

void nvsSaveState(const ACState& state) {
  // Проверяем изменились ли персистентные поля
  if (state.power == s_savedPower &&
      state.mode == s_savedMode &&
      state.speed == s_savedSpeed &&
      state.targetTemp == s_savedTargetTemp &&
      state.sleepMode == s_savedSleepMode &&
      state.timerActive == s_savedTimerActive) {
    return;  // ничего не изменилось — не трогаем flash
  }

  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) {
    return;
  }

  prefs.putBool("power",       state.power);
  prefs.putString("mode",      state.mode);
  prefs.putString("speed",     state.speed);
  prefs.putInt("targetTemp",   state.targetTemp);
  prefs.putBool("sleepMode",   state.sleepMode);
  prefs.putBool("timerActive", state.timerActive);

  prefs.end();

  // Обновляем кэш
  s_savedPower       = state.power;
  s_savedMode        = state.mode;
  s_savedSpeed       = state.speed;
  s_savedTargetTemp  = state.targetTemp;
  s_savedSleepMode   = state.sleepMode;
  s_savedTimerActive = state.timerActive;
}
