#pragma once
// scheduler.h — расписание включения/выключения кондея по времени (NTP)

#include <Arduino.h>

// Макс. количество слотов расписания
constexpr uint8_t SCHEDULER_MAX_SLOTS = 4;

struct ScheduleSlot {
  bool     enabled    = false;
  uint8_t  hour       = 0;     // 0-23
  uint8_t  minute     = 0;     // 0-59
  bool     power      = false; // true=включить, false=выключить
  String   mode       = "auto";
  uint8_t  targetTemp = 22;
  uint8_t  days       = 0x7F;  // битмаск дней: bit0=Пн, bit6=Вс
};

// Инициализация: запуск NTP, восстановление слотов из NVS
void schedulerSetup();

// Проверка расписания — вызывать в loop() раз в ~30 сек
void schedulerTick();

// Получить слот по индексу (0-3)
ScheduleSlot schedulerGetSlot(uint8_t index);

// Сохранить слот
void schedulerSetSlot(uint8_t index, const ScheduleSlot& slot);

// Текущее время в виде строки "HH:MM" (пустая если NTP не синхронизирован)
String schedulerGetTimeStr();
