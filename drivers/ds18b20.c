//***********************************************************
// Набор процедур и функций для работы с сетью MicroLan
//
//#include "driverlib/MSP430FR5xx_6xx/driverlib.h"
#include <driverlib.h>
#include "delay.h"		// Задержки
#include "system.h"
#include "microlan.h"

//*****************************************************************/
// Глобальные переменные состояний поиска
//*****************************************************************/
// Внешние Глобальные переменные
extern unsigned char LCDbuffer[8][22];
extern unsigned char crc8;
extern unsigned char ROM_NO[11];	      // буфер для поиска приборов
extern char result;
extern unsigned char termometr;	      // Количество найденных приборов.
extern unsigned char ds_sensor;
/*
// */
//**************************************************************************/
unsigned char Term_Char[16][6];	      // Температура в ASCII для HTTP и LCD
// формат данных в массиве Term_Char[]:
// Term_Char[][0] // знак температуры (+ или -);
// Term_Char[][1] // десятки;
// Term_Char[][2] // единицы;
// Term_Char[][3] // точка (.);
// Term_Char[][4] // дробь - десятые;
// Term_Char[][5] // признак конца строки (0);

//**************************************************************************/
extern unsigned char sensor,ind_DS2408;
extern unsigned char snum[8];
extern unsigned char lastD;
extern unsigned char EditParameter;	  // Флаг состояния редактирования параметров модуля(1-редактирование)
extern unsigned int time_ml,t_ds18b20;
extern unsigned int TickCount;	        // Счетчик секунд

//**************************************************************************/

extern unsigned char list_ds[2][13]; // "Список найденных приборов" и параметры размещения в "Таблице приборов"


