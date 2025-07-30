/*
* This file is part of the firmware for yogurt maker project
 * (https://github.com/mister-grumbler/yogurt-maker).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "adc.h"
#include "buttons.h"
#include "display.h"
#include "menu.h"
#include "params.h"
#include "relay.h"
#include "timer.h"

/* Макросы для управления прерываниями */
#define INTERRUPT_ENABLE    __asm rim __endasm;  /* Разрешить прерывания */
#define INTERRUPT_DISABLE   __asm sim __endasm;  /* Запретить прерывания */
#define WAIT_FOR_INTERRUPT  __asm wfi __endasm;  /* Ожидать прерывание */

/**
 * @brief Конкатенация двух строк
 * @param from Указатель на исходную строку
 * @param to Указатель на целевую строку (должен иметь достаточный размер)
 */
void strConcat(unsigned char * from, unsigned char * to)
{
    unsigned char i, s;

    /* Находим конец целевой строки */
    for (i = 0; to[i] != 0; i++);

    s = i; /* Сохраняем позицию конца строки */

    /* Копируем содержимое исходной строки */
    for (i = 0; from[i] != 0; i++) {
        to[s + i] = from[i];
    }

    to[s + i] = 0; /* Устанавливаем завершающий нуль-символ */
}

/**
 * @brief Главная функция программы
 * @return Технически никогда не возвращает значение (бесконечный цикл)
 */
int main(void)
{
    /* Буферы для работы с дисплеем */
    static unsigned char* stringBuffer[7];  /* Буфер для строковых данных */
    static unsigned char* timerBuffer[5];   /* Буфер для времени */
    unsigned char paramMsg[] = {'P', '0', 0}; /* Шаблон сообщения параметра */

    /* Инициализация всех модулей системы */
    initMenu();            /* Меню */
    initButtons();         /* Кнопки */
    initParamsEEPROM();    /* Параметры в EEPROM */
    initDisplay();         /* Дисплей */
    initADC();             /* АЦП и датчик температуры */
    initRelay();           /* Управление реле */
    initTimer();           /* Таймеры системы */

    INTERRUPT_ENABLE;      /* Разрешаем обработку прерываний */

    /* Основной бесконечный цикл программы */
    while (true) {
        /* Отключаем тестовый режим дисплея после первой секунды работы */
        if (getUptimeSeconds() > 0) {
            setDisplayTestMode(false, "");
        }

        /* Обработка текущего состояния меню */
        if (getMenuDisplay() == MENU_ROOT) {
            /* В основном меню попеременно показываем температуру и таймер */
            if (isRelayEnabled() && getUptimeSeconds() & 0x08) {
                stringBuffer[0] = 0; /* Очищаем буфер */

                if (isFTimer()) {
                    /* Мигаем точкой между часами и минутами */
                    if ((getUptimeTicks() & 0x100)) {
                        uptimeToString((unsigned char*)stringBuffer, "Ttt");
                    } else {
                        uptimeToString((unsigned char*)stringBuffer, "T.tt");
                    }
                } else {
                    /* Если таймер не активен - показываем "No Timer Running" */
                    setDisplayStr("N.T.R.");
                    continue; /* Переходим к следующей итерации */
                }

                setDisplayStr((char*)stringBuffer);
            } else {
                /* Показываем текущую температуру */
                int temp = getTemperature();
                itofpa(temp, (char*)stringBuffer, 0);
                setDisplayStr((char*)stringBuffer);

                /* Проверка и индикация граничных значений температуры */
                if (getParamById(PARAM_OVERHEAT_INDICATION)) {
                    if (temp < getParamById(PARAM_MIN_TEMPERATURE)) {
                        setDisplayStr("LLL"); /* Температура ниже минимальной */
                    } else if (temp > getParamById(PARAM_MAX_TEMPERATURE)) {
                        setDisplayStr("HHH"); /* Температура выше максимальной */
                    }
                }
            }
        } 
        else if (getMenuDisplay() == MENU_SET_TIMER) {
            /* Режим установки таймера ферментации */
            paramToString(PARAM_FERMENTATION_TIME, (char*)stringBuffer);
            setDisplayStr((char*)stringBuffer);
        } 
        else if (getMenuDisplay() == MENU_SELECT_PARAM) {
            /* Режим выбора параметра (P0, P1...) */
            paramMsg[1] = '0' + getParamId();
            setDisplayStr((unsigned char*)&paramMsg);
        } 
        else if (getMenuDisplay() == MENU_CHANGE_PARAM) {
            /* Режим изменения параметра */
            paramToString(getParamId(), (char*)stringBuffer);
            setDisplayStr((char*)stringBuffer);
        } 
        else {
            /* Неизвестное состояние меню - показываем ошибку */
            setDisplayStr("ERR");
            /* Мигаем дисплеем при ошибке */
            setDisplayOff((bool)((unsigned char)getUptimeTicks() & 0x80));
        }

        WAIT_FOR_INTERRUPT; /* Ожидаем следующее прерывание */
    };
}
