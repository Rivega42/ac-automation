#pragma once
// nvs_state.h — сохранение/восстановление ACState в NVS (Non-Volatile Storage)
// При перезагрузке ESP32 состояние восстанавливается из флеша.

#include "state.h"

// Восстановить состояние из NVS. Вызывать в setup() после инициализации мьютекса.
void nvsRestoreState();

// Сохранить текущее состояние в NVS. Вызывать при каждом изменении.
void nvsSaveState(const ACState& state);
