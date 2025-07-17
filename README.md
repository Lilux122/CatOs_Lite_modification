# CatOs Lite - читалка для ESP32
| *> CatOs Lite <* | [CatOs](https://github.com/CatDevCode/CatOs/) |
| --- | --- |

Прошивка для портативной читалки(шпоргалки) на базе ESP32 с OLED-дисплеем. 
## Особенности
- ⚙️ Системные настройки через веб-интерфейс
- 📶 Поддержка WiFi (STA и AP режимы)
- 📖 Файловый менеджер для LittleFS
- 🛠️ Сервисное меню с калибровкой

## Компоненты
- Микроконтроллер ESP32
- OLED дисплей 128x64 (I2C, 4 pins)
- 3 кнопоки управления
- Литий-ионный аккумулятор
## [МОЖНО ПРОШИТЬ ESP32 НА МОЁМ САЙТЕ](https://catdevcode.github.io/CatOs_webflasher/)
## Простой для DIY
1. Схема подключения

![scheme]((https://github.com/Lilux122/CatOs_Lite_modification/blob/main/photos/lol.png))

Чтобы увидеть более улучшеную схему подключения перейдите на этот сайт https://wokwi.com/projects/436710693132425217
## Создание изображений и загрузка
1. Запустите [imageProcessor.exe](https://github.com/AlexGyver/imageProcessor) (установите java)

![IMG1](https://github.com/CatDevCode/CatOs/blob/main/assets/img1.png)

2. Откройте изображение

![IMG2](https://github.com/CatDevCode/CatOs/blob/main/assets/img2.png)

3. Настройте размер и порог изображения для получения лучшего результата

![IMG3](https://github.com/CatDevCode/CatOs/blob/main/assets/img3.png)

4. Сделайте инверсию цвета (белый цвет будет отображаться на экране). И убедитесь что Result height и Result width стоят также как на изображении

![IMG4](https://github.com/CatDevCode/CatOs/blob/main/assets/img4.png)

5. Сохраните файл нажав SAVE, в папке image-processor появится файл .h . Также можно переименовать этот файл.

![IMG5](https://github.com/CatDevCode/CatOs/blob/main/assets/img5.png)

## Библиотеки
- [GyverOled](https://github.com/GyverLibs/GyverOLED/)
- [GyverButton(Старое, но работает отлично)](https://github.com/GyverLibs/GyverButton)
- [Settings](https://github.com/GyverLibs/Settings)
- PS. Все библиотеки от гайвера

## Установка
1. Установите [PlatformIO](https://platformio.org/)
```bash
pip install platformio
```
2. Клонируйте репозиторий:
```bash
git clone https://github.com/CatDevCode/CatOs_Lite.git
```
3. Перейдите в папку с проектом:
```bash
cd CatOs_Lite
```
4. Сбилдите проект
```bash
pio run
```
5. Загрузите проект на ESP32
```bash
pio run --target upload 
```
## Кредиты
- Спасибо [Алексу Гайверу](https://github.com/GyverLibs/) за библиотеки ❤
- Спасибо проекту [MicroReader](https://github.com/Nich1con/microReader/) за некоторые функции.
## Проект открыт для Pull-реквестов
## Сделано с любовью ❤
