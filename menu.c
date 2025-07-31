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

/**
 * Реализация меню приложения.
 */

 #include "menu.h"
 #include "buttons.h"
 #include "display.h"
 #include "params.h"
 #include "timer.h"
 #include "relay.h"
 
 // Константы времени для работы меню
 #define MENU_1_SEC_PASSED   32      // 1 секунда в тиках таймера
 #define MENU_3_SEC_PASSED   (MENU_1_SEC_PASSED * 3)  // 3 секунды
 #define MENU_5_SEC_PASSED   (MENU_1_SEC_PASSED * 5)  // 5 секунд
 #define MENU_AUTOINC_DELAY  (MENU_1_SEC_PASSED / 8)  // Задержка автоинкремента
 
 // Статические переменные меню
 static unsigned char menuDisplay;    // Текущее отображаемое меню
 static unsigned char menuState;     // Текущее состояние меню
 /* Счетчик таймера меню. Увеличивается при каждом вызове refreshMenu().
    Используется для обработки таймаутов меню и действий при удержании кнопки. */
 static unsigned int timer;
 
 // Прототипы внутренних функций (если есть)
 
 /**
  * @brief Инициализация локальных переменных меню.
  */
 void initMenu(void)
 {
     timer = 0;
     menuState = menuDisplay = MENU_ROOT;  // Начинаем с корневого меню
 }
 
 /**
  * @brief Получение текущего состояния меню для отображения.
  * @return Текущее отображаемое меню
  */
 unsigned char getMenuDisplay(void)
 {
     return menuDisplay;
 }
 
 /**
  * @brief Обновление состояния меню приложения и обработка событий.
  * @param event Событие меню (нажатие/отпускание кнопок или проверка таймера)
  *
  * Возможные состояния меню:
  *  MENU_ROOT          - Корневое меню
  *  MENU_SELECT_PARAM  - Выбор параметра
  *  MENU_CHANGE_PARAM  - Изменение параметра
  *  MENU_SET_TIMER     - Установка таймера
  *
  * Возможные события:
  *  MENU_EVENT_PUSH_BUTTON1..3    - Нажатие кнопки 1..3
  *  MENU_EVENT_RELEASE_BUTTON1..3 - Отпускание кнопки 1..3
  *  MENU_EVENT_CHECK_TIMER        - Проверка таймера
  */
 void feedMenu(unsigned char event)
 {
     bool blink;  // Флаг мигания дисплеем
 
     // Обработка в зависимости от текущего состояния меню
     if (menuState == MENU_ROOT) {
         // Корневое меню
         switch (event) {
         case MENU_EVENT_PUSH_BUTTON1:
             timer = 0;
             menuDisplay = MENU_SET_TIMER;
             break;
 
         case MENU_EVENT_RELEASE_BUTTON1:
             if (timer < MENU_5_SEC_PASSED) {
                 menuState = MENU_SET_TIMER;
             }
             timer = 0;
             break;
 
         case MENU_EVENT_CHECK_TIMER:
             if (timer > MENU_3_SEC_PASSED) {
                 timer = 0;
 
                 if (getButton1()) {
                     // Долгое нажатие кнопки 1 - вход в меню параметров
                     setParamId(0);
                     menuState = menuDisplay = MENU_SELECT_PARAM;
                 } else {
                     if (getButton2()) {    // Вкл/выкл термостата
                         if (isRelayEnabled() && !isFTimer()) {
                             enableRelay(false);
                         } else {
                             enableRelay(true);
                         }
                     } else if (getButton3()) { // Старт/стоп таймера ферментации
                         if (isFTimer()) {
                             stopFTimer();
                             enableRelay(false);
                         } else {
                             startFTimer();
                             enableRelay(true);
                         }
                     }
                 }
             }
             break;
 
         default:
             if (timer > MENU_5_SEC_PASSED) {
                 timer = 0;
                 menuState = menuDisplay = MENU_ROOT;
             }
             break;
         }
     } 
     else if (menuState == MENU_SELECT_PARAM) {
         // Меню выбора параметра
         switch (event) {
         case MENU_EVENT_PUSH_BUTTON1:
             menuState = menuDisplay = MENU_CHANGE_PARAM;
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON1:
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON2:
             incParamId();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON2:
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON3:
             decParamId();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON3:
             timer = 0;
             break;
 
         case MENU_EVENT_CHECK_TIMER:
             // Автоинкремент при удержании кнопки
             if (timer > MENU_1_SEC_PASSED + MENU_AUTOINC_DELAY) {
                 if (getButton2()) {
                     incParamId();
                     timer = MENU_1_SEC_PASSED;
                 } else if (getButton3()) {
                     decParamId();
                     timer = MENU_1_SEC_PASSED;
                 }
             }
 
             // Таймаут возврата в корневое меню
             if (timer > MENU_5_SEC_PASSED) {
                 timer = 0;
                 setParamId(0);
                 storeParams();
                 menuState = menuDisplay = MENU_ROOT;
             }
             break;
 
         default:
             break;
         }
     } 
     else if (menuState == MENU_CHANGE_PARAM) {
         // Меню изменения параметра
         switch (event) {
         case MENU_EVENT_PUSH_BUTTON1:
             menuState = menuDisplay = MENU_SELECT_PARAM;
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON1:
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON2:
             incParam();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON2:
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON3:
             decParam();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON3:
             timer = 0;
             break;
 
         case MENU_EVENT_CHECK_TIMER:
             // Автоинкремент при удержании кнопки
             if (timer > MENU_1_SEC_PASSED + MENU_AUTOINC_DELAY) {
                 if (getButton2()) {
                     incParam();
                     timer = MENU_1_SEC_PASSED;
                 } else if (getButton3()) {
                     decParam();
                     timer = MENU_1_SEC_PASSED;
                 }
             }
 
             // Возврат в меню выбора параметра при долгом нажатии кнопки 1
             if (getButton1() && timer > MENU_3_SEC_PASSED) {
                 timer = 0;
                 menuState = menuDisplay = MENU_SELECT_PARAM;
                 break;
             }
 
             // Таймаут возврата в корневое меню
             if (timer > MENU_5_SEC_PASSED) {
                 timer = 0;
                 storeParams();
                 menuState = menuDisplay = MENU_ROOT;
             }
             break;
 
         default:
             break;
         }
     } 
     else if (menuState == MENU_SET_TIMER) {
         // Меню установки таймера
         switch (event) {
         case MENU_EVENT_PUSH_BUTTON1:
             timer = 0;
             menuDisplay = MENU_ROOT;
             setDisplayOff(false);
             break;
 
         case MENU_EVENT_RELEASE_BUTTON1:
             if (timer < MENU_5_SEC_PASSED) {
                 storeParams();
                 menuState = MENU_ROOT;
                 setDisplayOff(false);
             }
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON2:
             setParamId(PARAM_FERMENTATION_TIME);
             incParam();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON2:
             timer = 0;
             break;
 
         case MENU_EVENT_PUSH_BUTTON3:
             setParamId(PARAM_FERMENTATION_TIME);
             decParam();
             // Продолжение в следующий case (нет break)
         case MENU_EVENT_RELEASE_BUTTON3:
             timer = 0;
             break;
 
         case MENU_EVENT_CHECK_TIMER:
             // Определение необходимости мигания дисплеем
             if (getButton2() || getButton3()) {
                 blink = false;
             } else {
                 blink = (bool)((unsigned char)getUptimeTicks() & 0x80);
             }
 
             // Автоинкремент при удержании кнопки
             if (timer > MENU_1_SEC_PASSED + MENU_AUTOINC_DELAY) {
                 setParamId(PARAM_FERMENTATION_TIME);
 
                 if (getButton2()) {
                     incParam();
                     timer = MENU_1_SEC_PASSED;
                 } else if (getButton3()) {
                     decParam();
                     timer = MENU_1_SEC_PASSED;
                 }
             }
 
             setDisplayOff(blink);
 
             // Таймаут возврата в корневое меню или переход в меню параметров
             if (timer > MENU_5_SEC_PASSED) {
                 timer = 0;
 
                 if (getButton1()) {
                     menuState = menuDisplay = MENU_SELECT_PARAM;
                     setDisplayOff(false);
                     break;
                 }
 
                 storeParams();
                 menuState = menuDisplay = MENU_ROOT;
                 setDisplayOff(false);
             }
             break;
 
         default:
             break;
         }
     }
 }
 
 /**
  * @brief Функция обновления меню, вызываемая из прерывания таймера.
  * @note Должна быть максимально быстрой, так как вызывается в прерывании.
  *       Обрабатывает всю временную логику меню: быстрое изменение значений
  *       при удержании кнопки, возврат в корневое меню при бездействии и т.д.
  */
 void refreshMenu(void)
 {
     timer++;
     feedMenu(MENU_EVENT_CHECK_TIMER);
 }
