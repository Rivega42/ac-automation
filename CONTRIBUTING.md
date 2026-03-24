# Правила работы с репозиторием AC Automation

## Содержание

1. [Структура репозитория](#структура-репозитория)
2. [Ветки и рабочий процесс](#ветки-и-рабочий-процесс)
3. [Правила коммитов](#правила-коммитов)
4. [Локальная разработка](#локальная-разработка)
5. [Прошивка ESP32](#прошивка-esp32)
6. [Текущее железо](#текущее-железо)

---

## Структура репозитория

```
ac-automation/
├── firmware/
│   ├── src/
│   │   ├── main.cpp          ← точка входа, setup/loop
│   │   ├── state.h           ← структура состояния кондея
│   │   ├── api.cpp / api.h   ← HTTP API эндпоинты
│   │   ├── mqtt.cpp / mqtt.h ← MQTT для Home Assistant
│   │   └── config.h          ← SSID, пароли, GPIO пины (не коммитить реальные!)
│   ├── data/                  ← SPIFFS: веб-интерфейс
│   │   ├── index.html
│   │   ├── style.css
│   │   └── app.js
│   └── platformio.ini         ← конфиг PlatformIO
│
├── telegram_bot/
│   ├── bot.py                 ← Telegram бот (Python)
│   ├── config.py              ← токен бота, URL ESP32 (не коммитить!)
│   └── requirements.txt
│
├── home_assistant/
│   ├── automations.yaml       ← примеры автоматизаций
│   └── README.md
│
├── hardware/
│   ├── photos/                ← фото компонентов и монтажа
│   └── wiring_diagram.png     ← схема подключения
│
├── docs/
│   ├── ТЗ.md                  ← полное техническое задание
│   └── hardware.md            ← описание железа
│
├── CONTRIBUTING.md            ← этот файл
└── README.md
```

---

## Ветки и рабочий процесс

### Ветки

| Ветка | Назначение |
|-------|-----------|
| `main` | Стабильная прошивка — заливается в ESP32 |
| `dev` | Разработка — тестируем, экспериментируем |

### Как работаем

```
dev ←── разработка (коммитим сюда)
  │
  │  После проверки на железе
  ▼
main ←── merge из dev
  │
  │  Вручную
  ▼
ESP32 (OTA или кабель)
```

**Шаги:**

1. Работаем в ветке `dev`:
   ```bash
   git checkout dev
   git pull origin dev
   ```

2. Вносим изменения, коммитим:
   ```bash
   git add .
   git commit -m "feat: добавлено чтение индикатора FULL WATER"
   git push origin dev
   ```

3. После проверки на ESP32 — merge в main:
   ```bash
   git checkout main
   git merge dev
   git push origin main
   ```

---

## Правила коммитов

### Формат

```
<тип>: <описание на русском>
```

### Типы

| Тип | Когда использовать |
|-----|-------------------|
| `feat:` | Новая функциональность |
| `fix:` | Исправление бага |
| `chore:` | Конфиги, мелкие правки без изменения логики |
| `refactor:` | Рефакторинг кода |
| `docs:` | Документация, схемы, ТЗ |
| `hw:` | Изменения, связанные с железом (GPIO, схема) |
| `ai:` | Задачи, выполненные с помощью AI |

### Примеры

```bash
# Хорошо
git commit -m "feat: HTTP API для управления кнопками"
git commit -m "fix: исправлен debounce кнопки POWER"
git commit -m "hw: добавлено чтение индикатора FULL WATER через GPIO 39"
git commit -m "docs: схема подключения PC817"
git commit -m "feat: веб-интерфейс — кнопки управления"

# Плохо
git commit -m "Fix"
git commit -m "Changes"
git commit -m "WIP"
```

---

## Локальная разработка

### Требования

- [PlatformIO](https://platformio.org/) (VS Code расширение или CLI)
- Python 3.10+ (для Telegram-бота)
- Arduino библиотеки (устанавливаются через `platformio.ini`)

### Первый запуск прошивки

```bash
# 1. Клонировать репо
git clone https://github.com/Rivega42/ac-automation.git
cd ac-automation/firmware

# 2. Создать конфиг с реальными данными (не коммитить!)
cp src/config.example.h src/config.h
# Отредактировать: SSID, пароль, MQTT хост

# 3. Собрать
pio run

# 4. Залить прошивку (USB)
pio run --target upload

# 5. Залить веб-интерфейс в SPIFFS
pio run --target uploadfs

# 6. Открыть монитор порта
pio device monitor --baud 115200
```

### OTA обновление (по Wi-Fi)

```bash
# После первой заливки через USB — дальше по воздуху:
pio run --target upload --upload-port ac-control.local
```

### Telegram-бот

```bash
cd telegram_bot
pip install -r requirements.txt
cp config.example.py config.py
# Отредактировать: BOT_TOKEN, ESP32_URL
python bot.py
```

---

## Прошивка ESP32

### GPIO карта

| GPIO | Назначение | Тип |
|------|-----------|-----|
| 2 | POWER кнопка → PC817 CH1 | OUT |
| 4 | MODE кнопка → PC817 CH2 | OUT |
| 12 | SPEED кнопка → PC817 CH3 | OUT |
| 13 | TIMER кнопка → PC817 CH4 | OUT |
| 14 | SLEEP кнопка → PC817 CH5 | OUT |
| 15 | TEMP+ кнопка → PC817 CH6 | OUT |
| 16 | TEMP- кнопка → PC817 CH7 | OUT |
| 3 | DHT22 датчик | IN |
| 26 | COMP индикатор | IN |
| 32 | AUTO индикатор | IN |
| 33 | COOL индикатор | IN |
| 34 | FAN индикатор | IN |
| 35 | HI индикатор | IN |
| 36 | LOW индикатор | IN |
| 39 | FULL WATER индикатор | IN |

### HTTP API

Базовый URL: `http://ac-control.local/api`

| Метод | Endpoint | Описание |
|-------|----------|----------|
| POST | `/api/button/power` | Нажать POWER |
| POST | `/api/button/mode` | Нажать MODE |
| POST | `/api/button/speed` | Нажать SPEED |
| POST | `/api/button/timer` | Нажать TIMER |
| POST | `/api/button/sleep` | Нажать SLEEP |
| POST | `/api/button/temp_up` | TEMP+ |
| POST | `/api/button/temp_down` | TEMP- |
| GET  | `/api/state` | Текущее состояние (JSON) |
| GET  | `/api/sensor` | Данные DHT22 |

### MQTT топики

Prefix: `home/ac/mystery`

| Топик | Направление | Описание |
|-------|-------------|----------|
| `.../state` | OUT | Полное состояние JSON |
| `.../temperature` | OUT | Температура DHT22 |
| `.../humidity` | OUT | Влажность DHT22 |
| `.../fullwater` | OUT | Бак полный (true/false) |
| `.../available` | OUT | Онлайн/оффлайн |
| `.../set/power` | IN | ON / OFF |
| `.../set/mode` | IN | auto / cool / fan |
| `.../set/temp` | IN | 16-30 |
| `.../set/speed` | IN | hi / low |

---

## Текущее железо

| Компонент | Модель | Статус |
|-----------|--------|--------|
| Кондиционер | Mystery MSS-09R07M | ✅ есть |
| Контроллер | ESP32-CAM | ✅ есть |
| Оптоизоляция | 8-канальный PC817 | ✅ есть |
| Датчик | DHT22 | нужно купить |
| Резисторы делитель | 10кОм + 20кОм × 7 | нужно купить |
| Резисторы PC817 | 1кОм × 7 | нужно купить |
| Провода | Dupont | нужно купить |

### Доступ

- Веб-интерфейс: `http://ac-control.local`
- OTA обновление: `http://ac-control.local/update`
- MQTT: `mqtt://<broker-ip>:1883`

---

*Последнее обновление: март 2026*
