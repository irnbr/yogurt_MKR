/*
 * This file is part of the W1209 firmware replacement project
 * (https://github.com/mister-grumbler/w1209-firmware).
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
 * Control functions for buttons.
 * The EXTI2 interrupt (5) is used to get signal on changing buttons state.
 */

 #include "buttons.h"
 #include "stm8s003/gpio.h"
 #include "menu.h"
 
 /* ================ Определения для работы с кнопками ================ */
 
 // Порт C используется для ввода с кнопок
 #define BUTTONS_PORT   PC_IDR
 
 // Биты кнопок на порту C
 #define BUTTON1_BIT    0x08  // PC.3
 #define BUTTON2_BIT    0x10  // PC.4
 #define BUTTON3_BIT    0x20  // PC.5
 
 /* ================ Статические переменные ================ */
 
 static unsigned char status;  // Текущее состояние кнопок
 static unsigned char diff;    // Флаги изменений состояния кнопок
 
 /* ================ Основные функции ================ */
 
 /**
  * @brief Инициализация кнопок
  * Настраивает GPIO на вход, включает прерывания и считывает начальное состояние
  */
 void initButtons(void)
 {
     // Настройка портов на вход с подтяжкой
     PC_CR1 |= BUTTON1_BIT | BUTTON2_BIT | BUTTON3_BIT;  // Включение подтягивающих резисторов
     PC_CR2 |= BUTTON1_BIT | BUTTON2_BIT | BUTTON3_BIT;  // Разрешение внешних прерываний
     
     // Чтение начального состояния кнопок (инвертированное, так как кнопки на землю)
     status = ~(BUTTONS_PORT & (BUTTON1_BIT | BUTTON2_BIT | BUTTON3_BIT));
     diff = 0;  // Сброс флагов изменений
     
     // Настройка прерываний на оба фронта (нажатие и отпускание)
     EXTI_CR1 |= 0x30;
 }
 
 /**
  * @brief Получение текущего состояния кнопок
  * @return Байт состояния, где каждый бит соответствует кнопке
  */
 unsigned char getButton(void)
 {
     return status;
 }
 
 /**
  * @brief Получение флагов изменений состояния кнопок
  * @return Байт, где установленные биты показывают изменившиеся кнопки
  */
 unsigned char getButtonDiff(void)
 {
     return diff;
 }
 
 /**
  * @brief Проверка состояния кнопки 1
  * @return true если кнопка 1 нажата
  */
 bool getButton1(void)
 {
     return status & BUTTON1_BIT;
 }
 
 /**
  * @brief Проверка состояния кнопки 2
  * @return true если кнопка 2 нажата
  */
 bool getButton2(void)
 {
     return status & BUTTON2_BIT;
 }
 
 /**
  * @brief Проверка состояния кнопки 3
  * @return true если кнопка 3 нажата
  */
 bool getButton3(void)
 {
     return status & BUTTON3_BIT;
 }
 
 /**
  * @brief Проверка изменения состояния кнопки 1
  * @return true если состояние кнопки 1 изменилось с последней проверки
  */
 bool isButton1(void)
 {
     if (diff & BUTTON1_BIT) {
         diff &= ~BUTTON1_BIT;  // Сброс флага изменения
         return true;
     }
     return false;
 }
 
 /**
  * @brief Проверка изменения состояния кнопки 2
  * @return true если состояние кнопки 2 изменилось с последней проверки
  */
 bool isButton2(void)
 {
     if (diff & BUTTON2_BIT) {
         diff &= ~BUTTON2_BIT;  // Сброс флага изменения
         return true;
     }
     return false;
 }
 
 /**
  * @brief Проверка изменения состояния кнопки 3
  * @return true если состояние кнопки 3 изменилось с последней проверки
  */
 bool isButton3(void)
 {
     if (diff & BUTTON3_BIT) {
         diff &= ~BUTTON3_BIT;  // Сброс флага изменения
         return true;
     }
     return false;
 }
 
 /**
  * @brief Обработчик прерывания от кнопок
  * Определяет какая кнопка была нажата/отпущена и отправляет событие в меню
  */
 void EXTI2_handler(void) __interrupt(5)
 {
     // Вычисление изменившихся кнопок (XOR предыдущего и текущего состояния)
     diff = status ^ ~(BUTTONS_PORT & (BUTTON1_BIT | BUTTON2_BIT | BUTTON3_BIT));
     // Обновление текущего состояния кнопок
     status = ~(BUTTONS_PORT & (BUTTON1_BIT | BUTTON2_BIT | BUTTON3_BIT));
 
     unsigned char event;
     
     // Определение какая кнопка вызвала прерывание
     if (isButton1()) {
         event = getButton1() ? MENU_EVENT_PUSH_BUTTON1 : MENU_EVENT_RELEASE_BUTTON1;
     } 
     else if (isButton2()) {
         event = getButton2() ? MENU_EVENT_PUSH_BUTTON2 : MENU_EVENT_RELEASE_BUTTON2;
     } 
     else if (isButton3()) {
         event = getButton3() ? MENU_EVENT_PUSH_BUTTON3 : MENU_EVENT_RELEASE_BUTTON3;
     } 
     else {
         return;  // Если прерывание вызвано не кнопками - выходим
     }
 
     // Передача события в систему меню
     feedMenu(event);
 }
