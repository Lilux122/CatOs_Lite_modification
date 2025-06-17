#define FIRMWARE_VERSION "v0.1LITE"
#include <GyverOLED.h>      // либа дисплея
#include "GyverButton.h"    // кнопки
#include <GyverDBFile.h>    // для настроек и wifi
#include <LittleFS.h>       // для хранения данных в little fs
#include <SettingsGyver.h>  // либа веб морды
#include <bitmaps.h>
#include <oled_menu.h>
// ------------------
bool alert_f;               // показ ошибки в вебморде
bool wifiConnected = false; // для wifi морды
// ------------------
//читалка
byte cursor = 0;            // Указатель (курсор) меню
byte files = 0;             // Количество файлов
const int MAX_PAGE_HISTORY = 150;
long pageHistory[MAX_PAGE_HISTORY] = {0};
int currentHistoryIndex = -1;
int totalPages = 0;
//-------------
//показ заряда
#define REF_VOLTAGE     3.3     // Опорное напряжение ADC (3.3V для ESP32)
#define ADC_RESOLUTION  12      // 12 бит (0-4095)
#define VOLTAGE_DIVIDER 2.0
// Напряжения для расчета процента заряда (калибровка под ваш аккумулятор). Настройки в serv меню
#define BAT_NOMINAL_VOLTAGE 3.7 // Номинальное напряжение (3.7V)
#define BATTERY_PIN         4   // GPIO4 для измерения напряжения
float batteryVoltage = 0;
int batteryPercentage = 0;
//----------------------------------
GyverDBFile db(&LittleFS, "/data.db");              //файл где хранятся настройки
SettingsGyver sett("CatOS " FIRMWARE_VERSION, &db); // веб морда
#define UP_PIN 5
#define DOWN_PIN 18
#define OK_PIN 23

GyverOLED<SSD1306_128x64, OLED_BUFFER, OLED_I2C> oled(0x3C);
GButton up(UP_PIN);
GButton down(DOWN_PIN);
GButton ok(OK_PIN);


//----------------------------------------
DB_KEYS(
  kk,
  OLED_BRIGHTNESS,
  BAT_MIN_VOLTAGE,
  BAT_MAX_VOLTAGE,
  AP_SSID,
  AP_PASS,
  wifi_enabled,
  wifi_ssid,
  wifi_pass,
  serial_num,
  apply
);
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
int getBattery() {
  int adcValue = analogRead(BATTERY_PIN);
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  
  batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
  batteryPercentage = mapFloat(batteryVoltage, bat_min, bat_max, 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  return batteryPercentage;
}
int getVoltage() {
  int adcValue = analogRead(BATTERY_PIN);
  batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
  return batteryVoltage;
}
void drawbattery() {
  int battery = getBattery(); // Обновляем заряд
  if (battery <= 10){
    oled.setContrast(50);
  } else {
    oled.setContrast(db[kk::OLED_BRIGHTNESS].toInt());
  }

  oled.clear(110, 0, 127, 6); 
  
  // Выбираем битмап в зависимости от уровня заряда
  if (battery <= 10) {
    oled.drawBitmap(110, 0, bmp_0, 20, 7);
  } else if (battery <= 30) {
    oled.drawBitmap(110, 0, bmp_20, 20, 7);
  } else if (battery <= 60) {
    oled.drawBitmap(110, 0, bmp_50, 20, 7);
  } else {
    oled.drawBitmap(110, 0, bmp_100, 20, 7);
  }
  oled.update();
}

void drawStaticMenu() {
  oled.clear();
  for (uint8_t i = 0; i < VISIBLE_ITEMS; i++) {
      oled.setCursor(1, i);
      oled.print(menu_items[i + top_item]);
  }
  oled.update();
}
void updatePointer() {
  static int8_t last_pointer = -1;
  static int8_t last_top = -1;
  if (last_top != top_item) {
      drawStaticMenu();
      last_top = top_item;
  }
  if (last_pointer != -1) {
      oled.setCursor(0, last_pointer - top_item);
      oled.print(" ");
  }
  oled.setCursor(0, pointer - top_item);
  oled.print(">");
  oled.update();
  last_pointer = pointer;
}
// красивая рамка
void ui_rama(const char* name, bool draw_bat, bool draw_line, bool cleardisplay) {
  if (cleardisplay) {
    oled.clear(); // чити
  }
  oled.home();  // домой
  oled.setScale(1); // размер 1
  oled.print(name);  // надпись
  oled.setScale(0); // вернуть размер назад (костыль)
  if (draw_bat) {
    drawbattery();
  }
  if (draw_line) {
    oled.line(0, 10, 127, 10);   // Линия
  }
  oled.update();
}
//------------------------
String getChipID() {
  uint64_t chipid = ESP.getEfuseMac();
  char sn[17];
  snprintf(sn, sizeof(sn), "%04X%08X", 
         (uint16_t)(chipid >> 32), 
         (uint32_t)chipid);
  return String(sn);
}

void update(sets::Updater& upd) {
  static uint8_t lastBrightness = db[kk::OLED_BRIGHTNESS].toInt();
  if(lastBrightness != db[kk::OLED_BRIGHTNESS].toInt()) {
      lastBrightness = db[kk::OLED_BRIGHTNESS].toInt();
      oled.setContrast(lastBrightness);
      db.update();
  }
  if (alert_f) {
    alert_f = false;
    upd.alert("Пароль должен быть не менее 8 символов!");
  }
    // Проверка корректности напряжений
    float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
    float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
    
    if(bat_min >= bat_max) {
      upd.alert("Мин. напряжение должно быть меньше макс.!");
      db[kk::BAT_MIN_VOLTAGE] = 2.32;
      db[kk::BAT_MAX_VOLTAGE] = 3.45;
      db.update();
    }
}
// ---------------------------------------------
bool connectToWiFi() {
  String ssid = db[kk::wifi_ssid];
  String pass = db[kk::wifi_pass];
  WiFi.mode(WIFI_AP_STA);
  if(ssid.isEmpty()) {
      Serial.println("WiFi SSID not configured!");
      return false;  // Return false if SSID is empty
  }
 
  // Сброс предыдущих статусов
  wifiConnected = false;
  WiFi.begin(ssid, pass);
  oled.autoPrintln(true);
  oled.clear();
  oled.home();
  oled.print("Подключение к");
  oled.setCursor(0, 2);
  oled.print(ssid);
  oled.setCursor(0, 3);
  oled.print("Статус:");
  oled.update();
  // Ожидание подключения с визуализацией
  uint32_t startTime = millis();
  // Прерываем ожидание если подключились
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    oled.print(".");
    oled.update();
    if (millis() - startTime > 10000) { // 10 second timeout
      oled.print("Не подключено! Запуск AP");
      oled.update();
      delay(1000);
      oled.autoPrintln(false);
      return false; // Return false if connection fails
    }
  }
  oled.autoPrintln(false);
  return true; // Return true if connected successfully
}
void startAP() {
  // Используем AP_SSID и AP_PASS вместо ap_ssid/ap_pass
  String ssid = db[kk::AP_SSID];
  String pass = db[kk::AP_PASS];
 
  if (ssid.length() == 0) {
      ssid = "CatOs";
      db[kk::AP_SSID] = ssid; // Исправлено на AP_SSID
  }
  if (pass.length() < 8) {
      pass = "12345678";
      db[kk::AP_PASS] = pass; // Исправлено на AP_PASS
  }
  WiFi.softAP(
    db[kk::AP_SSID].toString().c_str(),
    db[kk::AP_PASS].toString().c_str()
  );
 
}
void stopWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi completely stopped");
}
//----------
void build(sets::Builder& b) {
  // Секция настроек дисплея
  {
      sets::Group g(b, "Дисплей");
      b.Slider(kk::OLED_BRIGHTNESS, "Яркость дисплея", 0, 255, 1, "", nullptr, sets::Colors::Blue);
  }

  // Секция WiFi
  {
      sets::Group g(b, "WiFi");
      // Переключатель WiFi с привязкой к значению из БД
      bool wifiEnabled = db[kk::wifi_enabled].toInt();
      b.Switch(kk::wifi_enabled, "WiFi подключение", &wifiEnabled);
      b.Input(kk::wifi_ssid, "SSID WiFi");
      b.Pass(kk::wifi_pass, "Пароль WiFi");
  }

  // Секция точки доступа
  {
      sets::Group g(b, "Точка доступа");
      b.Input(kk::AP_SSID, "SSID точки");
      b.Pass(kk::AP_PASS, "Пароль точки");
  }
  {
    sets::Group g(b, "Калибровка АКБ");
    b.Number(kk::BAT_MIN_VOLTAGE, "Мин. напряжение");
    b.Number(kk::BAT_MAX_VOLTAGE, "Макс. напряжение");
  }
  // Секция управления
  {
      sets::Group g(b, "Управление");
      if(b.Button("Применить сетевые настройки")) {
        if (db[kk::AP_PASS].toString().length() >= 8) {
            db.update();
            sett.reload(true);
        } else {
            alert_f = true;  // Устанавливаем флаг
        }
    }
  }
}
void resetButtons() {
  ok.resetStates();
  down.resetStates();
  up.resetStates();
}
void exit_without_update() {
  oled.clear();
  resetButtons();
  oled.setScale(1);
  drawbattery();
  oled.update();
}
// Новая функция инициализации настроек
void initSettings() {
  // Существующие настройки
  if (!db.has(kk::OLED_BRIGHTNESS)) db.init(kk::OLED_BRIGHTNESS, 128);
  if (!db.has(kk::BAT_MIN_VOLTAGE)) 
    db.init(kk::BAT_MIN_VOLTAGE, 2.32f);
  if (!db.has(kk::BAT_MAX_VOLTAGE)) 
    db.init(kk::BAT_MAX_VOLTAGE, 3.45f);
  if (!db.has(kk::AP_SSID)) {
    db.init(kk::AP_SSID, "CatOs");
  }
  if (!db.has(kk::AP_PASS)) {
      db.init(kk::AP_PASS, "12345678");
  }
  if (!db.has(kk::wifi_enabled)) {
      db.init(kk::wifi_enabled, 0);
  }
  if (!db.has(kk::serial_num)) {
    db.init(kk::serial_num, getChipID());
  }
  if (!db.has(kk::wifi_ssid)) db.init(kk::wifi_ssid, "");
  if (!db.has(kk::wifi_pass)) db.init(kk::wifi_pass, "");
  
  db.update();
}
void buttons_tick() {
  up.tick();
  down.tick();
  ok.tick();
}
void exit() {
  oled.clear();
  resetButtons();
  oled.setScale(1);
  drawStaticMenu();
  updatePointer();
  oled.update();
}
bool draw_logo() {
  uint32_t tmr = millis();
  bool btn_pressed = false;
  
  oled.clear();
  oled.drawBitmap(50, 16, logo, 32, 32);
  oled.setCursor(0,0);
  oled.print("By CatDevCode " FIRMWARE_VERSION);
  oled.setCursor(0,7);
  oled.print("         CatOs");
  oled.update();

  while (millis() - tmr < 2000) {
      ok.tick();
      if (ok.isClick()) {
          btn_pressed = true;
          break;
      }
      delay(10);
  }

  oled.clear();
  oled.home();
  oled.update();
  return btn_pressed;
}
void testBattery() {
  ui_rama("Версия " FIRMWARE_VERSION, true, true, true);
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  while (true) {
    int adcValue = analogRead(BATTERY_PIN);
    
    batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
    batteryPercentage = mapFloat(batteryVoltage, bat_min, bat_max, 0, 100);
    batteryPercentage = constrain(batteryPercentage, 0, 100);
    oled.setCursor(0,2);
    oled.print("V ~: ");
    oled.print(batteryVoltage);
    
    oled.setCursor(0,3);
    oled.print("Уровень заряда ~: ");
    oled.print(batteryPercentage);
    oled.print("%");
    
    oled.setCursor(0,4);
    oled.print("Пин: ");
    oled.print(BATTERY_PIN);
    
    oled.setCursor(0,5);
    oled.print("Мин V: ");
    oled.print(bat_min);
    
    oled.setCursor(0,6);
    oled.print("Макс V: ");
    oled.print(bat_max);

    static uint32_t batteryTimer = millis();
    buttons_tick();
    if (millis() - batteryTimer > 5000) {
        batteryTimer = millis();
        drawbattery();
    }
    oled.update();
    
    if(ok.isClick()) {
      return;
    }
  }
}
void sysInfo() {
  oled.autoPrintln(true);
  ui_rama("Версия " FIRMWARE_VERSION, true, true, true);
  oled.setCursor(0,2);
  oled.printf("Опиративка: %d \n", ESP.getFreeHeap());
  oled.setCursor(0,3);
  oled.printf("Размер прош.:%u \n", ESP.getSketchSize());
  oled.setCursor(0,4);
  oled.printf("Модель ESP: %s\n", ESP.getChipModel());
  oled.update();
  while(true){
    buttons_tick();
    if (ok.isClick()) {
      resetButtons();
      oled.clear();
      oled.home();
      oled.update();
      oled.autoPrintln(false);
      return;
    }
  } 
}
bool deleteSettings() {
  if (LittleFS.remove("/data.db")) {   // Удаляем файл с настройками
    db.begin();                        // Переинициализируем базу
    initSettings();                    // Создаём настройки по умолчанию
    return true;
  }
  return false;
}
void deleteSettings_ui() {
  oled.clear();
  if (deleteSettings()) {
    oled.setScale(2);
    oled.setCursorXY(10, 16);
    oled.print(F("настройки"));
    oled.setCursorXY(16, 32);
    oled.print(F("сброшены"));
    oled.setScale(1);
    oled.update();
  } else {
    oled.setScale(2);
    oled.setCursorXY(10, 16);
    oled.print(F("Ошибочка!"));
    oled.setScale(1);
    oled.update();
  }
  delay(1500);
  ESP.restart();
}
void formatFS() {
  oled.clear();
  oled.setCursor(0, 2);
  oled.print("Подтвердите сброс:");
  oled.setCursor(0, 4);
  oled.print("Удерживайте OK");
  oled.update();
  
  // Ожидание подтверждения 3 секунды
  uint32_t tmr = millis();
  while(millis() - tmr < 3000) {
    ok.tick();
    if(ok.isHold()) {
      LittleFS.format();                   // Форматируем файловую систему
      oled.clear();
      oled.setScale(2);
      oled.setCursorXY(34, 16);
      oled.print(F("файлы"));
      oled.setCursorXY(22, 32);
      oled.print(F("удалены"));
      oled.setScale(1);
      oled.update();
      delay(1500);
      ESP.restart();
    }
  } 
}
void batteryCalibration() {
  enum CalState { MIN_VOLTAGE, MAX_VOLTAGE, SAVE }; // Объявляем перечисление
  CalState state = MIN_VOLTAGE; // Используем CalState как тип
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  float* current_val = &bat_min;
  
  while(true) {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Калибровка АКБ:");
    
    // Отображение текущих значений
    oled.setCursor(0, 2);
    oled.print(state == MIN_VOLTAGE ? ">" : " ");
    oled.print("Мин: ");
    oled.print(bat_min, 2);
    oled.print("V");
    
    oled.setCursor(0, 3);
    oled.print(state == MAX_VOLTAGE ? ">" : " ");
    oled.print("Макс: ");
    oled.print(bat_max, 2);
    oled.print("V");
    
    oled.setCursor(0, 5);
    oled.print(state == SAVE ? ">" : " ");
    oled.print("Сохранить");
    
    oled.setCursor(0, 7);
    oled.print("Удержать OK - выход");
    
    oled.update();

    buttons_tick();
    
    // Навигация
    if(up.isClick()) {
      if(state > MIN_VOLTAGE) state = (CalState)(state - 1);
    }
    if(down.isClick()) {
      if(state < SAVE) state = (CalState)(state + 1);
    }
    
    // Регулировка значений
    if(ok.isClick()) {
      switch(state) {
        case MIN_VOLTAGE: current_val = &bat_min; break;
        case MAX_VOLTAGE: current_val = &bat_max; break;
        case SAVE: 
          if(bat_min < bat_max) {
            db[kk::BAT_MIN_VOLTAGE] = bat_min;
            db[kk::BAT_MAX_VOLTAGE] = bat_max;
            db.update();
            return;
          }
          break;
      }
    }
    
    // Изменение значений
    if(state != SAVE) {
      if(up.isDouble()) *current_val += 0.01;
      if(down.isDouble()) *current_val -= 0.01;
      
      // Ограничения
      *current_val = constrain(*current_val, 2.0, 4.2);
    }
    
    // Выход по удержанию
    if(ok.isHold()) {
      if(bat_min < bat_max) {
        db[kk::BAT_MIN_VOLTAGE] = bat_min;
        db[kk::BAT_MAX_VOLTAGE] = bat_max;
        db.update();
      }
      return;
    }
  }
}
void servmode() {
  const char* serv_apps[] = {
    "Cброс настроек",
    "Форматирование",
    "Тест % батареи",
    "Калибровка АКБ",
    "Информация о системе",
    "Выход"
  };
  const uint8_t serv_apps_count = sizeof(serv_apps)/sizeof(serv_apps[0]);
  int8_t serv_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Версия " FIRMWARE_VERSION, true, true, true);
  
  while(true) {
    static uint32_t timer = 0;
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(serv_apps_ptr == 0 ? ">" : " ");
    oled.print(serv_apps[0]);

    // Рисуем остальные пункты если есть
    if(serv_apps_count > 1) {
      for(uint8_t i = 1; i < serv_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(serv_apps_ptr == i ? ">" : " ");
        oled.print(serv_apps[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && serv_apps_ptr < 5|| (up.isHold() && millis() - timer > 150)) {
      if (serv_apps_ptr < 1) {
          serv_apps_ptr++;
      }
      timer = millis();
      serv_apps_ptr--;
    }
    if(down.isClick() && serv_apps_ptr < 5|| (down.isHold() && millis() - timer > 150)) {
      if (serv_apps_ptr >= 5) {
          serv_apps_ptr--;
      }
      timer = millis();
      serv_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(serv_apps_ptr) {
        case 0: deleteSettings_ui(); break;
        case 1: formatFS(); break;
        case 2: testBattery(); break;
        case 3: batteryCalibration(); break;
        case 4: sysInfo(); break;
        case 5: ESP.restart(); // Выход
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Версия " FIRMWARE_VERSION, true, true, true);
    }
  }
}
void setup() {
    oled.init(); 
    oled.clear(); 
    oled.setContrast(255);
    oled.setScale(1);
    Wire.setClock(800000L);
    ok.tick();
    analogReadResolution(ADC_RESOLUTION);
    #ifdef ESP32
      LittleFS.begin(true);
    #else
      LittleFS.begin();
    #endif
    db.begin();  // Читаем данные из файла
    
    // Инициализируем ключи с проверкой значений
    initSettings();

    // Применяем настройки
    oled.setContrast(db[kk::OLED_BRIGHTNESS].toInt());
    // Если во время показа логотипа нажали OK - войти в сервисный режим
    if (draw_logo()) {
      servmode();
    }
    drawStaticMenu();
    updatePointer();
    randomSeed(getBattery() + getVoltage() / micros());
    setCpuFrequencyMhz(80);
    drawbattery();
}
void create_settings() {
  String ssid = db[kk::wifi_ssid];
  bool wifi_connected = false;
  
    if(db[kk::wifi_enabled].toInt()) {
      connectToWiFi();
      if(ssid.isEmpty()) {
        Serial.println("WiFi SSID not configured!");
        startAP();
      }
      if(WiFi.status() == WL_CONNECTED) {
          // Успешное подключение
          Serial.print("Connected! IP: ");
          Serial.println(WiFi.localIP());
          wifiConnected = true;
      } else {
          // Не удалось подключиться - запуск AP
          startAP();
      }
  } else {
      // WiFi отключен в настройках
      startAP();
  }
  // Основной интерфейс настроек
  oled.clear();
  ui_rama("WiFi Веб", false, true, true);
  oled.setCursor(0, 2);
  if (wifiConnected) {
    oled.print("IP:");
    oled.print(WiFi.localIP());
    oled.setCursor(0, 3);
    oled.print("Сеть: ");
    oled.print(ssid);
  } else {
    oled.setCursor(0, 2);
    oled.print("IP: " + WiFi.softAPIP().toString());
    oled.setCursor(0, 3);
    oled.print("Сеть: " + db[kk::AP_SSID].toString());
    oled.setCursor(0, 4);
    oled.print("Пароль: " + db[kk::AP_PASS].toString());
    oled.update();
  }
  oled.setCursor(0, 6);
  oled.print("OK - Перезагрузка");
  oled.update();
  sett.setVersion(FIRMWARE_VERSION);
  // Запуск веб-сервера
  sett.begin();
  sett.onBuild(build);
 
  while(true) {
      sett.tick();
      delay(10);
      buttons_tick();
      if(ok.isClick()) {
          ESP.restart();
      }
  }
}
/* ========================== Работа с файлами =========================== */
int getFilesCount() {
  File root = LittleFS.open("/");
  int count = 0;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.endsWith(".txt") || filename.endsWith(".h")) {
      count++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return count;
}

String getFilenameByIndex(int idx) {
  File root = LittleFS.open("/");
  int i = 0;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.endsWith(".txt") || filename.endsWith(".h")) {
      if (i == idx) {
        String name = "/" + filename;
        file.close();
        root.close();
        return name;
      }
      i++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return "";
}
/* ======================================================================= */
/* ============================ Главное меню ============================= */
void update_cursor() {
  for (uint8_t i = 0; i < 6 && i < files; i++) {    // Проходимся от 2й до 8й строки оледа
    oled.setCursor(0, i + 2);                       // Ставим курсор на нужную строку
    oled.print(getFilenameByIndex(cursor < 6 ? i : i + (cursor - 5)));  // Выводим имя файла
  }
  oled.setCursor(0, constrain(cursor, 0, 5) + 2); oled.print(" ");      // Чистим место под указатель
  oled.setCursor(0, constrain(cursor, 0, 5) + 2); oled.print(">");      // Выводим указатель на нужной строке
  oled.update();                                    // Выводим картинку
}
bool drawMainMenu(void) {                           // Отрисовка главного меню
  oled.clear();                                     // Очистка
  oled.home();                                      // Возврат на 0,0
  oled.line(0, 10, 127, 10);                        // Линия
  if (files == 0) {
    oled.clear();
    oled.setScale(2);
    oled.setCursorXY(34, 16);
    oled.print(F("файлов"));
    oled.setCursorXY(22, 32);
    oled.print(F("нету :("));
    oled.setScale(1);
    oled.update();
    delay(1500);
    return false;
  }
  oled.print("НАЙДЕННЫЕ ФАЙЛЫ: "); oled.print(files);   // Выводим кол-во файлов
  update_cursor();
  return true;
}
/* ======================================================================= */
/* ============================ Чтение файла ============================= */
uint8_t parseHFile(uint8_t *img, File &file) {
  int imgLen = 0;
  memset(img, 0, 1024); // Очистка буфера

  // Пропускаем все символы до '{'
  while (file.available()) {
    if (file.read() == '{') break;
  }

  // Читаем данные до '}' или конца файла
  while (file.available() && imgLen < 1024) {
    char c = file.read();
    if (c == '}') break; // Конец данных
    
    // Парсим HEX-значения вида 0xXX
    if (c == '0' && file.peek() == 'x') {
      file.read(); // Пропускаем 'x'
      char hex[3] = {0};
      hex[0] = file.read();
      hex[1] = file.read();
      img[imgLen++] = strtoul(hex, NULL, 16); // Конвертируем HEX в байт
    }
    yield(); // Для стабильности ESP
  }

  return (imgLen == 1024) ? 0 : 1; // 0 = успех, 1 = ошибка
}
void enterToReadBmpFile(String filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    files = getFilesCount();
    drawMainMenu();
    file.close();
    return;
  }

  uint8_t *img = new uint8_t[1024]; // 128x64 бит = 1024 байт
  if (parseHFile(img, file)) {
    delete[] img;
    file.close();
    files = getFilesCount();
    drawMainMenu();
    return;
  }

  oled.clear();
  oled.drawBitmap(0, 0, img, 128, 64);
  oled.update();
  delete[] img;
  setCpuFrequencyMhz(80);
  while (true) {
    ok.tick();
    if (ok.isClick()) {
      setCpuFrequencyMhz(240);
      file.close();
      files = getFilesCount();
      drawMainMenu();
      setCpuFrequencyMhz(80);
      return;
    }
    yield();
  }
}

void drawPage(File &file, bool storeHistory = true) {
  if(storeHistory) {
    // Сохраняем текущую позицию перед отрисовкой
    currentHistoryIndex = (currentHistoryIndex + 1) % MAX_PAGE_HISTORY;
    pageHistory[currentHistoryIndex] = file.position();
    totalPages = min(totalPages + 1, MAX_PAGE_HISTORY);
  }
  oled.clear();
  oled.home();
  
  const uint8_t maxLineLength = 37;
  uint8_t currentLine = 0;
  String buffer = "";
  String word = "";
  uint8_t spaceLeft = maxLineLength;

  while (file.available() && currentLine < 8) {
    char c = file.read();
    
    if (c == '\n' || c == '\r') {
      if (!buffer.isEmpty()) {
        oled.println(buffer);
        currentLine++;
        buffer = "";
        spaceLeft = maxLineLength;
      }
      continue;
    }

    if (c == ' ') {
      if (buffer.length() + word.length() + 1 <= maxLineLength) {
        buffer += word;
        buffer += ' ';
        spaceLeft = maxLineLength - buffer.length();
        word = "";
      } else {
        oled.println(buffer);
        currentLine++;
        buffer = word + ' ';
        spaceLeft = maxLineLength - buffer.length();
        word = "";
      }
      continue;
    }

    word += c;

    if (word.length() > spaceLeft) {
      String part = word.substring(0, spaceLeft);
      buffer += part;
      oled.println(buffer);
      currentLine++;
      
      word = word.substring(spaceLeft);
      buffer = "";
      spaceLeft = maxLineLength;
      
      if (currentLine >= 8) break;
    }
  }
  // Вывод остатков
  if (!buffer.isEmpty() && currentLine < 8) {
    oled.println(buffer);
    currentLine++;
  }
  if (!word.isEmpty() && currentLine < 8) {
    oled.println(word);
  }

  oled.update();
}
void enterToReadTxtFile(String filename){
  File file = LittleFS.open(getFilenameByIndex(cursor), "r"); // Чтение имени файла по положению курсора и открытие файла
  if (!file) {                                      // Если сам файл не порядке
    files = getFilesCount(); drawMainMenu();        // Смотрим количество файлов и рисуем главное меню
    file.close(); return;                           // Закрываем файл и выходим
  }
  memset(pageHistory, 0, sizeof(pageHistory));
  currentHistoryIndex = -1;
  totalPages = 0;

  drawPage(file);                                   // Если с файлом все ок - рисуем первую страницу
  setCpuFrequencyMhz(80);
  while (1) {                                       // Бесконечный цикл
    up.tick();                                      // Опрос кнопок
    ok.tick();
    down.tick();
    if (ok.isClick()) {                             // Если ок нажат
      setCpuFrequencyMhz(240);
      files = getFilesCount(); drawMainMenu();      // Смотрим количество файлов и рисуем главное меню
      file.close();
      setCpuFrequencyMhz(80);
      return;                         // Закрываем файл и выходим
    } else if (up.isClick() or up.isHold()) {       // Если нажата или удержана вверх
      if(totalPages > 0) {
        totalPages--;
        currentHistoryIndex = (currentHistoryIndex - 1 + MAX_PAGE_HISTORY) % MAX_PAGE_HISTORY;
        file.seek(pageHistory[currentHistoryIndex]);
        drawPage(file, false);                      // Не сохраняем в историю
      }                                             // Устанавливаем указатель файла
    } else if (down.isClick() or down.isHold()) {   // Если нажата или удержана вниз
      drawPage(file);                               // Рисуем страницу
    }
    yield();                                        // Внутренний поллинг ESP
  }
}

void enterToReadFile(void) { 
  setCpuFrequencyMhz(240);
  String filename = getFilenameByIndex(cursor);
  if (filename.endsWith(".h")) {
    enterToReadBmpFile(filename);
  } else if(filename.endsWith(".txt")) {
    // Вызов существующей функции для текстовых файлов
    enterToReadTxtFile(filename);
  } else {
  }                
}
void ShowFilesLittleFS() {
  oled.autoPrintln(false);
  setCpuFrequencyMhz(240);
  files = getFilesCount();                    // Читаем количество файлов
  if (drawMainMenu() == false){               // Рисуем главное меню
    exit();
    setCpuFrequencyMhz(80);
    return;
  }        
  setCpuFrequencyMhz(80);          
  while (true)
  {
    buttons_tick();                                     // Опрос кнопок
    static uint32_t timer = 0;                          // таймер
    if (up.isClick() || (up.isHold() && millis() - timer > 50)) {                // Если нажата или удержана кнопка вверх
      setCpuFrequencyMhz(240);
      cursor = constrain(cursor - 1, 0, files - 1);   // Двигаем курсор
      timer = millis();
      update_cursor();    
      setCpuFrequencyMhz(80);
    } else if (down.isClick() || (down.isHold() && millis() - timer > 50)) {       // Если нажата или удержана кнопка вниз
      setCpuFrequencyMhz(240);
      cursor = constrain(cursor + 1, 0, files - 1);   // Двигаем курсор
      timer = millis();
      update_cursor();                                 // Обновляем главное меню
      setCpuFrequencyMhz(80);
    } else if (ok.isHold()) {                         // Если удержана ОК
      exit();                                         // Выход                        
      return;                                         // Выход
    } else if (ok.isClick()) {                        // Если нажата ОК
      enterToReadFile();                              // Переходим к чтению файла
    }
  }
  
}
// Настройки сети
void networkSettings_ap() {
  String ssid = db[kk::AP_SSID].toString();
  String pass = db[kk::AP_PASS].toString();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Сеть: ");
      oled.print(ssid);
      oled.setCursor(0, 2);
      oled.print("Пароль: ");
      oled.print(pass);
      oled.setCursor(0, 4);
      oled.print("OK - выход");
      oled.update();

      buttons_tick();
      
      if(ok.isClick()) {
          return;
      }
  }
}
// Регулировка яркости
void brightnessAdjust() {
  int brightness = db[kk::OLED_BRIGHTNESS].toInt();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Яркость: ");
      oled.print(brightness);
      oled.setCursor(0, 2);
      oled.print("OK - сохранить");
      oled.update();

      buttons_tick();
      
      if(up.isClick()) brightness = constrain(brightness + 10, 0, 255); oled.setContrast(brightness);
      if(down.isClick()) brightness = constrain(brightness - 10, 0, 255); oled.setContrast(brightness);
      if(ok.isClick()) {
          db[kk::OLED_BRIGHTNESS] = brightness;
          oled.setContrast(brightness);
          db[kk::OLED_BRIGHTNESS] = brightness;
          db.update(); 
          return;
      }
      if(ok.isHold()) return;
  }
}
void aboutFirmware() {
  String serial_number = db[kk::serial_num].toString();
  ui_rama("О прошивке", true, true, true);
  
  oled.setCursor(0, 2);
  oled.print("Автор: CatDevCode");
  oled.setCursor(0, 3);
  oled.print("Версия: " FIRMWARE_VERSION);
  oled.setCursor(0, 4);
  #ifdef ESP32
    oled.print("Платформа: ESP32");
  #elif ESP8266
    oled.print("Платформа: ESP8266");
  #else
    oled.print("Платформа: Other");
  #endif
  oled.setCursor(0, 5);
  oled.print("Сборка: " __DATE__);
  oled.setCursor(0, 6);
  oled.print("Серийный номер:");
  oled.setCursor(0, 7);
  oled.print(serial_number);
  oled.update();

  while(true) {
      buttons_tick();
      if(ok.isClick() || ok.isHold()) {
          return;
      }
  }
}
void networkSettings_sta() {
  String ssid = db[kk::wifi_ssid].toString();
  String pass = db[kk::wifi_pass].toString();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Сеть: ");
      oled.print(ssid);
      oled.setCursor(0, 2);
      oled.print("Пароль: ");
      oled.print(pass);
      oled.setCursor(0, 4);
      oled.print("OK - выход");
      oled.update();

      buttons_tick();
      
      if(ok.isClick()) {
          return;
      }
  }
}
void settingsMenu() {
  const char* settings_items[] = {
    "Яркость дисплея",
    "Сеть AP",
    "Сеть STA",
    "О прошивке",
    "Назад"
};
  const uint8_t settings_apps_count = sizeof(settings_items)/sizeof(settings_items[0]);
  int8_t settings_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Настройки", true, true, true);
  
  while(true) {
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(settings_apps_ptr == 0 ? ">" : " ");
    oled.print(settings_items[0]);

    // Рисуем остальные пункты если есть
    if(settings_apps_count > 1) {
      for(uint8_t i = 1; i < settings_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(settings_apps_ptr == i ? ">" : " ");
        oled.print(settings_items[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && settings_apps_ptr > 0) {
      settings_apps_ptr--;
    }
    if(down.isClick() && settings_apps_ptr < settings_apps_count - 1) {
      settings_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(settings_apps_ptr) {
        case 0: brightnessAdjust(); break;
        case 1: networkSettings_ap(); break;
        case 2: networkSettings_sta(); break;
        case 3: aboutFirmware(); break;
        case 4: exit(); resetButtons(); return;
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Настройки", true, true, true);
    }
  }
}
void loop() {
    static uint32_t timer = 0;
    buttons_tick();
    static uint32_t batteryTimer = millis(); // Таймер для батареи

    if (millis() - batteryTimer > 5000) {
        batteryTimer = millis();
        drawbattery();
    }
    if (up.isClick() || (up.isHold() && millis() - timer > 150)) {
        if (pointer > 0) {
            pointer--;
            if (pointer < top_item) top_item--;
        }
        timer = millis();
        updatePointer();
    }

    if (down.isClick() || (down.isHold() && millis() - timer > 150)) {
        if (pointer < ITEMS - 1) {
            pointer++;
            if (pointer >= top_item + VISIBLE_ITEMS) top_item++;
        }
        timer = millis();
        updatePointer();
    }
    if (ok.isClick()) {
      switch (pointer) {
        case 0: ShowFilesLittleFS(); break;
        case 1: create_settings(); break;
        case 2: settingsMenu(); break;
      }
    }
}
