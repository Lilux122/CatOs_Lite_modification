#include <Arduino.h>
int8_t pointer = 0;
int8_t top_item = 0;  // Верхний видимый пункт списка


#define ITEMS 3  // Исправлено на реальное количество отображаемых элементов
#define VISIBLE_ITEMS 3  // Количество одновременно отображаемых пунктов


const char* menu_items[] = {
    " Читалка",
    " Веб меню",
    " Настройки",
};