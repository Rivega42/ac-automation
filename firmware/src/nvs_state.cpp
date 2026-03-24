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

void nvsSaveState(const ACState& state) {
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
}
