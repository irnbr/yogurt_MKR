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
 * Функции управления аналого-цифровым преобразователем (АЦП).
 * Используется прерывание ADC1 (22) по завершению преобразования.
 * Вход АЦП - порт D6 (pin 3, канал AIN6).
 */

 #include "adc.h"
 #include "stm8s003/adc.h"
 #include "params.h"
 
 /* ================== Константы и определения ================== */
 
 #define ADC_AVERAGING_BITS      4       // Количество битов для усреднения (2^4=16 значений)
 #define ADC_RAW_TABLE_SIZE      (sizeof(rawAdc) / sizeof(rawAdc[0]))  // Размер таблицы ADC
 #define ADC_RAW_TABLE_BASE_TEMP -520    // Базовое значение температуры (в десятых градуса Цельсия)
 
 /* 
  * Таблица соответствия значений АЦП температуре
  * Диапазон: от -52°C до 112°C (шаг 1°C, всего 165 значений)
  */
 const unsigned int rawAdc[] = {
    974, 971, 967, 964, 960, 956, 953, 948, 944, 940,
    935, 930, 925, 920, 914, 909, 903, 897, 891, 884,
    877, 871, 864, 856, 849, 841, 833, 825, 817, 809,
    800, 791, 782, 773, 764, 754, 745, 735, 725, 715,
    705, 695, 685, 675, 664, 654, 644, 633, 623, 612,
    601, 591, 580, 570, 559, 549, 538, 528, 518, 507,
    497, 487, 477, 467, 457, 448, 438, 429, 419, 410,
    401, 392, 383, 375, 366, 358, 349, 341, 333, 326,
    318, 310, 303, 296, 289, 282, 275, 269, 262, 256,
    250, 244, 238, 232, 226, 221, 215, 210, 205, 200,
    195, 191, 186, 181, 177, 173, 169, 165, 161, 157,
    153, 149, 146, 142, 139, 136, 132, 129, 126, 123,
    120, 117, 115, 112, 109, 107, 104, 102, 100, 97,
    95, 93, 91, 89, 87, 85, 83, 81, 79, 78,
    76, 74, 73, 71, 69, 68, 67, 65, 64, 62,
    61, 60, 58, 57, 56, 55, 54, 53, 52, 51,
    49, 48, 47, 47, 46
 };
 
 /* ================== Статические переменные ================== */
 
 static unsigned int result;      // Последнее считанное значение АЦП
 static unsigned long averaged;   // Накопленное значение для усреднения
 
 /* ================== Основные функции ================== */
 
 /**
  * @brief Инициализация АЦП
  * Настраивает предделитель, канал измерения и разрешает прерывания
  */
 void initADC(void)
 {
     ADC_CR1 |= 0x70;    // Установка предделителя f/18 (SPSEL)
     ADC_CSR |= 0x06;    // Выбор канала AIN6
     ADC_CSR |= 0x20;    // Разрешение прерывания по завершению преобразования (EOCIE)
     ADC_CR1 |= 0x01;    // Включение питания АЦП
     
     result = 0;         // Сброс последнего результата
     averaged = 0;       // Сброс накопленного значения
 }
 
 /**
  * @brief Запуск преобразования АЦП
  */
 void startADC(void)
 {
     ADC_CR1 |= 0x01;    // Установка бита запуска преобразования
 }
 
 /**
  * @brief Получение сырого результата последнего преобразования
  * @return Сырое значение АЦП (0-1023)
  */
 unsigned int getAdcResult(void)
 {
     return result;
 }
 
 /**
  * @brief Получение усредненного значения АЦП
  * @return Усредненное значение (16 последних измерений)
  */
 unsigned int getAdcAveraged(void)
 {
     return (unsigned int)(averaged >> ADC_AVERAGING_BITS);
 }
 
 /**
  * @brief Расчет температуры на основе усредненного значения АЦП
  * @return Температура в десятых градуса Цельсия с учетом калибровки
  */
 int getTemperature(void)
 {
     unsigned int val = averaged >> ADC_AVERAGING_BITS;  // Текущее усредненное значение
     unsigned char rightBound = ADC_RAW_TABLE_SIZE;      // Правая граница поиска
     unsigned char leftBound = 0;                        // Левая граница поиска
 
     /* Бинарный поиск в таблице соответствия */
     while ((rightBound - leftBound) > 1) {
         unsigned char midId = (leftBound + rightBound) >> 1;
 
         if (val > rawAdc[midId]) {
             rightBound = midId;
         } else {
             leftBound = midId;
         }
     }
 
     /* Линейная интерполяция между ближайшими значениями */
     if (val >= rawAdc[leftBound]) {
         val = leftBound * 10;
     } else {
         val = (rightBound * 10) - ((val - rawAdc[rightBound]) * 10) / 
               (rawAdc[leftBound] - rawAdc[rightBound]);
     }
 
     /* Применение температурной коррекции */
     return ADC_RAW_TABLE_BASE_TEMP + val + getParamById(PARAM_TEMPERATURE_CORRECTION);
 }
 
 /**
  * @brief Обработчик прерывания АЦП по завершению преобразования
  */
 void ADC1_EOC_handler(void) __interrupt(22)
 {
     /* Чтение результата преобразования (10-битное значение) */
     result = ADC_DRH << 2;      // Старшие 8 бит
     result |= ADC_DRL;          // Младшие 2 бита
     ADC_CSR &= ~0x80;           // Сброс флага завершения преобразования (EOC)
 
     /* Скользящее усреднение результатов */
     if (averaged == 0) {
         averaged = result << ADC_AVERAGING_BITS;  // Первое значение
     } else {
         // Добавление нового значения с учетом веса старых
         averaged += result - (averaged >> ADC_AVERAGING_BITS);
     }
 }
