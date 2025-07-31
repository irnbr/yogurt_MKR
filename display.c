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
 * Control functions for the seven-segment display (SSD).
 */

 #include "display.h"
 #include "stm8s003/gpio.h"
 
 /* Определения для работы с дисплеем */
 // Порт A управляет сегментами: B, F
 // Маска: 0000 0110
 #define SSD_SEG_BF_PORT     PA_ODR
 #define SSD_BF_PORT_MASK    0b00000110
 // Порт C управляет сегментами: C, G
 // Маска: 1100 0000
 #define SSD_SEG_CG_PORT     PC_ODR
 #define SSD_CG_PORT_MASK    0b11000000
 // Порт D управляет сегментами: A, E, D, P
 // Маска: 0010 1110
 #define SSD_SEG_AEDP_PORT   PD_ODR
 #define SSD_AEDP_PORT_MASK  0b00101110
 
 // Биты управления сегментами:
 #define SSD_SEG_A_BIT       0x20  // PD.5
 #define SSD_SEG_B_BIT       0x04  // PA.2
 #define SSD_SEG_C_BIT       0x80  // PC.7
 #define SSD_SEG_D_BIT       0x08  // PD.3
 #define SSD_SEG_E_BIT       0x02  // PD.1
 #define SSD_SEG_F_BIT       0x02  // PA.1
 #define SSD_SEG_G_BIT       0x40  // PC.6
 #define SSD_SEG_P_BIT       0x04  // PD.2 (десятичная точка)
 
 // Порты управления разрядами (цифрами):
 #define SSD_DIGIT_12_PORT   PB_ODR  // Порт B управляет цифрами 1 и 2
 #define SSD_DIGIT_3_PORT    PD_ODR  // Порт D управляет цифрой 3
 
 // Биты управления разрядами:
 #define SSD_DIGIT_1_BIT     0x10  // PB.4
 #define SSD_DIGIT_2_BIT     0x20  // PB.5
 #define SSD_DIGIT_3_BIT     0x10  // PD.4
 
 // Таблица преобразования hex-значений в символы
 const unsigned char Hex2CharMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A',
                                      'B', 'C', 'D', 'E', 'F'
                                     };
 
 // Текущий активный разряд для мультиплексирования
 static unsigned char activeDigitId;
 // Буферы дисплея для сегментов, управляемых портами A/C и D
 static unsigned char displayAC[3];
 static unsigned char displayD[3];
 
 // Прототипы статических функций
 static void enableDigit(unsigned char id);
 static void setDigit(unsigned char id, unsigned char val, bool dot);
 
 // Флаги состояния дисплея
 static bool displayOff;  // Состояние вкл/выкл дисплея
 static bool testMode;    // Режим тестирования дисплея
 
 /**
  * @brief Инициализация дисплея - настройка GPIO и начальных параметров
  * @note Настраивает соответствующие биты GPIO, инициализирует переменные
  *       и включает тестовый режим дисплея.
  */
 void initDisplay(void)
 {
     // Настройка направлений и управляющих регистров для всех сегментов
     PA_DDR |= SSD_SEG_B_BIT | SSD_SEG_F_BIT;
     PA_CR1 |= SSD_SEG_B_BIT | SSD_SEG_F_BIT;
     PB_DDR |= SSD_DIGIT_1_BIT | SSD_DIGIT_2_BIT;
     PB_CR1 |= SSD_DIGIT_1_BIT | SSD_DIGIT_2_BIT;
     PC_DDR |= SSD_SEG_C_BIT | SSD_SEG_G_BIT;
     PC_CR1 |= SSD_SEG_C_BIT | SSD_SEG_G_BIT;
     PD_DDR |= SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT | SSD_SEG_P_BIT | SSD_DIGIT_3_BIT;
     PD_CR1 |= SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT | SSD_SEG_P_BIT | SSD_DIGIT_3_BIT;
     
     // Инициализация состояния дисплея
     displayOff = false;
     activeDigitId = 0;
     setDisplayTestMode(true, "");
 }
 
 /**
  * @brief Обновление состояния дисплея (вызывается из прерывания таймера)
  * @note Должна быть максимально быстрой, так как вызывается в прерывании.
  *       Использует данные из буфера дисплея для управления GPIO.
  */
 void refreshDisplay(void)
 {
     // Сначала отключаем все разряды
     enableDigit(3);
 
     if (displayOff) {
         return;
     }
 
     // Обновляем состояние сегментов из буферов
     SSD_SEG_BF_PORT &= ~SSD_BF_PORT_MASK;
     SSD_SEG_BF_PORT |= displayAC[activeDigitId] & SSD_BF_PORT_MASK;
     SSD_SEG_CG_PORT &= ~SSD_CG_PORT_MASK;
     SSD_SEG_CG_PORT |= displayAC[activeDigitId] & SSD_CG_PORT_MASK;
     SSD_SEG_AEDP_PORT &= ~SSD_AEDP_PORT_MASK;
     SSD_SEG_AEDP_PORT |= displayD[activeDigitId];
     
     // Включаем текущий разряд
     enableDigit(activeDigitId);
 
     // Переключаемся на следующий разряд (кольцевой буфер)
     if (activeDigitId > 1) {
         activeDigitId = 0;
     } else {
         activeDigitId++;
     }
 }
 
 /**
  * @brief Включение/выключение тестового режима дисплея
  * @param val true - включить тестовый режим, false - выключить
  * @param str строка для отображения в тестовом режиме (если пустая - "888")
  */
 void setDisplayTestMode(bool val, char* str)
 {
     if (!testMode && val) {
         if (*str == 0) {
             setDisplayStr("888");
         } else {
             setDisplayStr(str);
         }
     }
 
     testMode = val;
 }
 
 /**
  * @brief Включение/выключение дисплея
  * @param val true - выключить дисплей, false - включить
  */
 void setDisplayOff(bool val)
 {
     displayOff = val;
 }
 
 /**
  * @brief Установка состояния десятичной точки для указанного разряда
  * @param id номер разряда (0..2)
  * @param val true - включить точку, false - выключить
  */
 void setDisplayDot(unsigned char id, bool val)
 {
     if (val) {
         displayD[id] |= SSD_SEG_P_BIT;
     } else {
         displayD[id] &= ~SSD_SEG_P_BIT;
     }
 }
 
 /**
  * @brief Отображение строки на дисплее
  * @param val указатель на строку с нулевым окончанием
  */
 void setDisplayStr(const unsigned char* val)
 {
     unsigned char i, d;
 
     // Подсчет количества необходимых разрядов для отображения строки
     for (i = 0, d = 0; *(val + i) != 0; i++, d++) {
         if (*(val + i) == '.' && i > 0 && *(val + i - 1) != '.') d--;
     }
 
     // Ограничение количества разрядов (максимум 3)
     if (d > 3) {
         d = 3;
     }
 
     // Отключение неиспользуемых разрядов
     for (i = 3 - d; i > 0; i--) {
         setDigit(3 - i, ' ', false);
     }
 
     // Установка значений для разрядов с учетом точек
     for (i = 0; d != 0 && *val + i != 0; i++, d--) {
         if (*(val + i + 1) == '.') {
             setDigit(d - 1, *(val + i), true);
             i++;
         } else {
             setDigit(d - 1, *(val + i), false);
         }
     }
 }
 
 /**
  * @brief Включение указанного разряда дисплея
  * @param id номер разряда (0-2), другие значения отключают все разряды
  * @note ID = 0 соответствует правому разряду на дисплее
  */
 static void enableDigit(unsigned char id)
 {
     switch (id) {
     case 0:  // Включить только разряд 1 (правый)
         SSD_DIGIT_12_PORT &= ~SSD_DIGIT_1_BIT;
         SSD_DIGIT_12_PORT |= SSD_DIGIT_2_BIT;
         SSD_DIGIT_3_PORT |= SSD_DIGIT_3_BIT;
         break;
 
     case 1:  // Включить только разряд 2 (средний)
         SSD_DIGIT_12_PORT &= ~SSD_DIGIT_2_BIT;
         SSD_DIGIT_12_PORT |= SSD_DIGIT_1_BIT;
         SSD_DIGIT_3_PORT |= SSD_DIGIT_3_BIT;
         break;
 
     case 2:  // Включить только разряд 3 (левый)
         SSD_DIGIT_3_PORT &= ~SSD_DIGIT_3_BIT;
         SSD_DIGIT_12_PORT |= SSD_DIGIT_1_BIT | SSD_DIGIT_2_BIT;
         break;
 
     default:  // Отключить все разряды
         SSD_DIGIT_12_PORT |= SSD_DIGIT_1_BIT | SSD_DIGIT_2_BIT;
         SSD_DIGIT_3_PORT |= SSD_DIGIT_3_BIT;
         break;
     }
 }
 

/**
 * @brief Sets bits within display's buffer appropriate to given value.
 *  So this symbol will be shown on display during refreshDisplay() call.
 *  When test mode is enabled the display's buffer will not be updated.
 *
 * The list of segments as they located on display:
 *  _2_       _1_       _0_
 *  <A>       <A>       <A>
 * F   B     F   B     F   B
 *  <G>       <G>       <G>
 * E   C     E   C     E   C
 *  <D> (P)   <D> (P)   <D> (P)
 *
 * @param id
 *  Identifier of character's position on display.
 *  Accepted values are: 0, 1, 2.
 * @param val
 *  Character to be represented on SSD at position being designated by id.
 *  Due to limited capabilities of SSD some characters are shown in a very
 *  schematic manner.
 *  Accepted values are: ANY.
 *  But only actual characters are defined. For the rest of values the
 *  '_' symbol is shown.
 * @param dot
 *  Enable dot (decimal point) for the character.
 *  Accepted values true/false.
 *
 */
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
  * @brief Установка символа в указанный разряд дисплея
  * @param id номер разряда (0-2)
  * @param val символ для отображения
  * @param dot отображать десятичную точку (true/false)
  * @note В тестовом режиме буфер дисплея не обновляется
  */
 static void setDigit(unsigned char id, unsigned char val, bool dot)
 {
     // Проверка допустимости номера разряда
     if (id > 2) return;
 
     // В тестовом режиме не обновляем буфер
     if (testMode) return;
 
     // Установка битов в буферах в соответствии с требуемым символом
     switch (val) {
     case '-':
         displayAC[id] = SSD_SEG_G_BIT;
         displayD[id] = 0;
         break;
 
     case ' ':
         displayAC[id] = 0;
         displayD[id] = 0;
         break;
 
     case '0':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_F_BIT | SSD_SEG_C_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case '1':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT;
         displayD[id] = 0;
         break;
 
     case '2':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case '3':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT;
         break;
 
     case '4':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = 0;
         break;
 
     case '5':
         displayAC[id] = SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT;
         break;
 
     case '6':
         displayAC[id] = SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case '7':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT;
         displayD[id] = SSD_SEG_A_BIT;
         break;
 
     case '8':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case '9':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT;
         break;
 
     // Обработка буквенных символов
     case 'A':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'B':
         displayAC[id] = SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'C':
         displayAC[id] = SSD_SEG_F_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'D':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'E':
         displayAC[id] = SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'F':
         displayAC[id] = SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'H':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_C_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_E_BIT;
         break;
 
     case 'L':
         displayAC[id] = SSD_SEG_F_BIT;
         displayD[id] = SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'N':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_F_BIT | SSD_SEG_C_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'O':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_F_BIT | SSD_SEG_C_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'P':
         displayAC[id] = SSD_SEG_B_BIT | SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'R':
         displayAC[id] = SSD_SEG_F_BIT;
         displayD[id] = SSD_SEG_A_BIT | SSD_SEG_E_BIT;
         break;
 
     case 'T':
         displayAC[id] = SSD_SEG_F_BIT | SSD_SEG_G_BIT;
         displayD[id] = SSD_SEG_D_BIT | SSD_SEG_E_BIT;
         break;
 
     default:  // Для неопределенных символов показываем нижнее подчеркивание
         displayAC[id] = 0;
         displayD[id] = SSD_SEG_D_BIT;
     }
 
     // Установка/сброс бита десятичной точки
     if (dot) {
         displayD[id] |= SSD_SEG_P_BIT;
     } else {
         displayD[id] &= ~SSD_SEG_P_BIT;
     }

    return;
}
