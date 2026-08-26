//********************************************************************
//  utilites.c
//  Различные вспомогательные функции
//--------------------------------------------------------------------
//
//--------------------------------------------------------------------
//
#include <time.h>
#include "driverlib.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "microlan.h"
#include "rdx0154.h"            // Утилиты обслуживания LCD и клавиатуры
//
// Информация из таблицы распределения памяти для TLV в даташите msp430fr5959(стр. 113):
// Калибровочные значения Термодатчика, при Опорном Напряжении 1,2В
#define CALADC12_12V_30C  *((unsigned int *)0x1A1A) // Калибровочное значение термодатчика для 30 C
#define CALADC12_12V_85C  *((unsigned int *)0x1A1C) // Калибровочное значение термодатчика для 85 C
// Калибровочные значения Термодатчика, при Опорном Напряжении 2,0В
#define CALADC12_20V_30C  *((unsigned int *)0x1A1E) // Калибровочное значение термодатчика для 30 C
#define CALADC12_20V_85C  *((unsigned int *)0x1A20) // Калибровочное значение термодатчика для 85 C
// Калибровочные значения Термодатчика, при Опорном Напряжении 2,5В
#define CALADC12_25V_30C  *((unsigned int *)0x1A22) // Калибровочное значение термодатчика для 30 C
#define CALADC12_25V_85C  *((unsigned int *)0x1A24) // Калибровочное значение термодатчика для 85 C

volatile float temperatureDegC;

extern struct skm8_packet pack;                 // Пакет данных для передачи на сервер.
extern struct skm8_packet config;               // Пакет данных для передачи на сервер.
extern unsigned int pack_time;                  // Время следующей перадачи данных. !!! В ТИКАХ WDT !!!
extern unsigned int pack_time_sinc;             // Время для синхронизации формирования архива.
extern signed char server;

extern struct rxM_buf RxMbuf;                   // буфер модема
extern struct buf_str TxMbuf;                   //  -"-
extern struct remote_data remote;
extern unsigned char display;                   // наличие дисплея:     0-дисплей есть, 1-нет.
extern unsigned char indi;                      // разрешить индикацию
signed int temrat,temrat_2,t1,t2,t3,t4,t5,t6,t7;
extern unsigned int temp;
unsigned int vcc;
extern unsigned long volt;//, uuso;
extern unsigned long cnters;

unsigned int ixx,uxx,res_ult,count_result;
extern unsigned char delay_15;
char delay_comm;                                // флаг задержки чтения архива
extern unsigned char lastrec[];                 // номер блока и начало последней записи в архиве НеШТатных ситуаций
extern time_t request_unix;
extern unsigned long date_2000;                 // = 946684800;  // время: Sat, 01 Jan 2000 00:00:00 GMT
extern unsigned long req_date;                  // запрашиваемая дата в формате юникс, в минутах.
extern char *pnt;
extern unsigned int  vtruchannja;               //
extern struct tm recivedate;
extern uint16_t time_hour;
extern unsigned int ind_after_last_transmit;    // используется для подсчета количества часовых записей,
                                                // которые нужно передать на сервер их часового (1 или 2) архива.
extern unsigned int patern_time_hour;           // это такты таймера WDT за час
extern unsigned char start_init_job;            // флаг "НАЧАТЬ РАБОТУ"
extern unsigned char max_day;                   // максимальное количество дней в месяце
union {
    float ftmp;
    unsigned char  ltmp[4];
}tm;

extern unsigned int atmp;//, arch;

extern     struct {
        sens_str a[8];  // Аналоговые входы устройства
        sens_str d[8];  // Цифровые входы устройства
        sens_str v[2];  // Внутренние параметры устройства: температура и питание.
    } sens;

extern unsigned char ROM_NO[];          // буфер для поиска приборов
unsigned int t_ds18b20, minus;
char znaki;
char ROM_DS[7];
char MON_DS[7];
//extern char eco;                        // флаг режима работы СКМ8:
                                        // 1 - экономный режим (для ШРП),
                                        // 0 - Максимальныйный режим (для ГРП)
extern unsigned char charge, ip[];            // флаг "ЗАРЯЖАТЬ ВНУТРЕННИЙ АККУМУЛЯТОР" 0-нет;1-да.
extern calibr pit;                      // множитель значения Напряжения питания
extern unsigned int rm_result;
extern unsigned char iphex[3];
extern unsigned char number;
extern unsigned int c;
//unsigned char RxMbuf.buf[300];

//********************************************************************/
//
// Читаем датчик температуры
//
void get_temperatura(void)
{
char i;

  for (i=0; i<10; i++){
    Loop_DS1820_M(1);                   // Запуск на измерение
                                        //
    __delay_cycles(3000000);            // ждем...
    //
    minus = Loop_DS1820_M(0);           // Чтение результата
    if(minus == 0){
        t_ds18b20  = ROM_NO[1]<<8;
        t_ds18b20 |= ROM_NO[0];
        minus = t_ds18b20 & 0x8000;     // Проверяем на отрицательное значение температуры
        if(minus)                       // Если "-", то берем дополнительный код.
        {   t_ds18b20=(-t_ds18b20);
            MON_DS[0] = '-';
        }
        else    MON_DS[0] = '+';

        // Сохраняем в буфере "сырые" показания температуры
        // в ASCII виде, для вывода на сервер
        ITOA(t_ds18b20, (unsigned char *)&ROM_DS[0]);
        ROM_DS[4]=0;

        //Корректир.показания температуры
        t_ds18b20 = t_ds18b20/16; //для DS18B20
        znaki = ItoDecAShot_znach(t_ds18b20, (char *)&MON_DS[1]);
        MON_DS[znaki+1]=0;
        break;
    }else{
        if(minus==11){
        MON_DS[0]='+';
        MON_DS[1]='0';
        MON_DS[2]='.';
        MON_DS[3]='0';
        MON_DS[4]=0;

        ROM_DS[0]='0';
        ROM_DS[1]='0';
        ROM_DS[2]='0';
        ROM_DS[3]='0';
        ROM_DS[4]=0;
            return;
        }
    }
  }
  if(i>=3){
        MON_DS[0]='+';
        MON_DS[1]='0';
        MON_DS[2]='.';
        MON_DS[3]='0';
        MON_DS[4]=0;

        ROM_DS[0]='0';
        ROM_DS[1]='0';
        ROM_DS[2]='0';
        ROM_DS[3]='0';
        ROM_DS[4]=0;
  }
}
//********************************************************************/
//
// ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ ПАРАМЕТРОВ
//
//
void make_measure_bat(void)
{
signed char tmp;
char  buff[8];
    //
    // Инициализируем Модуль Опорных Напряжений
    // By default, REFMSTR=1 => REFCTL is used to configure the internal reference
    while(REFCTL0 & REFGENBUSY);                // Ждем, пока не освободится генератор Опорного Напряжения
    //REFCTL0 |= REFVSEL_0 + REFON;               // Включаем внутреннее Опорное Напряжение 1.2V
    //REFCTL0 |= REFVSEL_1 + REFON;               // Включаем внутреннее Опорное Напряжение 2.0V
    REFCTL0 |= REFVSEL_2 + REFON;               // Включаем внутреннее Опорное Напряжение 2.5V
//******************************************************************************
    // Инициализируем ADC12_A
    ADC12CTL0  &= ~ADC12ENC;                    // Отключаем ADC12_A
    ADC12CTL0   = ADC12SHT0_9 + ADC12ON;        // Настраиваем время измерений (1000b = 256 циклов ADC12CLK)
    ADC12CTL0  |= ADC12MSC;                     // Устанавливаем режим Множественных измерений и преобразований
    ADC12CTL1   = ADC12SHP;                     // Включаем таймер измерений
    ADC12CTL1  |= ADC12CONSEQ_1;                // Однократно выполнять серию преобразований нескольких каналов
//******************************************************************************
    // Порт 4
    //P4SEL1 |= BIT0;                      // Настраиваем входы P4.0 и P4.1 как ADC
    //P4SEL0 |= BIT0;
    //ADC12MCTL28 |= ADC12VRSEL_1 + ADC12INCH_8;  // Для A8: Опорн.напр.= VREF, ADC вход по каналу A8(P4.0)15
//    ADC12MCTL29 |= ADC12VRSEL_1 + ADC12INCH_9;  // Для A9: Опорн.напр.= VREF, ADC вход по каналу A9(P4.2)16

    ADC12CTL3   = ADC12TCMAP + ADC12BATMAP;     // Подключаем для измерений Термодатчик и Напряжение Питания
    ADC12MCTL30 = ADC12VRSEL_1 + ADC12INCH_30;  // Для температуры: Опорн.напр.= VREF, вход по каналу A30 => temp sense
    ADC12MCTL31 = ADC12VRSEL_1 + ADC12INCH_31;  // Для напряжения питания: Опорн.напр.= VREF,  вход через канал A31 => vcc
    ADC12IER1   = ADC12IE31;                    // Разрешаем прерывание после завершения преобразования на канале А31
    ADC12CTL3  |= ADC12CSTARTADD_30;            // Начинать измерения с канала А30
    ADC12MCTL31|= ADC12EOS ;                    // Канал A31 - последний в последовательности измерений

    while(!(REFCTL0 & REFGENRDY));              // Ждем, когда будет готов генератор Опорного Напряжения
    ADC12CTL0 |= ADC12ENC;                      // ВКЛючаем ADC12_A

    //
    // Запускаем АЦП на измерение и преобразование температуры
    //
    ADC12CTL0 |= ADC12SC;                           // Запускаем АЦП на измерение и преобразование температуры

    //    __bis_SR_register(LPM0_bits + GIE);       // Входим в LPM0 w/ interrupts
    __bis_SR_register(LPM4_bits + GIE);       // Входим в LPM4 w/ interrupts
    __no_operation();
    //__delay_cycles(1000);                           // ждем...
    //
    sens.d[6].dat = ADC12MEM31; // значение напряжения питания MSP430FR
    temp = ADC12MEM30; // значение температуры MSP430FR
    volt = ADC12MEM31; // значение напряжения питания MSP430FR


   	//sens.a[6].dat = ADC12MEM28;        // напряжение на a8  - Uскм
    //tm.ftmp = (sens.a[6].dat * 2.5/4096)/0.1264;
    //sprintf((char *)&pack.a7, "%f3.1", tm.ftmp);
    // вход с коэф.деления 0,1264, поэтому:
//    tm.ftmp = (sens.a[7].dat * 2.5/4096)/0.1264;
//    sprintf((char *)&pack.a8, "%f3.1", tm.ftmp);
    //
    // Готовим к выводу значение напряжения на батарейке
    //
    //vcc = volt * 12/4095;             // для VREF 1,2в
    //vcc = 2 * volt * 200/4095;        // для VREF 2,0в
    vcc = 2 * volt * 250/4096;          // для VREF 2,5в
    //
    if(vcc < 340){      // Если напряжение на внутреннем акк. меньше 3.4 вольт
                        // не отключаем внешний аккумулятор! - происходит заряд внутреннего акк.
        charge = 1;     // заряжать аккумулятор
    }
    else if(vcc > 350){ // Если напряжение на внутреннем акк. больше 3.5 вольт
                        // напряжение на внутреннем акк. в норме - отключаем внешний акк. в режиме ожидания и экономим энергию
            charge = 0; // НЕ заряжать аккумулятор
    }
    ItoDecAShot_2dot(vcc, pack.bat);

    __no_operation();
// */
    //
    // Определяемся с калибровкой температуры
    //
    //pack.temp[0]=pack.temp[1]=pack.temp[2]=pack.temp[3]=pack.temp[4]='0';
    temperatureDegC = (float)(((long)temp - CALADC12_25V_30C) * (85 - 30)) /(CALADC12_25V_85C - CALADC12_25V_30C) + 30.0f; // при Опорном Напряжении 2,5В
    tm.ftmp = temperatureDegC;
    //tm.ftmp = -23.456;
    //
    t7 = tm.ftmp * 100;
    //
    sens.v[0].dat = t7;     // Сохраняем температуру для отправки на сервер!!!
    //
    sprintf((char *)&buff, "%d", t7 );
    //
    // Проверяем отрицательные значения температуры
    //
    //tmp = (long)tm.ftmp & 0x80000000;    // проверяем знак
    tmp = tm.ltmp[3] & 0x80;    // проверяем знак
    if(tmp > 0){
        pack.temp[0]='-';
        if(tm.ftmp <= -10.0){
            pack.temp[0]=buff[0];
            pack.temp[1]=buff[1];
            pack.temp[2]=buff[2];
            pack.temp[3]='.';
            pack.temp[4]=buff[3];
        }
        else if(tm.ftmp <= -1.0){
            pack.temp[0]=buff[0];
            pack.temp[1]=buff[1];
            pack.temp[2]='.';
            pack.temp[3]=buff[2];
            pack.temp[4]=buff[3];
        }
        else if(tm.ftmp <= 0.0){
            pack.temp[0]=buff[0];
            pack.temp[1]='0';
            pack.temp[2]='.';
            pack.temp[3]=buff[1];
            pack.temp[4]=buff[2];
        }

    }
    else{
        if(tm.ftmp >= 10.0){
            pack.temp[0]=buff[0];
            pack.temp[1]=buff[1];
            pack.temp[2]='.';
            pack.temp[3]=buff[2];
            pack.temp[4]=buff[3];
        }
        else if(tm.ftmp >= 1.0){
            pack.temp[0]=buff[0];
            pack.temp[1]='.';
            pack.temp[2]=buff[1];
            pack.temp[3]=buff[2];
            pack.temp[4]=buff[3];
        }
        else if(tm.ftmp >= 0.0){
            pack.temp[0]='0';
            pack.temp[1]='.';
            pack.temp[2]=buff[0];
            pack.temp[3]=buff[1];
            pack.temp[4]=buff[2];
        }
    }
    if(pack.temp[3]==0)pack.temp[3]='0';
    if(pack.temp[4]==0)pack.temp[4]='0';

}
/*
###############################################################################
Описание    : Сравнивает заданную строку символов с содержимым в буфере
Аргументы   : string - Указатель на сравниваемую строку;
            : buf    - Указатель на буфер
            : point_after_compare; - индекс в буфере после сравнения.
            : это индекс в буфере конца фразы, которая сравнивалась.
            : Если сравнение НЕТ, то это конец буфера.
Возврат       : 0- не совпало; если совпало - индекс в буфере+1 начала текста.
Примечание  :
###############################################################################
*/
//
unsigned int cmp_str(const char *string, unsigned char *buf )
{
unsigned int ind_str;           // индекс в искомой строке "string"
unsigned char wait_char;        // символ в искомой строке
unsigned int point_after_compare;
unsigned int result_compare;    // Точка в буфере после сравнения.


    ind_str=0;   // индекс в искомой строке "string"

    for(point_after_compare=0; point_after_compare < 272; point_after_compare++)
    {
        wait_char = buf[point_after_compare];   // Считываем символ,
        if (string[ind_str] == wait_char){
            ind_str++;                          // Смотрим строку символов ОТВЕТА
        }
        else ind_str=0;                         // Если символ не совпал - сбрасываем указатель
        if (string[ind_str]==0){
          result_compare = point_after_compare;
          return result_compare;                // Нашли конец искомой строки - выходим
        }
    }
    result_compare = 0;
    return result_compare;
}
//********************************************************************/
// Ожидает поступления данных в буфер в течение заданного времени.
// На выходе выдает позицию следующего в буфере, за найденной строкой, символа.
// если нет совпадения и время ожидания прошло - на выходе "нуль".
// string - шаблон для поиска (строка);
// buf    - буфер в котором нужно искать и ожидать строку;
// time   - время ожидания.
//
unsigned int wait_compl (const char *string, unsigned char *buf , unsigned int time )
{
unsigned int time_w1;
unsigned int result;
      time_w1 = 0;
      result = 0;

      while(time_w1 < time){
          result = cmp_str(string, buf);

           if(result != 0){
             break;
           }
           //SysCtlDelay(1500000L);  // обязательно подождать!!!
           __delay_cycles(10000);  // ждем... 1с=8,000,000
           time_w1++;
      }
  return result;
}
/*---------   ----------------------
Выводит в строке 4 символа.
Если разряд пустой - выводится нуль:
*/
void  ItoDecAShot_zero(unsigned int Dec, unsigned char* str)
{
  char i;
    i=0;
    if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    else str[i++] = '0';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    else str[i++] = '0';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = '0';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = '0';
                  str[i++]='0'+ Dec%10;
                  str[i]=0;
}
/*---------   ----------------------
Выводит только значащие символы, ведущие нули опускаются.
Подсчитывает количество значащих символов в строке:
*/
//void  ItoDecAShot_zero(unsigned int Dec, unsigned char* str)
char  ItoDecAShot_znach(unsigned int Dec, char* str)
{
  char i;
    i=0;
    if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    //else str[i++] = ' ';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    //else str[i++] = ' ';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    //else str[i++] = ' ';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    //else str[i++] = ' ';
                  str[i++]='0'+ Dec%10;
                  str[i]=0;
  return i;
}
/*---------   ----------------------
Выводит только значащие символы, ведущие нули опускаются.
Подсчитывает количество значащих символов в строке:
* /
void  HextoCtr(unsigned long Hexi, char* str)
{
  char i;
    i=0;
    if(Dec/10000000){str[i++]='0'+(Dec/10000000)%10;}
    else str[i++] = '0';
    if(Dec/1000000){str[i++]='0'+(Dec/1000000)%10;}
    else str[i++] = '0';
    if(Dec/100000){str[i++]='0'+(Dec/100000)%10;}
    else str[i++] = '0';
    if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    else str[i++] = '0';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    else str[i++] = '0';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = '0';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = '0';
                  str[i]='0'+ Dec%10;
}
// */
/*---------   ----------------------
Выводит только значащие символы, ведущие нули опускаются.
Подсчитывает количество значащих символов в строке:
*/
void  ItoDec_8(unsigned long Dec, char* str)
{
  char i;
    i=0;
    if(Dec/10000000){str[i++]='0'+(Dec/10000000)%10;}
    else str[i++] = '0';
    if(Dec/1000000){str[i++]='0'+(Dec/1000000)%10;}
    else str[i++] = '0';
    if(Dec/100000){str[i++]='0'+(Dec/100000)%10;}
    else str[i++] = '0';
    if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    else str[i++] = '0';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    else str[i++] = '0';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = '0';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = '0';
                  str[i]='0'+ Dec%10;
}
/*---------   ----------------------*/
void  ItoDecAShot_1dot(unsigned int Dec, char* str)
{
  char i;
    i=0;
    //if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    //else str[i++] = ' ';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    //else str[i++] = '';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    //else str[i++] = '';
    str[i++] = '.';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = '0';
//                  str[i++]='0'+ Dec%10;
                  //str[i]=0;
}
/*---------   ----------------------*/
void  ItoDecAShot_2dot(unsigned int Dec, char* str)
{
  char i;
    i=0;
    //if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    //else str[i++] = ' ';
    //if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    //else str[i++] = ' ';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = '0';
    str[i++] = '.';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = '0';
                  str[i++]='0'+ Dec%10;
                  //str[i]=0;
}
/*---------   ----------------------*/
void  ItoDecAShot_bn(unsigned int Dec, unsigned char* str)
{
  char i;
    i=0;
    if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    else str[i++] = ' ';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    else str[i++] = ' ';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = ' ';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = ' ';
                  str[i++]='0'+ Dec%10;
                  //str[i]=0;// !!! ЭТО ОСНОВНОЕ ОТЛИЧИЕ !!!
}
/*---------   ----------------------*/
void  ItoDecAShot4(unsigned int Dec, unsigned char* str)
{
  char i;
    i=0;
    //if(Dec/10000){str[i++]='0'+(Dec/10000)%10;}
    //else str[i++] = ' ';
    if(Dec/1000) {str[i++]='0'+(Dec/1000)%10;}
    else str[i++] = ' ';
    if(Dec/100)  {str[i++]='0'+(Dec/100)%10;}
    else str[i++] = ' ';
    if(Dec/10)   {str[i++]='0'+(Dec/10)%10;}
    else str[i++] = ' ';
                  str[i]='0'+ Dec%10;
                  //str[i]=0;// !!! ЭТО ОСНОВНОЕ ОТЛИЧИЕ !!!
}
/*
###############################################################################
Описание   : Преобразует 1 байт шестнадцатиричн. числа в 2 байта символов ASCII
           :  пример: 0x2E --> "2E"
Аргументы  : byData - шестнадцатиричн. число для преобразования
               : *buf   - указатель места записи 2-х байт символов ASCII
Возврат    :
Примечание :
###############################################################################
*/
void HTOA(unsigned char byData, unsigned char *buf)
{
        // Старший символ
        if((byData / 0x10) >= 10)
                *buf++ = ('A'+((byData/0x10)%0x0A));
        else
                *buf++ = ('0'+((byData/0x10)%0x0A));
        // Младший символ
        if((byData % 0x10) >= 10)
                *buf = ('A' + ((byData%0x10)%0x0A));
        else
                *buf = ('0' + ((byData%0x10)%0x0A));
}
/*
###############################################################################
Описание   : Преобразует 1 байт шестнадцатиричн. числа в 2 байта символов ASCII
           :  пример: 0x2E --> "2E"
Аргументы  : byData - шестнадцатиричн. число для преобразования
               : *buf   - указатель места записи 2-х байт символов ASCII
Возврат    :
Примечание :
###############################################################################
*/
void ITOA(unsigned int intData, unsigned char *buff)
{
char buf[4];
        // Разряд 0
        buf[0] = intData / 0x1000;
        if(buf[0] >= 10)
            buf[0] = 'A'+ (buf[0]%0x0A);
        else
            buf[0] = '0'+ buf[0];
        *buff++ = buf[0];
        // Разряд 1
        buf[1] = (intData / 0x100)%0x10;
        if(buf[1] >= 10)
            buf[1] = 'A'+ (buf[1]%0x0A);
        else
            buf[1] = '0'+ buf[1];
        *buff++ = buf[1];
        // Разряд 2
        buf[2] = (intData / 0x10)%0x10;
        if(buf[2] >= 10)
            buf[2] = 'A'+ (buf[2]%0x0A);
        else
            buf[2] = '0'+ buf[2];
        *buff++ = buf[2];
        // Разряд 3
        buf[3] = intData %0x10;
        if(buf[3] >= 10)
            buf[3] = 'A'+ (buf[3]%0x0A);
        else
            buf[3] = '0'+ buf[3];
        *buff = buf[3];
}
/*
###############################################################################
Описание   : Преобразует длинное слово из 4 шестнадцатиричн байтов в
           : 8 чисел символов ASCII
           :  пример: 0x1234567E --> "1234567E"
Аргументы  : LongData - длинное шестнадцатиричн. число для преобразования
               : *buf   - указатель места записи 8 символов ASCII
Возврат    :
Примечание :
###############################################################################
*/
void LtoChars(unsigned long LongData, unsigned char *buff)
{
unsigned char buf[8];

    // Разряд 0
    buf[0] = LongData / 0x10000000;
    if(buf[0] >= 10)
        buf[0] = 'A'+ (buf[0]%0x0A);
    else
        buf[0] = '0'+ buf[0];
    *buff++ = buf[0];
    // Разряд 1
    buf[1] = (LongData / 0x1000000)%0x10;
    if(buf[1] >= 10)
        buf[1] = 'A'+(buf[1]%0x0A);
    else
        buf[1] = '0'+ buf[1];
    *buff++ = buf[1];
    // Разряд 2
    buf[2] = (LongData / 0x100000)%0x10;
    if(buf[2] >= 10)
        buf[2] = 'A'+ (buf[2]%0x0A);
    else
        buf[2] = '0'+ buf[2];
    *buff++ = buf[2];
    // Разряд 3
    buf[3] = (LongData / 0x10000)%0x10;
    if(buf[3] >= 10)
        buf[3] = 'A'+(buf[3]%0x0A);
    else
        buf[3] = '0'+ buf[3];
    *buff++ = buf[3];
    // Разряд 4
    buf[4] = (LongData / 0x1000)%0x10;
    if(buf[4] >= 10)
        buf[4] = 'A'+(buf[4]%0x0A);
    else
        buf[4] = '0'+ buf[4];
    *buff++ = buf[4];
    // Разряд 5
    buf[5] = (LongData / 0x100)%0x10;
    if(buf[5] >= 10)
        buf[5] = 'A'+(buf[5]%0x0A);
    else
        buf[5] = '0'+ buf[5];
    *buff++ = buf[5];
    // Разряд 6
    buf[6] = (LongData / 0x10)%0x10;
    if(buf[6] >= 10)
        buf[6] = 'A'+(buf[6]%0x0A);
    else
        buf[6] = '0'+ buf[6];
    *buff++ = buf[6];
    // Разряд 7
    buf[7] = LongData %0x10;
    if(buf[7] >= 10)
        buf[7] = 'A'+ (buf[7]%0x0A);
    else
        buf[7] = '0'+ buf[7];
    *buff = buf[7];

}

/*
void FloatToChars(float LongData, unsigned char *buff)
{
unsigned char buf[8];
union {
    float ftmp;
    unsigned long  lng;
    unsigned char  ltmp[4];
}tm;

    // Разряд 0
    buf[0] = LongData / 0x10000000;
    if(buf[0] >= 10)
        buf[0] = 'A'+ (buf[0]%0x0A);
    else
        buf[0] = '0'+ buf[0];
    *buff++ = buf[0];
    // Разряд 1
    buf[1] = (LongData / 0x1000000)%0x10;
    if(buf[1] >= 10)
        buf[1] = 'A'+(buf[1]%0x0A);
    else
        buf[1] = '0'+ buf[1];
    *buff++ = buf[1];
    // Разряд 2
    buf[2] = (LongData / 0x100000)%0x10;
    if(buf[2] >= 10)
        buf[2] = 'A'+ (buf[2]%0x0A);
    else
        buf[2] = '0'+ buf[2];
    *buff++ = buf[2];
    // Разряд 3
    buf[3] = (LongData / 0x10000)%0x10;
    if(buf[3] >= 10)
        buf[3] = 'A'+(buf[3]%0x0A);
    else
        buf[3] = '0'+ buf[3];
    *buff++ = buf[3];
    // Разряд 4
    buf[4] = (LongData / 0x1000)%0x10;
    if(buf[4] >= 10)
        buf[4] = 'A'+(buf[4]%0x0A);
    else
        buf[4] = '0'+ buf[4];
    *buff++ = buf[4];
    // Разряд 5
    buf[5] = (LongData / 0x100)%0x10;
    if(buf[5] >= 10)
        buf[5] = 'A'+(buf[5]%0x0A);
    else
        buf[5] = '0'+ buf[5];
    *buff++ = buf[5];
    // Разряд 6
    buf[6] = (LongData / 0x10)%0x10;
    if(buf[6] >= 10)
        buf[6] = 'A'+(buf[6]%0x0A);
    else
        buf[6] = '0'+ buf[6];
    *buff++ = buf[6];
    // Разряд 7
    buf[7] = LongData %0x10;
    if(buf[7] >= 10)
        buf[7] = 'A'+ (buf[7]%0x0A);
    else
        buf[7] = '0'+ buf[7];
    *buff = buf[7];

}
// */

/*
********************************************************************************
*              Функция копирования данных в буфер передачи Tx
*
* Description:
* Arguments  : Tx_Buf : Адрес в Буфере передачи куда должны быть скопированы данные
*              : Stream : данные для копирования
* Returns    : Length - Размер скопированных данных
* Note       :
********************************************************************************
*/
u_int WriteBuf(u_char *buf, const char *stream)
{
u_int len;

    for (len = 0; *stream != '\0'; len++)
        {*buf++ = *stream++;}

    return len;
}
//*****************************************************************************/
u_int WriteBufMultySimb(u_char *buf, const char *data, char dlina)
{
u_int len;

        for (len = 0; len < dlina; len++)
            {*buf++ = *data++;}

        return len;
}
//*****************************************************************************/
u_int WriteBufMultySimb_znach(u_char *buf, const char *data, char dlina)
{
u_int len;

        for (len = 0; len < dlina; len++)
            {*buf++ = *data++;}

        return len;
}
//*****************************************************************************/
u_int WriteBufMultySimbASCII(unsigned char *buf, unsigned char *data, char dlina)
{
u_int len;

        for (len = 0; len < dlina; len++){
            //HTOA(*data, buf);
            //*buf ++;
            // Старший символ
             if((*data / 0x10) >= 10)
                     *buf++ = ('A'+((*data/0x10)%0x0A));
             else
                     *buf++ = ('0'+((*data/0x10)%0x0A));
             // Младший символ
             if((*data % 0x10) >= 10)
                     *buf++ = ('A' + ((*data%0x10)%0x0A));
             else
                     *buf++ = ('0' + ((*data%0x10)%0x0A));
            *data++;
            len++;
        }

        return len;
}


//**********************************************************************************************
// request - адрес начала данных в таблице
// mode   == 0-запрос (на сервер), 1-ответ (от сервера)
//**********************************************************************************************
void httpreq_smart (void)
{
    unsigned int lenreq,lenhead;
    unsigned int dataplace=0;
//unsigned char i;

    lenreq  = WriteBuf( &TxMbuf.buf[0], "POST /stm/wel_aes256-test.php HTTP/1.0\r\n");
    //lenreq  = WriteBuf( &TxMbuf.buf[0], "POST /stm/welcome.php HTTP/1.0\r\n");

         if(server==0) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: skydom.info");       // сервер grmu.skydom.info        СКАЙДОМ
    else if(server==1) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: sky.kgaz.com.ua");// сервер grmu.skydom.info        СКАЙДОМ
    else if(server==2) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: metro.gazbil.ks.ua");// сервер grmu.skydom.info        СКАЙДОМ
    else if(server==3) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: sky.kirgas.com");// сервер grmu.skydom.info        СКАЙДОМ
    else if(server==4) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: grmu.skydom.info");// сервер grmu.skydom.info        СКАЙДОМ
    else if(server==5) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: grmu-skydom.grmu.com.ua");// сервер grmu.skydom.info        СКАЙДОМ
//    else if(server==6) lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: grmu.skydom.info");// сервер grmu.skydom.info        СКАЙДОМ
    //lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Host: metro.gazbil.ks.ua");
    lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\r\n");
    lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Content-Type: application/x-www-form-urlencoded\r\n");
    //lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Content-Type: application/octet-stream\r\n");
    //lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Content-Type: text/plain\r\n");
    // **********************************************************************************************
    //lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Content-Length:       ");
    lenreq += WriteBuf( &TxMbuf.buf[lenreq], "Content-Length:  544");
//        dataplace = lenreq-6;
//    lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\r\n\r\n");
    lenhead = lenreq;// Запоминаем длину заголовка
    // ******************************************************************************************
    // Номер объекта (Адрес устройства)
    lenreq += WriteBuf( &RxMbuf.buf[lenreq], "u=");   //
    if(config.addr[0]>'0'){
        RxMbuf.buf[lenreq++] = config.addr[0];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[1];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[2];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[3];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[4];// Обозначаем конец строки
    }
    else if(config.addr[1]>'0'){
        RxMbuf.buf[lenreq++] = config.addr[1];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[2];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[3];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[4];// Обозначаем конец строки
    }
    else if(config.addr[2]>'0'){
        RxMbuf.buf[lenreq++] = config.addr[2];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[3];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[4];// Обозначаем конец строки
    }
    else if(config.addr[3]>'0') {
        RxMbuf.buf[lenreq++] = config.addr[3];// Обозначаем конец строки
        RxMbuf.buf[lenreq++] = config.addr[4];// Обозначаем конец строки
    }
    else {
        RxMbuf.buf[lenreq++] = config.addr[4];// Обозначаем конец строки
    }
    // ************************************************************************************************
    // Создаем пакет 1:
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"&p1=1100010001");    // Номер пакета
/*
        sens_str a[8];  // Аналоговые входы устройства
        sens_str d[8];  // Цифровые входы устройства
        sens_str v[2];  // Внутренние параметры устройства: температура и питание.
    } sens;
 *
 * */
    __no_operation();
//
//sens_str a[8];  // Аналоговые входы устройства
// 1	"0102" длина += 8; - Рвх
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0102");    // Номер и статус параметра
    LtoChars(sens.a[0].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 2	"0202" длина += 8; - Рф
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0202");    // Номер и статус параметра
    LtoChars(sens.a[1].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 3	"0302" длина += 8; - Рвих
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0302");    // Номер и статус параметра
    LtoChars(sens.a[2].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 4	"0402" длина += 8; - ЭХЗ
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0402");    // Номер и статус параметра
    LtoChars(sens.a[3].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 5	"0502" длина += 8; - Гпри
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0502");    // Номер и статус параметра
    LtoChars(sens.a[4].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 6	"0602" длина += 8; - ПСК
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0602");    // Номер и статус параметра
    LtoChars(sens.a[5].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 7	"0702" длина += 8; - Uскм
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0702");    // Номер и статус параметра
    //LtoChars(rm_result, &RxMbuf.buf[lenreq]);  			// Напряжение Uскм - результат вычисления RMS
    LtoChars(sens.a[6].dat, &RxMbuf.buf[lenreq]);  		// Данные. Число с плавающей точкой
    lenreq += 8;
// 8	"0802" длина += 8; - Uснс
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0802");    // Номер и статус параметра
    //LtoChars(rm_result, &RxMbuf.buf[lenreq]);  			// ДОРАБОТАТЬ!!! Напряжение питания датчиков - результат вычисления RMS
    LtoChars(sens.a[7].dat, &RxMbuf.buf[lenreq]);  		// Данные. Число с плавающей точкой
    lenreq += 8;
/*
    for(i=0; i<8; i++){
        HTOA(sens.a[i].ind, &RxMbuf.buf[lenreq]);       // Код состояния параметра. 1 - неиспользуется, 2 - используется, 3 - обрыв датчика
        lenreq += 2;
        HTOA(sens.a[i].stat, &RxMbuf.buf[lenreq]);      // Номер параметра
        lenreq += 2;
        LtoChars(sens.a[i].dat, &RxMbuf.buf[lenreq]);   // Данные.
        lenreq += 8;
    }
// */
//sens_str d[8];  // Цифровые входы устройства
// 9	"0902" длина += 8; - ПЗК
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0902");    // Номер и статус параметра
    LtoChars(sens.d[0].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 10	"0A02" длина += 8; - Д1
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0A02");    // Номер и статус параметра
    LtoChars(sens.d[1].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 11	"0B02" длина += 8; - Д2
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0B02");    // Номер и статус параметра
    LtoChars(sens.d[2].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 12	"0C02" длина += 8; - Д3
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0C02");    // Номер и статус параметра
    LtoChars(sens.d[3].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 13	"0D02" длина += 8; - -
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0D02");    // Номер и статус параметра
    LtoChars(sens.d[4].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 14	"0E02" длина += 8; - -
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0E02");    // Номер и статус параметра
    LtoChars(sens.d[5].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 15	"0F02" длина += 8; - Uбат Напряжение на внутреннем аккумуляторе
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"0F02");    // Номер и статус параметра
    LtoChars(sens.d[6].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 16	"1002" длина += 8; - CSQ - качество GSM сигнала
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"1002");    // Номер и статус параметра
    LtoChars(sens.d[7].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
/*    for(i=0; i<8; i++){
        HTOA(sens.d[i].ind, &RxMbuf.buf[lenreq]);
        lenreq += 2;
        HTOA(sens.d[i].stat, &RxMbuf.buf[lenreq]);
        lenreq += 2;
        LtoChars(sens.d[i].dat, &RxMbuf.buf[lenreq]);
        lenreq += 8;
    }
// */
//sens_str v[2];  // Внутренние параметры устройства: температура и питание.
// 17	"1102" длина += 8; - Тскм температура MSP430FR
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"1102");    // Номер и статус параметра
    LtoChars(sens.v[0].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
// 18	"1202" длина += 8; - Uпит напряжения питания MSP430FR
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"1202");    // Номер и статус параметра
    LtoChars(sens.v[1].dat, &RxMbuf.buf[lenreq]);  		// Данные.
    lenreq += 8;
/*
    for(i=0; i<2; i++){
        HTOA(sens.v[i].ind, &RxMbuf.buf[lenreq]);
        lenreq += 2;
        HTOA(sens.v[i].stat, &RxMbuf.buf[lenreq]);
        lenreq += 2;
        LtoChars(sens.v[i].dat, &RxMbuf.buf[lenreq]);
        lenreq += 8;
    }
// */
    // ************************************************************************************************
    // Создаем пакет 2:
    lenreq += WriteBuf( &RxMbuf.buf[lenreq],"&p2=110002000201020000");    // Номер пакета
    //lenreq += WriteBuf( &RxMbuf.buf[lenreq], (const char *) &ROM_DS[0]);    // Номер пакета
    HTOA(ROM_NO[1], &RxMbuf.buf[lenreq]);// Обозначаем конец строки
        lenreq += 2;
    HTOA(ROM_NO[0], &RxMbuf.buf[lenreq]);// Обозначаем конец строки
        lenreq += 2;

//  char  csq[5];   // Сила сигнала GSM - "31.99"
    //lenreq += WriteBuf( &RxMbuf.buf[lenreq],"&csq=");    // Номер пакета
    //lenreq += WriteBufMultySimb( &RxMbuf.buf[lenreq], (const char *) &pack.csq, 5);

//  char  bat[4];   // напряжение на элементе питания "3.15"
//    lenreq += WriteBufMultySimb( &RxMbuf.buf[lenreq], (const char *) &pack.bat, 4);
//  char temp[5];   // температура в градусах Цельсия со знаком - "+25.5"
//    lenreq += WriteBufMultySimb( &RxMbuf.buf[lenreq], (const char *) &pack.temp, 5);
//  char time[5];   // Период между передачами на сервер - "43200"
//    lenreq += WriteBufMultySimb( &RxMbuf.buf[lenreq], (const char *) &pack.time, 5);
    //
    // после успешной передачи управляем индексами архивов.
    //

    // 3. Окончание пакета данных
    //*******************************************************************************************
    __no_operation();                        // For debug only
    //*******************************************************************************************
    RxMbuf.buf[lenreq] = 0;     // Обозначаем конец строки
    RxMbuf.ind = lenhead;// !!! тут длина заголовка !!! // Сохраняем длину сообщения
    //
//    ItoDecAShot_bn(lenreq - lenhead,  &TxMbuf.buf[dataplace]);// записываем длину данных !!! ItoDecAShot_bn !!!
    //
}
/*
********************************************************************
* Функция обрабатывает принятые удаленные команды с модема.
* Просматривает буфер приема, находит команду и выполняет ее.
* ответ: - результат выполнения. 0-нет; 1- Успешное выполнение
********************************************************************
*/
char date_m[20];
char wait_m[5];
//
char parse_remote_command(void)
{
char bfer[5],i;
//
//*************************************************************************************************
//
    // принимаем время из интернета
    // time=2017-10-19 08:30:22;
    // структурируем дату в recivedate
    res_ult = cmp_str("time=",&RxMbuf.buf[0]);

    if(res_ult > 0){
        res_ult++;
        for(i=0; i<19; i++){
         date_m[i] = RxMbuf.buf[res_ult+i];
        }
        //sscanf("2017-10-19 23:30:22", "%d-%d-%d %d", &recivedate.tm_year, &recivedate.tm_mon, &recivedate.tm_mday, &recivedate.tm_hour);
        sscanf((const char *)&RxMbuf.buf[res_ult], "%d-%d-%d %d:%d", &recivedate.tm_year, &recivedate.tm_mon, &recivedate.tm_mday, &recivedate.tm_hour, &recivedate.tm_min);
        recivedate.tm_year = recivedate.tm_year-2000;   // обязательная операция!!!
        // определяем максимальные даты в месяце
        //max_day = recivedate.tm_mon;

          if(recivedate.tm_mon == 1)  max_day = 31; //'Січень'

     else if(recivedate.tm_mon == 2)  max_day = 28; //'Лютий'   - здесь обработка високосніх годов

     else if(recivedate.tm_mon == 3)  max_day = 31; //'Березень'
     else if(recivedate.tm_mon == 4)  max_day = 30; //'Квітень';
     else if(recivedate.tm_mon == 5)  max_day = 31; //'Травень';
     else if(recivedate.tm_mon == 6)  max_day = 30; //'Червень';
     else if(recivedate.tm_mon == 7)  max_day = 31; //'Липень';
     else if(recivedate.tm_mon == 8)  max_day = 31; //'Серпень';
     else if(recivedate.tm_mon == 9)  max_day = 30; //'Вересень';
     else if(recivedate.tm_mon == 10) max_day = 31; //'Жовтень';
     else if(recivedate.tm_mon == 11) max_day = 30; //'Листопад';
     else if(recivedate.tm_mon == 12) max_day = 31; //'Грудень';
     else max_day = 31;
    }
//
//*************************************************************************************************
//
    // Параметр частота передачи скайдера - время интервалов между передачами на сервер
    //----01234567890123456789
    // ft=5
    res_ult = cmp_str("ft=",&RxMbuf.buf[0]);

    if(res_ult > 0){
      res_ult++;
      for(ixx=0; ixx<5; ixx++){
        uxx = res_ult + ixx;
        if(RxMbuf.buf[uxx] == ';') break;
        bfer[ixx] = RxMbuf.buf[uxx]; // накапливаем в буфере число

      }
/*
 *  pack.time[0]='0';pack.time[1]='0';pack.time[2]='0';pack.time[3]='1';pack.time[4]='5';
 *  pack_time  = atoi((const char *)pack.time);
 *
*/
      if(ixx==1){
          pack.time[0] = '0';
          pack.time[1] = '0';
          pack.time[2] = '0';
          pack.time[3] = '0';
          pack.time[4] = bfer[0];
      }
      else if(ixx==2){
          pack.time[0] = '0';
          pack.time[1] = '0';
          pack.time[2] = '0';
          pack.time[3] = bfer[0];
          pack.time[4] = bfer[1];
      }
      else if(ixx==3){
          pack.time[0] = '0';
          pack.time[1] = '0';
          pack.time[2] = bfer[0];
          pack.time[3] = bfer[1];
          pack.time[4] = bfer[2];
      }
      else if(ixx==4){
          pack.time[0] = '0';
          pack.time[1] = bfer[0];
          pack.time[2] = bfer[1];
          pack.time[3] = bfer[2];
          pack.time[4] = bfer[3];
      }
      else if(ixx==5){
          pack.time[0] = bfer[0];
          pack.time[1] = bfer[1];
          pack.time[2] = bfer[2];
          pack.time[3] = bfer[3];
          pack.time[4] = bfer[4];
      }
      else{
          pack_time  = 422; // код ошибки 422 - не определена длина паузы utilites_skz.c строка 1209
          return 0;
      }
      pack_time  = (pack.time[0]-'0')*10000;  //atoi((const char *)pack.time);
      pack_time += (pack.time[1]-'0')*1000;  //atoi((const char *)pack.time);
      pack_time += (pack.time[2]-'0')*100;  //atoi((const char *)pack.time);
      pack_time += (pack.time[3]-'0')*10;  //atoi((const char *)pack.time);
      pack_time +=  pack.time[4]-'0';     //atoi((const char *)pack.time);
      pack_time *= 7;   // обычно,  60*7 = 420 - норма
  }
    else{
          pack_time  = 423; // код ошибки 423 - нет команды о задержке utilites_skz.c строка 1116
    }

    return 0;
}
//
uint8_t Data[16];
uint8_t CipherKey[32] =
    {0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f
    };
uint8_t DataAESencrypted[16];       // Зашифровані дані
uint8_t DataAESdecrypted[16];       // Розшифровані дані
U8 encrdata[272];
//U8 decrdata[272];
//
void aes256(U8 *rawdata){
uint8_t i = 0;
uint8_t r;
int u=0;
//int q=0;
    // зупиняємо watchdog
//    WDT_A_hold(WDT_A_BASE);

    // Розділяємо телеметрічні дані по блоках.
    for (r = 0; r < 17; r++){           // Всього блоків по 16 байтів буде 17.
        for (i=0; i < 16; i++){         // Виділяємо блоки по 16 байтів в масив Data[]
            Data[i] = rawdata[i+u];
            //DataAESencrypted[i]=0;
        }
        // Завантажуємо ключ шифрування в модуль
        AES256_setCipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);

        // Виконуємо шифрування даних за допомогою попередньо завантаженого ключа шифрування
        AES256_encryptData(AES256_BASE, Data, DataAESencrypted);

        for (i=0; i < 16; i++){
            encrdata[i+u] = DataAESencrypted[i];
        }
/*
        // Завантажуємо ключ шифрування в модуль
        AES256_setDecipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);

        // Розшифровуємо дані
        AES256_decryptData(AES256_BASE, DataAESencrypted, DataAESdecrypted);


        for (i=0; i < 16; i++){
            decrdata[i+u] = DataAESdecrypted[i];
            //HTOA(DataAESdecrypted[i], &decrdata[q]); q += 2;
        }
// */
        u += 16;

        // Array DataunAES should now contain the same data as array Data
        // Масив DataunAES тепер повинні містити ті самі дані, що й в масиву Data
    }
    __no_operation();


//    while(1){};
}

