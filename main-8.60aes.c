#include "system.h"
#include "drivers/rdx0154.h"            // Утилиты обслуживания LCD и клавиатуры
#include <stdint.h>
#include <stdlib.h>
#include <driverlib.h>
#include <stdio.h>
#include <time.h>
#include "drivers/microlan.h"
#include "app_header.h"

extern __interrupt void Port_1(void);
extern __interrupt void Port_2(void);
extern __interrupt void Port_3(void);
extern __interrupt void WDT_ISR(void);
extern __interrupt void USCI_A0_ISR(void);
extern __interrupt void USCI_B0_ISR(void);
extern __interrupt void ADC12ISR(void);
extern void _c_int00_noargs_mpu(void);

#pragma DATA_SECTION(g_app_header, ".app_header")
#pragma RETAIN(g_app_header)

const AppHeader g_app_header =
{
    APP_HEADER_MAGIC,
    APP_HEADER_FORMAT_VERSION,

    _c_int00_noargs_mpu,

    Port_1,
    Port_2,
    Port_3,

    WDT_ISR,

    USCI_A0_ISR,
    USCI_B0_ISR,

    ADC12ISR
};

#define SLAVE_ADDRESS 0x38      // дисплей RDX0154
#define MCLK_FREQUENCY          1000000
#define SMCLK_FREQUENCY         1000000
#define HOUR_TICK               424     //3    // тестирование
#define SUT_ARCH                366
//#define SUT_ARCH              3           // тестирование
#define CHAS_ARCH               1444        // 744
//#define CHAS_ARCH             24          // тестирование
//#define DATCHIKI                4000000     // циклы ожидания включения датчиков
#define DATCHIKI                7000000     // циклы ожидания включения датчиков
//
//const char vers[] ="СКМ-8.10 ШГРП"; // start version
//const char vers[] ="СКМ-8.11 ШГРП"; // 08.10.2018 - обрабатывается отсутствие СИМ-карты. Нет сим - отключить все и ждать 1 час.
//const char vers[] ="СКМ-8.12 ШГРП"; // 08.11.2018 - обрабатывается ошибка входа в GPRS - отключить все и ждать 1 час.
//const char vers[] ="СКМ-8.14 ШГРП"; // 10.12.2018 - измер.напряж.на аккум, меню настройки CSQ, доработка платы СКМ
//const char vers[] ="СКМ-8.15 ШГРП"; // 12.12.2018 - добавлена кнопка активации монитора на вход Р3.0
//const char vers[] ="СКМ-8.2.1 ШГРП"; // 16.03.2019 - обновление отображения на мониторе, реакции на события, контроль и заряд вн.аккумулятора
//const char vers[] ="СКМ-8.2.2 ШГРП"; // 27.11.2019 - отображение на мониторе, процесса отправки сообщения
//const char vers[] ="СКМ-8.2.3 ШГРП"; // 17.04.2020 - ускорено включение дисплея
//const char vers[] ="СКМ-8.2.4 ШГРП"; // 24.07.2021 - добавлена обработка СИМ-карт М2М на 3-х операторов сети GSM
//const char vers[] ="СКМ-8.4.0 ШГРП"; // 08.11.2021 - Входные сигналы обрабатывает чип ADS7128.
//                                                   Из пакета передачи убраны не используемые параметры: 13 "0D02" и 14 "0E02".
//const char vers[] ="СКМ-8.4.1 ШГРП"; // 08.11.2021 - Входные сигналы обрабатывает чип ADS7128. Подальша модифікація
//const char vers[] ="СКМ-8.4.2 ШГРП"; // 14.11.2023 -  Подальша модифікація
//const char vers[] ="СКМ-8.4.3 ШГРП"; // 14.01.2024 -  Оптимизована робота ads7128
//const char vers[] ="СКМ-8.4.4 ШГРП"; // 10.10.2024 -  Виправлені помилки по дискретних входах (P3.0)
//const char vers[]   ="СКМ-8.51aes.ШГРП"; // 21.01.2026 -  кодування в AES-256 пакет даних для передачі на сервер
//const char vers[]   ="СКМ-8.52aes.ШГРП"; // 29.03.2026 - Підключаєм таймер WDT на час вимірювання RMS
//const char vers[]   ="\xd1\xca\xcc-8.60aes.\xd8\xc3\xd0\xcf"; // 19.05.2026 - Вхід в меню - на старті утримувати 2 кнопку.
const char vers[]   ="СКМ-8.60aes.ШГРП"; // 19.05.2026 - Вхід в меню - на старті утримувати 2 кнопку.
//
volatile uint16_t time_transmit;         // Глобальный счетчик - передача данных на сервер
volatile uint16_t start_transmit;
volatile uint16_t time_ind;              // таймер индикации
volatile uint16_t start_indi;
uint8_t RXData, TXData = 0;
unsigned char display;          // наличие дисплея:     0-дисплей есть, 1-нет.
volatile unsigned char indi;                      // разрешить индикацию
struct rxM_buf RxMbuf;          // буфер модема
struct buf_str TxMbuf;          //  -"- длина 410
unsigned int time_rxM;          // и окончания приема данных с модема
extern int repl;                // результат сравнения командой cmp_str();
extern unsigned char csq[8];    // данніе о качестве сигнала GSM
extern unsigned int t_ds18b20, minus;
extern char znaki;

unsigned int temp;
extern unsigned char ROM_NO[];       // буфер для поиска приборов
unsigned int atmp;//, arch;
unsigned long volt;//, uuso;
unsigned char result;
//*
struct remote_data remote;
time_t request_unix;
const unsigned long date_2000 = 946684800;  // время: Sat, 01 Jan 2000 00:00:00 GMT
unsigned long req_date;//,req_dat;                      // запрашиваемая дата в формате юникс, в минутах.
char *pnt;

uint16_t time_hour;
uint16_t time_day;
const uint16_t patern_time_day  =24;        // это количество часов в сутках (24 часа)

unsigned char restart;
unsigned int  pack_time;// = HOUR_TICK;        // это время между передачами
unsigned char max_day;              // максимальное количество дней в месяце
volatile unsigned char start_work;           // флаг "Начата работа" 1- начать чтение данніх и передачу; 0 - работа в штатном режиме.
unsigned char alarm_cur;
unsigned char alarm_pred;
unsigned char alarm_changed;
unsigned char skip_alarm_after_transmit;
//
// создаем секцию во FRAM независимую от сброса системы:
//
// Данные для передачи:
//
/* sens_str a[8];  // Аналоговые входы устройства
 *        sens.a[0].dat = ADC12MEM22;        // напряжение на a13 - Рвх
 *        sens.a[1].dat = ADC12MEM23;        // напряжение на a14 - Рф
 *        sens.a[2].dat = ADC12MEM24;        // напряжение на a15 - Рвих
 *        sens.a[3].dat = ADC12MEM25;        // напряжение на a3  - ЭХЗ
 *        sens.a[4].dat = ADC12MEM26;        // напряжение на a4  - Гпри
 *        sens.a[5].dat = ADC12MEM27;        // напряжение на a5  - ПСК
 *        sens.a[6].dat = ADC12MEM28;        // напряжение на a8  - Uскм
 *        sens.a[7].dat = ADC12MEM29;        // напряжение на a9  - Uснс
 *
 *    sens_str d[8];  // Цифровые входы устройства
 *        sens.d[0].dat = P1IN & 0x01 - сработки ПЗК
 *        sens.d[1].dat = P1IN & 0x02 - сработки Д1
 *        sens.d[2].dat = P1IN & 0x04 - сработки Д2
 *        sens.d[3].dat = P3IN & 0x01 - сработки Д3
 *        sens.d[4].dat =
 *        sens.d[5].dat =
 *        sens.d[6].dat = ADC12MEM31; // значение напряжения на аккумуляторе - внешнее питание отключено!
 *        sens.d[7].dat = atol(pack.csq) - CSQ - качество GSM сигнала
 *
 *    sens_str v[2];  // Внутренние параметры устройства: температура и питание.
 *        sens.v[0].dat = ADC12MEM30; // значение температуры MSP430FR
 *        sens.v[1].dat = ADC12MEM31; // значение напряжения питания MSP430FR
 *
 */
#pragma PERSISTENT(sens)
struct {
    sens_str a[8];  // Аналоговые входы устройства
    sens_str d[8];  // Цифровые входы устройства
    sens_str v[2];  // Внутренние параметры устройства: температура и питание.
    //sens_str c[2];  // для тестирования
}sens = {  0x01,0x02,0x00000000, 0x02,0x02,0x00000000, 0x03,0x02,0x00000000, 0x04,0x02,0x00000000
    ,0x05,0x02,0x00000000, 0x06,0x02,0x00000000, 0x07,0x02,0x00000000, 0x08,0x02,0x00000000
    ,0x09,0x02,0x00000000, 0x0A,0x02,0x00000000, 0x0B,0x02,0x00000000, 0x0C,0x02,0x00000000
    ,0x0D,0x01,0x00000000, 0x0E,0x01,0x00000000, 0x0F,0x02,0x00000000, 0x10,0x02,0x00000000
    ,0x11,0x02,0x00000000, 0x12,0x02,0x00000000}; //, 0x13,0x02,0x00000000, 0x14,0x02,0x00000000
    //
    #pragma PERSISTENT(patern_time_hour)
    const unsigned int patern_time_hour =HOUR_TICK;         // это такты таймера WDT за час
    //
    #pragma PERSISTENT(start_init_job)
    unsigned char start_init_job=0;             // флаг "НАЧАТЬ РАБОТУ"
    //
    #pragma PERSISTENT(recivedate)
    struct tm recivedate={0};                   // структура для хранения даты и времени, принятых из интернета
    //
    #pragma PERSISTENT(charge)
    unsigned char charge=0;                       // флаг "ЗАРЯЖАТЬ ВНУТРЕННИЙ АККУМУЛЯТОР" 0-нет;1-да.
    //
    //#pragma PERSISTENT(try)
    extern char try;                         // флаг режима работы СКМ8:

    /*
     * typedef struct skm8_packet{// Пакет данных для передачи на сервер.
     *            char addr[5];   // Адрес устройства в системе - "9999"
     *            char type[4];   // тип устройства - идентификатор "0008"
     *            char  bat[4];   // напряжение на элементе питания "3.15"
     *            char temp[5];   // температура в градусах Цельсия со знаком - "+25.5"
     *            char  csq[5];   // Сила сигнала GSM - "31.99"
     *            char time[6];   // Период между передачами на сервер - "43200". [6]- здесь 0 - конец строки
     *            char     dio;   // цифровые входы Р1.0|Р1.1|Р1.2|Р3.0
     *            char   a1[4];   // напряжение на a13 - Рвх
     *            char   a2[4];   // напряжение на a14 - Рф
     *            char   a3[4];   // напряжение на a15 - Рвих
     *            char   a4[4];   // напряжение на a3  - ЭХЗ
     *            char   a5[4];   // напряжение на a4  - Гпри
     *            char   a6[4];   // напряжение на a5  - ПСК
     *            char   a7[4];   // напряжение на a8  - Uскм
     *            char   a8[4];   // напряжение на a9  - Uснс
     *      }skm8_packet;
     */
    #pragma PERSISTENT(config)
    struct skm8_packet config={               // Конфигурация устройства СКАЙБИ-GSM
        //'0','0','8','7','2'                         //    char addr[5];   // Адрес устройства в системе - "19999"
        '1','1','3','0','0'                     //    char addr[5];   // Адрес устройства в системе - "19999"
        //'0','0','3','0','2'                     //    char addr[5];   // Адрес устройства в системе - "19999"
        ,'0','0','0','1'                        //    char type[4];   // тип устройства - идентификатор "0001"
        ,'0','0','0','0'                        //    char  bat[4];   // напряжение на элементе питания "3.15"
        ,'0','0','0','0','0'                    //    char temp[5];   // температура в градусах Цельсия со знаком - "+25.5"
        ,'0','0','0','0','0'                    //    char  csq[5];   // Сила сигнала GSM - "31.99"
        ,'0','0','0','0','0'                    //    char koef[5];   // Коэффициент счетчика эл.энергии "6400"
        ,'0','0','4','2','4',0                  //    char time[6];   // Период между передачами на сервер - "43200"
    };
    // !!! РЕЖИМ РАБОТЫ СКМ !!!
    #pragma PERSISTENT(eco)
    char eco=1;                         // флаг режима работы СКМ8:
    // 1 - экономный режим (для ШРП),
    // 0 - Максимальныйный режим (для ГРП)
    //
    struct skm8_packet pack={                 // Пакет данных для передачи на сервер.
        '0','0','0','0','0'                    //    char addr[5];   // Адрес устройства в системе - "19999"
        ,'0','0','0','1'                        //    char type[4];   // тип устройства - идентификатор "0008"
        ,'0','0','0','0'                        //    char  bat[4];   // напряжение на элементе питания "3.15"
        ,'0','0','0','0','0'                    //    char temp[5];   // температура в градусах Цельсия со знаком - "+25.5"
        ,'0','0','0','0','0'                    //    char  csq[5];   // Сила сигнала GSM - "31.99"
        ,'0','0','0','0','0'                    //    char koef[5];   // Коэффициент счетчика эл.энергии "6400"
        ,'0','0','0','0','0',0                  //    char time[6];   // Период между передачами на сервер - "43200"
        ,'0'                                    //    char  dio;   // цифровые входы Р1.0|Р1.1|Р1.2|Р3.0
        ,'0','0','0','0'                        //    char   a1[4];   // напряжение на a13 - Рвх
        ,'0','0','0','0'                        //    char   a2[4];   // напряжение на a14 - Рф
        ,'0','0','0','0'                        //    char   a3[4];   // напряжение на a15 - Рвих
        ,'0','0','0','0'                        //    char   a4[4];   // напряжение на a3  - ЭХЗ
        ,'0','0','0','0'                        //    char   a5[4];   // напряжение на a4  - Гпри
        ,'0','0','0','0'                        //    char   a6[4];   // напряжение на a5  - ПСК
        ,'0','0','0','0'                        //    char   a7[4];   // напряжение на a8  - Uскм
        ,'0','0','0','0'                        //    char   a8[4];   // напряжение на a9  - Uснс
        ,'0','0','0','0'                        //    char   uuso[4]; // напряжение rms  - Uскм
    };
    //
    //
    #pragma PERSISTENT(server)                  // флаг Выбор сервера
    //
    //signed char server=0;                     // сервер skydom.info        СКАЙДОМ
    //signed char server=1;                     // сервер sky.kgaz.com.ua    КРЕМЕНЧУКГАЗ
    //signed char server=2;                     // сервер metro.gazbil.ks.ua ХЕРСОНГАЗ
    //signed char server=3;                     // сервер  Кіровоградгаз
    signed char server=4;                       // 78.27.235.144    // сервер skydom
    //signed char server=5;                     // сервер grmu-skydom.grmu.com.ua - ГАЗМЕРЕЖІ УКРАЇНИ
    //signed char server=6;                     // сервер 31.42.179.214 - Шепетівкагаз
    //
    #pragma PERSISTENT(pit)
    calibr pit  = {8.26, 0};                    // множитель значения Напряжения питания
    //
    extern char date_m[];
    extern char MON_DS[7];
    unsigned char error;
    //unsigned char datchiki;

    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    float u_xx,uload;
    float i_xx,iload;
    unsigned int rm_result;
    unsigned int rm_xx;

    extern unsigned char error, rms_cfg,channel, alert,system_status,general_cfg,rms_lsb,rms_msb;
    extern unsigned int cnt, rms_resume;
    unsigned int  rms_result;


    /* Используется для отслеживания состояния конечного автомата программного обеспечения*/
    extern volatile I2C_Mode MasterMode;

    /* Регистр адреса/команды для использования*/
    extern uint8_t TransmitRegAddr;

    /* ReceiveBuffer: Буфер, используемый для приема данных в ISR
     * RXByteCtr: количество байтов, оставшихся для получения
     * ReceiveIndex: индекс следующего байта, который должен быть получен в ReceiveBuffer
     * TransmitBuffer: буфер, используемый для передачи данных в ISR
     * TXByteCtr: количество байтов, оставшихся для передачи
     * TransmitIndex: индекс следующего байта, который будет передан в TransmitBuffer.
     **/
    extern uint8_t ReceiveBuffer[];
    extern volatile uint8_t RXByteCtr;
    extern volatile uint8_t ReceiveIndex;
    extern uint8_t TransmitBuffer[];
    extern volatile uint8_t TXByteCtr;
    extern volatile uint8_t TransmitIndex;
    extern union {
        float ftmp;
        unsigned long  lng;
        unsigned char  ltmp[4];
    }tm;
    extern uint8_t cycle[]; // набор команд для RMS-опроса восьми входов ADS7128
    unsigned int rms[8];
    union {
        float ftmp;
        unsigned long  lng;
        unsigned char  ltmp[4];
    }izm;

    unsigned int data_tmp;
    // !!!
    // !!! Коэф.делителя нужен для калибровки значений питания СКМ и питания датчиков,
    // !!! чтобы они правильно отображались на дисплее ИНДИВИДУАЛЬНО ДЛЯ КАЖДОГО КОНТРОЛЛЕРА !!!
    #define DEL6847   1320  // коєф.передачи входного делителя напряжения 6.8к/6.8+47к = 0.1264 !!!
    //#define DEL6847   910; // коєф.передачи входного делителя напряжения 6.8к/6.8+68к = 0.09090 !!!
    // !!!
    unsigned char number;
    unsigned int i;
    unsigned int c;
    unsigned char ipdec[12];    // IP адреса для тестування
    //unsigned char ipdec[12] ="185.6.15"; // IP адреса для тестування
    //    unsigned char  ipdec[12] ="255.255.255"; // IP адреса для тестування
    unsigned char iphex[3];
    //unsigned char txbuffer[100]="852&p1=11000100010102000004E10202000004D5&p2=110002000201020000019C";
    //        "0402000001A40502000001B106020000018E0702000005FF08020000020C0902000000000A02000000010B02000000000C02000000000D02000000000E02000000000F0200000B1D1002000000101102000007F0120200000000&p2=110002000201020000019C";
    //unsigned char outbuffer[300];

    void get_RMS(void)
    {

        initGPIO();                         // Подготавливаем вывода для I2C
        initI2C();                          // Настраиваем на работу с АЦП RMS ADS7128
        //
        // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
        //
        P3OUT |= BIT6;                        // HIBER  - включение питания от сети !!! высокий !!!
        P3OUT |= BIT7;                        // DATCHIKI  - ВКЛючение датчиков !!! высокий !!!
        PJOUT |= BIT4;                        // OUT_1  - включение +5В 1 !!! высокий !!!
        __delay_cycles(1000000);        // ждем...

        /*
         *    ,'0','0','0','0'                        //    char   a1[4];   // напряжение на a13 - Рвх
         *    ,'0','0','0','0'                        //    char   a2[4];   // напряжение на a14 - Рф
         *    ,'0','0','0','0'                        //    char   a3[4];   // напряжение на a15 - Рвих
         *    ,'0','0','0','0'                        //    char   a4[4];   // напряжение на a3  - ЭХЗ
         *    ,'0','0','0','0'                        //    char   a5[4];   // напряжение на a4  - Гпри
         *    ,'0','0','0','0'                        //    char   a6[4];   // напряжение на a5  - ПСК
         *    ,'0','0','0','0'                        //    char   a7[4];   // напряжение на a8  - Uскм
         *    ,'0','0','0','0'                        //    char   a8[4];   // напряжение на a9  - Uснс
         *    ,'0','0','0','0'                        //    char   uuso[4]; // напряжение rms  - Uскм
         */
         /* Захист тривалого RMS-вимірювання:
          * WDT clock = VLO незалежно від ACLK.
          */
        WDTCTL = WDTPW | WDTCNTCL | WDTSSEL__VLO | WDTIS__512K; // ACLK watchdog ~52s

        sens.a[0].dat  = rms[0] = measure_RMS(&cycle[0]);   // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[1].dat  = rms[1] = measure_RMS(&cycle[6]);   // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[2].dat  = rms[2] = measure_RMS(&cycle[12]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[3].dat  = rms[3] = measure_RMS(&cycle[18]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[4].dat  = rms[4] = measure_RMS(&cycle[24]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[5].dat  = rms[5] = measure_RMS(&cycle[30]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[6].dat  = rms[6] = measure_RMS(&cycle[36]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        sens.a[7].dat  = rms[7] = measure_RMS(&cycle[42]);  // Выполняем измерение RMS(средне квадратичное значение напряжения)
        /* Повертаємо інтервальний WDT:
         * ACLK = LFMODCLK.
         */
        WDTCTL = WDTPW | WDTCNTCL | WDTTMSEL | WDTSSEL__ACLK | WDTIS_4; // ACLK, restore timer mode

        __no_operation();

        if(display == 1){                   // Если есть дисплей, то:
            // то нужно проинициализировать дисплей
            i2c_open();                       // Открываем порт
        }
        __no_operation();
    }
    /*
     * ###############################################################################
     * Описание   : Преобразует строку символов ІР адреса в масив из 3-х HEX чисел
     *           :    Пример: "73.167.150" --> 0x49,0xF7,0x96
     * Аргументы  : str  - Указатель строки для преобразования
     *           : out  - Указатель преобразованной строки
     *           : base - Значение базы исчисления.Должно быть между 2 ~ 16
     * Возврат    : нет
     * Примечание : В конце строки обязательно должен быть ноль '\0'
     * ###############################################################################
     */
    void IPdecToHex(unsigned char* str,unsigned char* out)
    {
        u_char i;
        u_char j=0;
        unsigned char numer = 0;
        //
        for(i=0;i<11;){
            if(*str =='.')break;
            numer = numer * 10 + C2D(*str++);
            i++;
        }
        out[j++]=numer; // Зберігаємо старше число
        numer = 0;
        *str++;
        for(i=i;i<11;){
            if(*str =='.')break;
            numer = numer * 10 + C2D(*str++);
            i++;
        }
        out[j++]=numer; // Зберігаємо середнє число
        *str++; numer = 0;
        for(i=i;i<11;){
            if(*str == 0)break;
            numer = numer * 10 + C2D(*str++);
            i++;
        }
        out[j]=numer; // Зберігаємо молодше число
        //    return i;
    }
    /******************************************************************************
    u_char AtoH(u_char* str, char base)
    {
        u_char num = 0;
        //while (*str !=0)
        num = num * base + C2D(*str++);
        return num;
    } */
    /*C2D
     * ###############################################################################
     * Описание    : Преобразует Символы в двоичные числа - Тетрады Hex(0-F)
     * Аргументы   : c - символ ('0'~'F') для преобразования в Hex
     * Возврат     : Возвращается двоичное число (0x00~0x0F)
     * Примечание  :
     * ###############################################################################
     * C2D
     */
    unsigned char C2D(unsigned char d)
    {
        unsigned char c = d;
        unsigned char ret;

        if((c >= 0x30) && (c <= 0x39)){
            ret = c - 0x30;
            return ret;
        }
        else if((c >= 0x41) && (c <= 0x46)){
            ret = c - 0x41 +10;
            return ret;
        }
        return c;
    }

    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************
    //****************************************************************************************************

    //
    //
    void main(void)
    {
        unsigned char ttt[6];
        unsigned char digit[4]={'0','0','0','0'};
        //    unsigned char ipdec[] ="69.245.177";

        /*
         * WDTCTL   - Регистр управления таймером WDT
         * --------------------------------------------
         * WDTPW    - пароль WDT. всегда равен 0х069.
         * WDTTMSEL - Режим работы таймера: 0 - режим WDT; 1 - таймер интервалов.
         * WDTSSEL_2- Источник тактирования.
         *          00b = SMCLK
         *          01b = ACLK
         *          10b = VLOCLK
         *          11b = X_CLK, такой-же как VLOCLK, если другое не указано в даташите
         * WDTIS_5  - интервал WDT
         *          000b = Watchdog clock source / 2^31 (18:12:16 at 32.768 kHz)
         *          001b = Watchdog clock source / 2^27 (01:08:16 at 32.768 kHz)
         *          010b = Watchdog clock source / 2^23 (00:04:16 at 32.768 kHz)
         *          011b = Watchdog clock source / 2^19 (00:00:16 at 32.768 kHz)
         *        100b = Watchdog clock source / 2^15 (      1s at 32.768 kHz)
         *          101b = Watchdog clock source / 2^13 (   250ms at 32.768 kHz)
         *          110b = Watchdog clock source / 2^9  (15.625ms at 32.768 kHz)
         *          111b = Watchdog clock source / 2^6  (  1.95ms at 32.768 kHz)
         */
        //WDTCTL = WDTPW | WDTTMSEL | WDTSSEL_2 | WDTIS_4;    // VLOCLK, 16s interrupts
        WDTCTL = WDTPW | WDTTMSEL | WDTSSEL_1 | WDTIS_4;  // ACLK(VLOCLK), ~3.3s/tick
        //  WDTCTL = WDTPW | WDTSSEL__SMCLK | WDTTMSEL | WDTCNTCL | WDTIS__512K;  // ~5 сек.
        SFRIE1 |= WDTIE;                                    // Enable WDT interrupt
        // Настраиваем системное тактирование
        CSCTL0_H = CSKEY >> 8;                      // Снимаем блокировку регистров CS
        CSCTL1 = DCOFSEL_0;                         // Устанавливаем DCO на 1 MHz
        //CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__VLOCLK;
        CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__LFMODOSC;
        CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;       // Устанавливаем все делители в 1
        CSCTL4 = LFXTOFF | HFXTOFF;
        CSCTL0_H = 0;                               // Блокируем регистры CS
        //*************************************************************************
        // определения переменных и констант
        //
        // СКМ8 имеет 12 входов:
        // * ПЗК  -   P1.0; дискретный вход
        // * Д1   -   P1.1; дискретный вход
        // * Д2   -   P1.2; дискретный вход
        // * Д3   -   P3.0; дискретный вход
        //
        // * Рвх  -   P3.1; аналоговый вход A13. опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * Рф   -   P3.2; аналоговый вход A14. опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * Рвих -   P3.3; аналоговый вход A15. опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * Uэхз -   P1.3; аналоговый вход A3.  опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * Гпри -   P1.4; аналоговый вход A4.  опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * ПСК  -   P1.5; аналоговый вход A5.  опорное напряжение 2.5В; входной сигнал 0,4 - 2,0В.
        // * Uскм -   P4.0; аналоговый вход A8.  опорное напряжение 2.5В; входной сигнал 0 - 19,78В. Делитель (47+6.8)/6.8 = 7,912. макс.значение- 2,5*7,912=19,78В
        // * Uснс -   P4.1; аналоговый вход A9.  опорное напряжение 2.5В; входной сигнал 0 - 19,78В.
        // *
        // * 2 выхода:
        // * OUT_1-   PJ.4
        // * OUT_2-   PJ.5
        // */
        //*********************************************************************************
        // По умолчанию, все порты переключаем на вывод и устанавливаем "высокий"
        //*********************************************************************************
        //
        // Конфигурируем порт3
        //
        // P3.0 - Д3        -  8 Дискретный вход 3
        // P3.1 - Рвх       -  9 аналоговый вход А13
        // P3.2 - Рф        - 10 аналоговый вход А14
        // P3.3 - Рвих      - 11 аналоговый вход А15
        // P3.4 - DTR_M     - 26 выход DTR для модема
        // P3.5 - PWRKEY    - 27 выход включения модема
        // P3.6 - HIBER     - 28 выход включения питания от сети
        // P3.7 - DATCHIKI  - 29 выход включения питания датчиков
        //
        P3OUT = 0x00;
        P3DIR = 0xF0;
        P3OUT |= BIT4;                              // 26 Включаем DTR для модема
        P3OUT |= BIT5;                              // 27 PWRKEY - включение модема !!! высокий, а потом, низкий более 1сек.,затем опять высокий  !!!
        P3OUT |= BIT6;                              // 28 HIBER  - включение питания от сети !!! высокий !!!

        P3IES |= BIT0;                              // Уст. фронт прерывания - с "высокого" на "низкий" P3.0 Hi/Lo edge
        P3IE  |= BIT0;                              // РАЗРЕШАЕМ прерывание P3.0
        //
        // Конфигурируем порт1, где:
        //
        // * ПЗК  -   P1.0; дискретный вход. Подтяжка к верху.
        // * Д1   -   P1.1; дискретный вход. Подтяжка к верху.
        // * Д2   -   P1.2; дискретный вход. Подтяжка к верху.
        // * Uэхз -   P1.3; аналоговый вход. Вх. напряжение 0-3В
        // * Гпри -   P1.4; аналоговый вход
        // * ПСК  -   P1.5; аналоговый вход
        // дисплей    P1.6  - I2C_SDA
        // дисплей    P1.7  - I2C_SCK
        /******************************************************
         * Таблица 12-1. Конфигурация ввода / вывода
         *  PxDIR PxREN PxOUT I/O Configuration
         *    0     0     x   Вход
         *    0     1     0   Вход с понижающим резистором
         *    0     1     1   Вход с подтягивающим резистором
         *    1     x     x   Выход
         *******************************************************/
        //
        P1IES = BIT0 | BIT1 | BIT2;                // Уст. фронт прерывания - с "высокого" на "низкий" P1.3,5 Hi/Lo edge
        P1IE  = BIT0 | BIT1 | BIT2;                // Разрешаем прерывание P1.0,1,2
        //
        // Конфигурируем порт2
        // здесь: P2.4  - кнопка КН4 включения индикации
        //        P2.3  - кнопка КН3
        //        P2.2  - сигнал модема RING_M
        //        P2.7  - СВЕТОДИОД/выход DIR485
        // Конфигурация GPIO
        P2DIR = 0xE7;                   // Устанавливаем всем направление на вывод, кроме P2.3,P2.4 - входы
        P2OUT = 0x7F;                   // Усі виводи високі
        P2IES = BIT3 | BIT4;            // Уст. фронт прерывания - с "высокого" на "низкий"
        P2IFG = 0;                      // Чистим все флаги прерываний P1
        P2OUT  |= BIT2;                 // високий на P2.2
        //
        // Конфигурируем порт4
        //
        P4OUT = 0; P4DIR = 0xFC;
        //
        // Конфигурируем портJ
        // здесь: PJ.0  - кнопка КН1
        //        PJ.1  - кнопка КН2
        //        PJ.2  - выход MICROLAN DRIVE
        //        PJ.3  - вход  MICROLAN SENSE
        //        PJ.4  - выход реле 1 OUT_1
        //        PJ.5  - выход реле 2 OUT_2
        //
        PJDIR = 0xF4;                               // Устанавливаем выводы PJ.0,PJ.1,PJ.3 направлением на ввод
        PJREN = 0x03;                               // Включаем на PJ.0,PJ.1
        PJOUT = 0x00;                               // подтяжку вниз
        PJOUT |= BIT4;                              // OUT_1  - включение нагрузки 1 !!! высокий !!!
        PJOUT |= BIT5;                              // OUT_2  - включение нагрузки 2 !!! высокий !!!

        start_work = 1;                             // флаг "Начата работа" 1- начать чтение данных и передачу; 0 - работа в штатном режиме.
        pack_time  = 3;
        time_rxM = 0;                               // чистим флаг слежения за приемом с модема
        start_transmit = 0;                         // снимаем флаг начала передачи
        start_indi = 0;                             // снимаем флаг начала индикации
        //charge=0;                                   // снимаем флаг "ЗАРЯЖАТЬ ВНУТРЕННИЙ АККУМУЛЯТОР" 0-нет;1-да.
        /*
         * Прямой доступ к регистру во избежание сбоев компилятора - # 10420-D
         * «Для устройств FRAM при запуске необходимо отключить режим
         * высокоомного включения питания GPIO, чтобы активировать ранее
         * настроенные параметры порта.
         * Это можно сделать, очистив бит LOCKLPM5 в регистре PM5CTL0».
         */
        PM5CTL0 &= ~LOCKLPM5;
        //
        // ИНИЦИАЛИЗИРУЕМ ПЕРИФЕРИЮ
        //
        __enable_interrupt();

        if(PJIN & 0x03){
            display = 1;            // есть дисплей
            i2c_open();             // * Открываем порт
        }
        else{
            display = 0;            // нет дисплея
            P2IE &= ~BIT4;          // Запрещаем прерывание P2.4
            P2IE &= ~BIT3;          // Запрещаем прерывание P2.3

        };

        P2OUT &= ~BIT7;             // BКЛючаем LED
        //
        if(display == 1){           // Если есть дисплей, то:
            // то нужно проинициализировать дисплей
            init_LCD ();            // * Инициализируем дисплей
            clear_LCD (0);          // * очищаем экран
            i2c_SetAddress(25, LINE4);
            i2c_PutStrUtf8(vers); // Выводим время
            P2IE  = BIT3 | BIT4;    // Разрешаем прерывание P2.3,4 ()
        }
        //
        // предустанавливаем конфигурацию:
        //
        //for(i=5; i>0; i--)
        for(i=0; i<5; i++)
        {pack.addr[i] = config.addr[i];}  // char addr[4];   // Адрес устройства в системе - "9999"
        for(i=0; i<6; i++)
        {pack.time[i] = config.time[i];}  // char time[6];   //Период между передачами на сервер - "43200"
        // перед измерением напряжения на аккумуляторе:
        P3OUT &= ~BIT6;                             // Отключаем питание от сети HIBER
        P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
        __delay_cycles(1000000);                    // ждем ОТКЛючения внешнего питания...
        //
        P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
        //
        make_measure_bat();                         // измеряем напряжение на аккумуляторе
        //
        // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
        //
        P3OUT = BIT6 | BIT7;                        // HIBER  - включение питания от сети !!! высокий !!!
        __delay_cycles(DATCHIKI);                   // ждем...

        __no_operation();

        get_RMS();
        //
        // Читаем датчик температуры
        //
        get_temperatura();

        P3OUT &= ~BIT7;                             // ОТКЛючение питание DATCHIKI

        __no_operation();
        //
        if(display == 1){                            // Если есть дисплей, то:

                        i = PJIN & 0x03;
                        if(i == 0x01){                          // Если нажата кнопка PJ.1  [ - * - - ]
                            kbd_process();                      // Читаем клавиатуру
                        }

            // то нужно проинициализировать дисплей
            //i2c_open();                         // * Открываем порт
            init_LCD ();                        // * Инициализируем дисплей
            clear_LCD (0);                      // * очищаем экран
            start_indi = 0;                     // снимаем флаг начала индикации
            //
            ItoDecAShot4(sens.a[0].dat, (unsigned char *)&pack.a1);
            ItoDecAShot4(sens.a[1].dat, (unsigned char *)&pack.a2);
            ItoDecAShot4(sens.a[2].dat, (unsigned char *)&pack.a3);
            ItoDecAShot4(sens.a[3].dat, (unsigned char *)&pack.a4);
            ItoDecAShot4(sens.a[4].dat, (unsigned char *)&pack.a5);
            ItoDecAShot4(sens.a[5].dat, (unsigned char *)&pack.a6);
            data_tmp = sens.a[6].dat * 1000 /DEL6847; //0.1264 - коєф.передачи входного делителя напряжения 6.8к/47к
            ItoDecAShot_1dot(data_tmp, (char *)&pack.a7);
            data_tmp = sens.a[7].dat * 1000 /DEL6847; //0.1264 - коєф.передачи входного делителя напряжения 6.8к/47к
            ItoDecAShot_2dot(data_tmp, (char *)&pack.a8);
            //
            //i2c_SetAddress(0, LINE1); i2c_PutStr(&test[0]);       // тестирование строки. 22 символа
            //i2c_SetAddress(20, LINE1); i2c_PutStr(&vers[0]);                                             // текст на экран
            //------------------------------------1234567890123456789012               Р1.0 Р1.1 Р1.2 Р3.0
            //i2c_SetAddress(0, LINE1); i2c_PutStr(" 21.5 1111 >9999 3600 "); // статус,  цифровые входы,     адр.СКМ, таймер.
            i2c_SetAddress( 0, LINE1);  i2c_PutStrUtf8("8.60s      >");
            i2c_SetAddress(38, LINE1);i2c_PutMultySimb((const char *)&digit, 4);            // Выводим значение дискретных входов
            i2c_SetAddress(70, LINE1);i2c_PutMultySimb_znach((const char *)&pack.addr, 5);
            // time_transmit
            ItoDecAShot_bn((pack_time - time_transmit), &ttt[0]);                       // Выводим значение таймера передачи
            i2c_SetAddress(100, LINE1);i2c_PutMultySimb((const char *) &ttt[0],  5);    // Выводим значение таймера передачи
            //
            //                i2c_SetAddress(0, LINE2); i2c_PutStr("                      "); // Выводим значени  Р3.1 Р3.2 Р3.3 Р1.3
            ItoDecAShot_bn((pack_time), &ttt[0]);
            i2c_SetAddress(100, LINE2);i2c_PutMultySimb((const char *) &ttt[0],  5);// Выводим значение таймера передачи
            //i2c_SetAddress(0, LINE3); i2c_PutStr(" 4095 4095 4095 4095  "); // Выводим значения Рвх, Рф,  Рвих, ЭХЗ
            i2c_SetAddress(8,  LINE3); i2c_PutMultySimb((const char *) &pack.a1, 4);
            i2c_SetAddress(44, LINE3); i2c_PutMultySimb((const char *) &pack.a2, 4);
            i2c_SetAddress(74, LINE3); i2c_PutMultySimb((const char *) &pack.a3, 4);
            i2c_SetAddress(104,LINE3); i2c_PutMultySimb((const char *) &pack.a4, 4);
            //
            //i2c_SetAddress(0, LINE5); i2c_PutStr(" 4095 4095 4095 4095  "); // Выводим значения Гпом, ПСК, Uскм, Uснс.
            i2c_SetAddress(8,  LINE4); i2c_PutMultySimb((const char *) &pack.a5, 4);
            i2c_SetAddress(44, LINE4); i2c_PutMultySimb((const char *) &pack.a6, 4);
            //i2c_SetAddress(74, LINE4); i2c_PutMultySimb((const char *) &pack.uuso, 4);
            i2c_SetAddress(74, LINE4); i2c_PutMultySimb((const char *) &pack.a7, 4);
            i2c_SetAddress(104,LINE4); i2c_PutMultySimb((const char *) &pack.a8, 4);
            //
            //i2c_SetAddress(0, LINE5); i2c_PutStr("                      "); // Выводим значени  Р1.4  Р1.5 Р4.0  Р4.1
            i2c_SetAddress(2, LINE5); i2c_PutStr("T:");i2c_PutMultySimb((const char *) &MON_DS, 4);
            //i2c_SetAddress(60,LINE6); i2c_PutStr("OK*");
            //
            //i2c_SetAddress(0, LINE7); i2c_PutStr(" С:12.0  OK*  T:-12.5 "); // Выводим значение
            i2c_SetAddress(2, LINE7); i2c_PutStr("v:");i2c_PutMultySimb((const char *) &pack.bat, 4);
            i2c_SetAddress(46,LINE7); i2c_PutStr("t:");i2c_PutMultySimb((const char *) &pack.temp,5);
            i2c_SetAddress(94,LINE7); i2c_PutStr("c:");i2c_PutMultySimb((const char *) &pack.csq, 4);
            //         sscanf((const char *)&RxMbuf.buf[res_ult], "%d-%d-%d %d:%d", &recivedate.tm_year, &recivedate.tm_mon, &recivedate.tm_mday, &recivedate.tm_hour, &recivedate.tm_min);

            //i2c_SetAddress(0, LINE8); i2c_PutStr(date_m); // Выводим время
            i2c_SetAddress(10, LINE8); i2c_PutStrUtf8(vers); // Выводим время
        }
        //
        // разрешаем прерывания
        //
        __enable_interrupt();

        error = 0;
        alarm_cur  = 0;
        alarm_pred = 0;
        //
        // запускаем Главный цикл
        //
        while(1)
        {
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            // переходим в режим сна LPM4
            //    __bis_SR_register(LPM0_bits + GIE);       // Входим в LPM0 w/ interrupts
            __bis_SR_register(LPM3_bits + GIE);       // LPM3: CPU off, ACLK active
            __no_operation();
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            if(PJIN & 0x03){
                display = 1;            // есть дисплей
                i2c_open();             // * Открываем порт
            }
            else{
                display = 0;            // нет дисплея
                P2IE &= ~BIT4;          // Запрещаем прерывание P2.4
                P2IE &= ~BIT3;          // Запрещаем прерывание P2.3
            };
            //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            //
            // Проверяем ДИСКРЕТНЫЕ ДАТЧИКИ
            //                ПЗК  Д1   Д2   Д3
            // цифровые входы Р1.0|Р1.1|Р1.2|Р3.0
            //
            // вход ПЗК **********************************************************************
            if(P1IN & 0x01){                       // проверка ПЗК
                sens.d[0].dat=1;                   // не было сработки ПЗК
                pack.dio |= 0x01;                  // втручання було
                digit[0] = '1';
                P1IES |= BIT0;                     // Изменяем фронт прерывания на P1.0 - с "высокого" на "низкий" Hi/Lo edge
            }
            else{
                sens.d[0].dat=0;                   // была сработка ПЗК
                pack.dio &= 0xFE;                  // втручання не було
                digit[0] = '0';
                P1IES &= ~BIT0;                    // Изменяем фронт прерывания на P1.0 - с "низкого" на "высокий" Lo/Hi edge
            }// вход Д1  **********************************************************************
            if(P1IN & 0x02){                       // проверка Д1
                sens.d[1].dat=1;                   // не было сработки Д1
                pack.dio |= 0x02;                  // втручання було
                digit[1] = '1';
                P1IES |= BIT1;                    // Изменяем фронт прерывания на P1.1 - с "высокого" на "низкий" Hi/Lo edge
            }
            else{
                sens.d[1].dat=0;                   // была сработка Д1
                pack.dio &= 0xFD;                  // втручання не було
                digit[1] = '0';
                P1IES &= ~BIT1;                    // Изменяем фронт прерывания на P1.1 - с "низкого" на "высокий" Lo/Hi edge
            }// вход Д2  **********************************************************************
            if(P1IN & 0x04){                       // проверка Д2
                sens.d[2].dat=1;                   // не было сработки Д2
                pack.dio |= 0x04;                  // втручання було
                digit[2] = '1';
                P1IES |= BIT2;                     // Изменяем фронт прерывания на P1.2 - с "высокого" на "низкий" Hi/Lo edge
            }
            else{
                sens.d[2].dat=0;                   // была сработка Д2
                pack.dio &= 0xFB;                  // втручання не було
                digit[2] = '0';
                P1IES &= ~BIT2;                    // Изменяем фронт прерывания на P1.2 - с "низкого" на "высокий" Lo/Hi edge
            }// вход Д3  **********************************************************************
            if(P3IN & 0x01){                       // проверка Д3
                sens.d[3].dat=1;                   // не было сработки Д3
                pack.dio |= 0x08;                  // втручання було
                digit[3] = '1';
                P3IES |= BIT0;                     // Изменяем фронт прерывания на P3.0 - с "высокого" на "низкий" Hi/Lo edge
            }
            else{
                sens.d[3].dat=0;                   // была сработка Д3
                pack.dio &= 0xF7;                  // втручання не було
                digit[3] = '0';
                P3IES &= ~BIT0;                    // Изменяем фронт прерывания на P3.0 - с "низкого" на "высокий" Lo/Hi edge
            }
            //
            alarm_cur = pack.dio;                  // запоминаем текущее состояние
            //
            // отключаем светодиодный индикатор
            //
            P2OUT |= BIT7;                         // ОТКЛючаем светодиодный индикатор.
            //
            if(start_work == 1){
                //  Управляем индикатором LED
                //
                P2OUT &= ~BIT7;                    // BКЛючаем LED
                //
                alarm_pred = alarm_cur;     //
                pack_time  = 3;
                //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                //P2OUT  &= ~(BIT6 | BIT5 | BIT1 | BIT0);//+++++++++++// устанавливаем высокими выходы
                //P3OUT  &= ~(BIT5 | BIT4);//+++++++++++++++++++++++++// устанавливаем высокими выходы
                //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                P3OUT &= ~BIT6;                             // Отключаем питание от сети HIBER
                P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
                __delay_cycles(1000000);                    // ждем ОТКЛючения внешнего питания...
                //
                // ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ
                make_measure_bat();                         // измеряем напряжение на аккумуляторе
                //
                // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
                //
                P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
                P3OUT |= BIT7;                              // DATCHIKI  - ВКЛючение датчиков !!! высокий !!!
                __delay_cycles(DATCHIKI);                    // ждем...
                //
                get_RMS();
                //
                // Читаем датчик температуры
                //
                get_temperatura();

                P3OUT &= ~BIT7;                             // ОТКЛючение питание DATCHIKI
                //
                // отключаем светодиодный индикатор
                //
                P2OUT |= BIT7;                              // ОТКЛючаем светодиодный индикатор.

            }
            // срочное сообщение!!!
            alarm_changed = (alarm_cur ^ alarm_pred) & 0x0F;
            if((alarm_changed != 0) && (start_work == 0)){      // если Сработал какой-то датчик - передать срочное сообщение!!!
                if(skip_alarm_after_transmit){
                    alarm_pred = alarm_cur;
                    skip_alarm_after_transmit = 0;
                }
                else{
                //  Управляем индикатором LED
                //
                P2OUT &= ~BIT7;                             // BКЛючаем LED
                //
                alarm_pred = alarm_cur;     // сохраняем
                //
                start_transmit = 1;                         // Устанавливаем флаг "Начать передачу"
                }
            }
            else{
                skip_alarm_after_transmit = 0;
            }
            if(indi > 0){                                   // Если есть разрешение на индикацию
                if(display == 1){                           // Если есть дисплей, то:
                    __no_operation();
                    //
                    //  Управляем индикатором LED
                    //
                    P2OUT &= ~BIT7;                         // BКЛючаем LED
                    //
                    start_work  = 0;
                    //
                    P3OUT &= ~BIT6;                             // Отключаем питание от сети HIBER
                    P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
                    __delay_cycles(1000000);                    // ждем ОТКЛючения внешнего питания...
                    //
                    // ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ
                    make_measure_bat();                         // измеряем напряжение на аккумуляторе
                    //
                    // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
                    //
                    P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
                    P3OUT |= BIT7;                              // DATCHIKI  - ВКЛючение датчиков !!! высокий !!!
                    __delay_cycles(DATCHIKI);                    // ждем...
                    //
                    get_RMS();
                    //
                    // Читаем датчик температуры
                    //
                    get_temperatura();

                    P3OUT &= ~BIT7;                             // ОТКЛючение питание DATCHIKI

                    __no_operation();
                    //
                    // ИНИЦИАЛИЗИРУЕМ ПЕРИФЕРИЮ
                    //
                    if(time_ind < 10){                          // если не закончилось время индикации
                        i = PJIN & 0x03;
                        if(i == 0x01){                          // Если нажата кнопка PJ.1  [ - * - - ]
                            kbd_process();                      // Читаем клавиатуру
                        }
                        if(start_indi == 1){                    // если это начало индикации,
                            // то нужно проинициализировать дисплей
                            i2c_open();                         // * Открываем порт
                            init_LCD ();                        // * Инициализируем дисплей
                            start_indi = 0;                     // снимаем флаг начала индикации
                        }
                        clear_LCD (0);                      // * очищаем экран
                        //
                        ItoDecAShot4(sens.a[0].dat, (unsigned char *)&pack.a1);
                        ItoDecAShot4(sens.a[1].dat, (unsigned char *)&pack.a2);
                        ItoDecAShot4(sens.a[2].dat, (unsigned char *)&pack.a3);
                        ItoDecAShot4(sens.a[3].dat, (unsigned char *)&pack.a4);
                        ItoDecAShot4(sens.a[4].dat, (unsigned char *)&pack.a5);
                        ItoDecAShot4(sens.a[5].dat, (unsigned char *)&pack.a6);
                        //                prep_data(sens.a[6].dat);
                        data_tmp = sens.a[6].dat * 1000 /DEL6847; //0.1264 - коєф.передачи входного делителя напряжения 6.8к/47к
                        ItoDecAShot_1dot(data_tmp, (char *)&pack.a7);
                        //                prep_data(sens.a[7].dat);
                        data_tmp = sens.a[7].dat * 1000 /DEL6847; //0.1264 - коєф.передачи входного делителя напряжения 6.8к/47к
                        ItoDecAShot_2dot(data_tmp, (char *)&pack.a8);
                        //
                        //------------------------------------  1234567890123456789012               Р1.0 Р1.1 Р1.2 Р3.0
                        //i2c_SetAddress(0, LINE1); i2c_PutStr(" 21.5 1111 >9999 3600 "); // статус,  цифровые входы,     адр.СКМ, таймер.
                        i2c_SetAddress( 0, LINE1);i2c_PutStr("8.60s      >");
                        i2c_SetAddress(38, LINE1);i2c_PutMultySimb((const char *)&digit, 4);            // Выводим значение цифровых входов
                        i2c_SetAddress(70, LINE1);i2c_PutMultySimb_znach((const char *) &pack.addr,5);  // Выводим адрес устройства
                        // time_transmit
                        if(pack_time < time_transmit)time_transmit=pack_time;
                        ItoDecAShot_bn((pack_time - time_transmit), &ttt[0]);
                        i2c_SetAddress(100, LINE1);i2c_PutMultySimb((const char *) &ttt[0],  5);// Выводим значение таймера передачи
                        //
                        //                    i2c_SetAddress(0, LINE2); i2c_PutStr("                      "); // Выводим значени  Р3.1 Р3.2 Р3.3 Р1.3
                        ItoDecAShot_bn((pack_time), &ttt[0]);
                        i2c_SetAddress(100, LINE2);i2c_PutMultySimb((const char *) &ttt[0],  5);// Выводим значение таймера передачи
                        //
                        //i2c_SetAddress(0, LINE3); i2c_PutStr(" 4095 4095 4095 4095  "); // Выводим значения Рвх, Рф,  Рвих, ЭХЗ
                        i2c_SetAddress(8,  LINE3); i2c_PutMultySimb((const char *) &pack.a1, 4);
                        i2c_SetAddress(44, LINE3); i2c_PutMultySimb((const char *) &pack.a2, 4);
                        i2c_SetAddress(74, LINE3); i2c_PutMultySimb((const char *) &pack.a3, 4);
                        i2c_SetAddress(104,LINE3); i2c_PutMultySimb((const char *) &pack.a4, 4);
                        //
                        //i2c_SetAddress(0, LINE5); i2c_PutStr(" 4095 4095 4095 4095  "); // Выводим значения Гпом, ПСК, Uскм, Uснс.
                        i2c_SetAddress(8,  LINE4); i2c_PutMultySimb((const char *) &pack.a5, 4);
                        i2c_SetAddress(44, LINE4); i2c_PutMultySimb((const char *) &pack.a6, 4);
                        i2c_SetAddress(74, LINE4); i2c_PutMultySimb((const char *) &pack.a7, 4);
                        //i2c_SetAddress(74, LINE4); i2c_PutMultySimb((const char *) &pack.uuso, 4);//i2c_SetAddress(74, LINE4); i2c_PutMultySimb((const char *) &pack.a7, 4);
                        i2c_SetAddress(104,LINE4); i2c_PutMultySimb((const char *) &pack.a8, 4);
                        //
                        //i2c_SetAddress(0, LINE5); i2c_PutStr("                      "); // Выводим значени  Р1.4  Р1.5 Р4.0  Р4.1
                        i2c_SetAddress(2, LINE5); i2c_PutStr("T:");i2c_PutMultySimb((const char *) &MON_DS, 4);
                        //i2c_SetAddress(60,LINE6); i2c_PutStr("OK*");
                        //
                        //i2c_SetAddress(0, LINE7); i2c_PutStr(" С:12.0  OK*  T:-12.5 "); // Выводим значение
                        i2c_SetAddress(2, LINE7); i2c_PutStr("v:");i2c_PutMultySimb((const char *) &pack.bat, 4);
                        i2c_SetAddress(46,LINE7); i2c_PutStr("t:");i2c_PutMultySimb((const char *) &pack.temp,5);
                        i2c_SetAddress(94,LINE7); i2c_PutStr("c:");i2c_PutMultySimb((const char *) &pack.csq, 4);
                        i2c_SetAddress(0, LINE8); i2c_PutStr(date_m); // Выводим время
                        //i2c_SetAddress(0, LINE8); i2c_PutStr(" 2018-01-14 12:34:56  "); // Выводим время
                    }
                    else{                                       // если закончилось время индикации
                        indi = 0;                               // запрещаем индикацию
                        time_ind=1000;                          // Очищаем таймер индикации
                        start_indi = 0;                         // Снимает флаг "Начать индикацию"
                        P3OUT &= ~BIT7;                         // DATCHIKI - ОТКЛючение питания датчиков !!! высокий !!!
                        lcd_power_off();                        // ОТКЛючаем дисплей
                    }
                }
                // проверяем наличие дисплея
                //            i2c_open();                 // * Открываем порт
                //            read_Status_LCD ();
                if(display == 1){           // Если есть дисплей, то:
                    __no_operation();
                    //
                    P2IE  = BIT3 | BIT4;        // Разрешаем прерывание P2.5,6 от кнопок
                }

            }  // if(indi > 0){                                // Если есть разрешение на индикацию
            //
            //  Управление передачей данных на сервер
            //
            if(start_transmit){                             // Если установлен флаг "Начать передачу"
                //  Управляем индикатором LED
                //
                P2OUT &= ~BIT7;                             // BКЛючаем LED
                //
                restart = 0;                                // Команда меню уже запустила эту передачу
                start_work  = 0;
                //
                P3OUT &= ~BIT6;                             // Отключаем питание от сети HIBER
                P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
                __delay_cycles(1000000);                    // ждем ОТКЛючения внешнего питания...
                //
                // ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ
                make_measure_bat();                         // измеряем напряжение на аккумуляторе
                //
                // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
                //
                P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
                P3OUT |= BIT7;                              // DATCHIKI  - ВКЛючение датчиков !!! высокий !!!
                __delay_cycles(DATCHIKI);                    // ждем...
                //
                get_RMS();
                //
                // Читаем датчик температуры
                //
                get_temperatura();

                P3OUT &= ~BIT7;                             // ОТКЛючение питание DATCHIKI
                //
                if(indi <= 0){                                  // Если НЕТ разрешения на индикацию
                    P3OUT &= ~BIT7;                             // DATCHIKI  - ОТКЛючение питания датчиков !!! высокий !!!
                    PJOUT &= ~BIT4;                             // OUT_1  - ОТКЛючение нагрузки 1 !!! низкий !!!
                    PJOUT &= ~BIT5;                             // OUT_2  - ОТКЛючение нагрузки 2 !!! низкий !!!
                }
                //
                // Настраиваем UART
                //
                init_uart0 ();
                //
                // инициализируем модем
                //
                // обработка отсутствия СИМ карты добавлена 08.09.2018
                // уязвимость - постоянный поиск СИМ приводит к разрядке аккумулятора!
                //
                error = 0;  // снимаем ошибку модема

                //
                //            httpreq_security();    // формуемо кодований HTTP-запит на передачу даних.
                //
                __no_operation();
                //            result = test_packet ();
                __no_operation();
                //
                if(init_sim800c ()){
                    //
                    // передаем пакет
                    //
                    result = send_packet ();
                    //
                    if(result==0){
                        error = 1;  // Есть ошибка передачи
                    }
                }
                else{
                    error = 1;  // Есть ошибка модема
                }
                //
                // Рестарт вызывается искусственно из меню настроек.
                //
                if(restart==1){ // Из меню производится активация передачи ("3.Рестарт устройства ")
                    //
                    //
                    // ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ ПАРАМЕТРОВ
                    //
                    P3OUT &= ~BIT6;                             // Отключаем питание от сети HIBER
                    P3OUT &= ~BIT7;                             // Отключаем питание DATCHIKI
                    __delay_cycles(1000000);                    // ждем ОТКЛючения внешнего питания...
                    //
                    // ВЫПОЛНЯЕМ ИЗМЕРЕНИЕ
                    make_measure_bat();                         // измеряем напряжение на аккумуляторе
                    //
                    // ВКЛЮЧАЕМ ВНЕШНЕЕ ПИТАНИЕ
                    //
                    P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
                    //            __delay_cycles(3000000);                    // ждем включения внешнего питания...
                    P3OUT |= BIT7;                              // DATCHIKI  - ВКЛючение датчиков !!! высокий !!!
                    __delay_cycles(DATCHIKI);                    // ждем...
                    //            PJOUT |= BIT4;                              // OUT_1  - включение нагрузки 1 !!! высокий !!!
                    //            PJOUT |= BIT5;                              // OUT_2  - включение нагрузки 2 !!! высокий !!!
                    //            __delay_cycles(1000000);                    // ждем...
                    //
                    //
                    get_RMS();
                    //
                    // Читаем датчик температуры
                    //
                    get_temperatura();

                    //                P3OUT &= ~BIT7;                             // ОТКЛючение питание DATCHIKI
                    //
                    if(indi <= 0){                              // Если НЕТ разрешения на индикацию
                        P3OUT &= ~BIT7;                         // DATCHIKI  - ОТКЛючение питания датчиков !!! высокий !!!
                        PJOUT &= ~BIT4;                         // OUT_1  - ОТКЛючение нагрузки 1 !!! низкий !!!
                        PJOUT &= ~BIT5;                         // OUT_2  - ОТКЛючение нагрузки 2 !!! низкий !!!
                    }
                    //
                    result = send_packet ();                    // передаем пакет
                    //
                    if(result==0){
                        error = 1;  // Есть ошибка передачи
                    }
                    restart=0;                                  // Отключаем (заканчиваем) процедуру рестарта
                }
                //*******************************************************************************************************************
                //
                // ПОСЛЕ ОКОНЧАНИЯ ПЕРЕДАЧИ ОТКЛЮЧАЕМ ПЕРИФЕРИЮ:
                //
                //*******************************************************************************************************************
                //
                //            PJOUT &= ~BIT4;                             // OUT_1  - ОТКЛючение нагрузки 1 !!! низкий !!!
                //            PJOUT &= ~BIT5;                             // OUT_2  - ОТКЛючение нагрузки 2 !!! низкий !!!
                //
                // отключаем выводы модуля UART0
                //
                GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0 + GPIO_PIN1);
                //
                P2DIR = 0xE3;                               // Устанавливаем всем направление на вывод, кроме P2.2,P2.3,P2.4 - входы
                P2REN = 0x18;                               // Включаем на P2.3,P2.4
                P2OUT = 0x18;                               // подтяжку вверх
                P2IES = BIT2 | BIT3 | BIT4;                 // Уст. фронт прерывания - с "высокого" на "низкий" P1.5,6 Hi/Lo edge
                //
                // отключаем DTR вверх (P2.2)
                //
                P3OUT &= ~BIT4;
                //
                // снимаем PWRKEY вниз (P3.5)
                //
                P3OUT &= ~BIT5;
                //
                // ОТКЛючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;                             // 4 - M_ON - включение модема
                //
                // отключаем дисплей, если он есть
                //
                lcd_power_off();
                //
                // отключаем АЦП
                //
                ADC12CTL0 &= ~ADC12ENC;                     // Запрещаем измерения  ADC12_A
                ADC12CTL0 &= ~ADC12ON;                      // Отключаем модуль ADC12_A
                REFCTL0   &= ~REFON;                        // ВЫКЛючаем внутреннее Опорное Напряжение

                time_transmit = 0;                          // Обнуляем таймер передачи
                start_transmit= 0;                          // Снимаем флаг "Начать передачу"
                skip_alarm_after_transmit = 1;               // Первый цикл после передачи только синхронизирует входы

                P3OUT &= ~BIT7;                             // DATCHIKI  - ОТКЛючение питания датчиков !!! высокий !!!
            }
            //
            // отключаем светодиодный индикатор
            //
            P2OUT |= BIT7;                                  // ОТКЛючаем светодиодный индикатор.

            //
            // отключаем монитор
            //        P1.6  - I2C_SDA - дисплей
            //        P1.7  - I2C_SCK - дисплей
            P1DIR |= BIT6;                                  // P1.6 - направление на выход
            P1DIR |= BIT7;                                  // P1.7 - направление на выход
            P1OUT |= BIT6;                                  // Выход высокий
            P1OUT |= BIT7;                                  // Выход высокий
            P1REN &= ~BIT6;                                 // ОТключаем резистор подтяжки
            P1REN &= ~BIT7;                                 // ОТключаем резистор подтяжки
            //
            if(error){    // если есть ошибка модема
                // Запрещаем прерывания:
                P1IE  &= ~BIT0;
                P1IE  &= ~BIT1;
                P1IE  &= ~BIT2;
                P3IE  &= ~BIT0;
                //
                P1IFG = 0;                                  // Чистим все флаги прерываний P1
                P3IFG = 0;                                  // Чистим все флаги прерываний P3
                //
            }
            //
            // настраиваем прерывания для дальнейшей работы:
            //
            ADC12IER1   = ADC12IE31;                    // Разрешаем прерывание после завершения преобразования на канале А31
            SFRIE1 |= WDTIE;                            // Разрешаем прерывание таймеру WDT
            //        P1REN  = BIT4 | BIT5;                       // Включаем подтяжку вверх на P1.2,3,5
            //        P1IES |= BIT5 | BIT4;                       // Уст. фронт прерывания - с "высокого" на "низкий" P1.5 Hi/Lo edge
            if(P1IN & 0x08){                            // проверка "датчика втручання"
                pack.dio = 0;                           // втручання не було
                P1IES |= BIT3;                          // Изменяем фронт прерывания на P1.3 - с "высокого" на "низкий" Hi/Lo edge
            }
            else{
                pack.dio = 1;                           // вручання було
                P1IES &= ~BIT3;                         // Изменяем фронт прерывания на P1.3 - с "низкого" на "высокий" Lo/Hi edge
            }

            P2IFG = 0;                                  // Чистим все флаги прерываний P2
            P2IE  = BIT3 | BIT4;                        // Разрешаем прерывание P2.3,P2.4 от кнопок
            //        P3IES |= BIT0;                              // Уст. фронт прерывания - с "высокого" на "низкий" P3.0 Hi/Lo edge
            P3IFG &= ~BIT0;                             // Чистим флаг прерывания P3.0 IFG
            P3IE  |= BIT0;                              // РАЗРЕШАЕМ прерывание P3.0
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            //P2OUT  = BIT1 | BIT0;//+++++++++++// категорически НЕТ! устанавливаем высокими выходы
            //P2OUT  = BIT6 | BIT5;//+++++++++++// категорически НЕТ! устанавливаем высокими выходы
            //P3OUT  = BIT5 | BIT4;//+++++++++++++++++++++++++// НЕТ! устанавливаем высокими выходы
            //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

            if(start_work == 1){
                P3OUT |= BIT6;                            // HIBER  - включение питания от сети !!! высокий !!!
            }
        } //    while(1)
    }













    //******************************************************************************
    //******************************************************************************
    //******************************************************************************
    //
    // Это процедуры обработки векторов прерываний
    //
    //******************************************************************************
    //******************************************************************************
    //******************************************************************************
    //*
    // Port 2 interrupt service routine
    //#pragma vector=PORT2_VECTOR
    __interrupt void Port_2(void)
    {
        if(P2IFG & BIT3){                           // Если нажата кнопка P2.3         [ - - * - ]
            P2IFG &= ~BIT3;                         // Чистим флаг прерывания P2.3 IFG
            time_ind=1000;                          // Очищаем таймер индикации
            start_indi = 0;                         // Снимает флаг "Начать индикацию"
            P2IE  |= BIT3;                          // РАЗРЕШАЕМ прерывание P2.3
            //
            __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
        }
        if(P2IFG & BIT4){                           // Если нажата кнопка P2.4          [ - - - * ]
            P2IFG &= ~BIT4;                         // Чистим флаг прерывания P2.4 IFG
            indi=30;                                // разрешить индикацию
            time_ind=0;                             // Очищаем таймер индикации
            start_indi = 1;                         // Устанавливаем флаг "Начать индикацию"
            alarm_pred = alarm_cur =0;     //
            P2IE  |= BIT4;                          // РАЗРЕШАЕМ прерывание P2.4
            //
            __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
        }
    }

    // Port 3 interrupt service routine
    //#pragma vector=PORT3_VECTOR
    __interrupt void Port_3(void)
    {
        unsigned char buffer;
        unsigned char time_out;
        //
        buffer = 0xAA;
        P2OUT &= ~BIT7;                 // BКЛючаем LED
        if(P3IFG & BIT0){                   // Если сработал датчик(геркон) на входе Д3/ЗАЩИТА (порт P3.0)
            //
            // Ждем, пока состояние кнопки станет ясным или закончится выяснение состояния
            //
            time_out = 100;

            while (time_out-- > 0)
            {
                // Содержание 8 местного (битного) положения буфера
                // Все предыдущие состояния (биты) сдвигаются влево и вправо
                // добавляется новое состояние (бит).
if(P3IES & BIT0){
    buffer <<= 1;
    buffer |= ((P3IN & BIT0) == 0) ? 0x01 : 0x00;
}else{
    buffer <<= 1;
    buffer |= (P3IN & BIT0) ? 0x01 : 0x00;
}
                // Если все 8 бит в верхнем состоянии,то
                // кнопка точно нажата
                if (buffer == 0xFF)
                {
                    sens.d[3].dat=1;                // была сработка Д3
                    P3IFG &= ~BIT0;                 // Чистим флаг прерывания P1.0 IFG
                    P3IE  |= BIT0;                  // РАЗРЕШАЕМ прерывание P1.0
                    //
                    P3OUT |= BIT6;                  // HIBER  - включение питания от сети !!! высокий !!!
                    time_out= 0;
                    //
                    P2OUT |= BIT7;              // ОТКЛючаем светодиодный индикатор.
                    //
                    __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
                }
                //
                // Пауза 1 миллисекунда
                //
                __delay_cycles(MCLK_FREQUENCY / 1000);   // 1 ms
            }
            sens.d[3].dat=0;                        // не было сработки Д3
            P3IFG &= ~BIT0;                         // Чистим флаг прерывания P3.0 IFG
            P3IE  |= BIT0;                          // РАЗРЕШАЕМ прерывание P3.0
        }
    }
    // */
    // Port 1 interrupt service routine
    // #pragma vector=PORT1_VECTOR
    __interrupt void Port_1(void)
    {
        //*
        unsigned char buffer;
        unsigned char time_out;

        P2OUT &= ~BIT7;             // BКЛючаем LED
        //
        if(P1IFG & BIT0){                           // Если сработал датчик(геркон) на входе ПЗК(порт P1.0)
            // вход ПЗК  **********************************************************************
            //
            // Ждем, пока состояние кнопки станет ясным или закончится выяснение состояния
            //
            buffer = 0xAA;
            time_out = 100;

            while (time_out-- > 0)
            {
                // Содержание 8 местного (битного) положения буфера
                // Все предыдущие состояния (биты) сдвигаются влево и вправо
                // добавляется новое состояние (бит).
if(P1IES & BIT0){
    /*
     * Очікуємо перехід High -> Low.
     * Записуємо 1, якщо P1.0 реально LOW.
     */
    buffer <<= 1;
    buffer |= ((P1IN & BIT0) == 0) ? 0x01 : 0x00;
}
else{
    /*
     * Очікуємо перехід Low -> High.
     * Записуємо 1, якщо P1.0 реально HIGH.
     */
    buffer <<= 1;
    buffer |= (P1IN & BIT0) ? 0x01 : 0x00;
}
                // Если все 8 бит в верхнем состоянии,то
                // кнопка точно нажата
                if (buffer == 0xFF)
                {
                    sens.d[0].dat=1;           // была сработка ПЗК
                    P1IFG &= ~BIT0;                 // Чистим флаг прерывания P1.0 IFG
                    P1IE  |= BIT0;                  // РАЗРЕШАЕМ прерывание P1.0
                    //
                    P3OUT |= BIT6;                  // HIBER  - включение питания от сети !!! высокий !!!
                    time_out= 0;
                    //
                    P2OUT |= BIT7;              // ОТКЛючаем светодиодный индикатор.
                    //
                    __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
                }
                //
                // Пауза 1 миллисекунда
                //
                __delay_cycles(MCLK_FREQUENCY / 1000);   // 1 ms
            }
            sens.d[0].dat=0;                   // не было сработки ПЗК
            P1IFG &= ~BIT0;                         // Чистим флаг прерывания P1.2 IFG
            P1IE  |= BIT0;                          // РАЗРЕШАЕМ прерывание P1.2

        }
        if(P1IFG & BIT1){                           // Если сработал датчик(геркон) на входе Д1(порт P1.1)
            /*
             * Каждый бит PxIES выбирает край прерывания для соответствующего ввода-вывода.
             * • Бит = 0: Соответствующий флаг PxIFG устанавливается при переходе с низкого к высокому
             * • Бит = 1: соответствующий флаг PxIFG установлен на переходе с высокого на низкий
             *
             * Запись в P1IES или в P2IES для каждого соответствующего ввода-вывода может
             * привести к установке соответствующих флагов прерывания:
             * PxIES  PxIN PxIFG
             * 0 -> 1  0   Будет установлено
             * 0 -> 1  1   Без изменений
             * 1 -> 0  0   Без изменений
             * 1 -> 0  1   Будет установлено
             */
            // вход Д1  **********************************************************************
            //
            // Ждем, пока состояние кнопки станет ясным или закончится выяснение состояния
            //
            buffer = 0xAA;
            time_out = 100;

            while (time_out-- > 0)
            {
                // Содержание 8 местного (битного) положения буфера
                // Все предыдущие состояния (биты) сдвигаются влево и вправо
                // добавляется новое состояние (бит).
if(P1IES & BIT1){
    buffer <<= 1;
    buffer |= ((P1IN & BIT1) == 0) ? 0x01 : 0x00;
}else{
    buffer <<= 1;
    buffer |= (P1IN & BIT1) ? 0x01 : 0x00;
}

                // Если все 8 бит в верхнем состоянии,то
                // кнопка точно нажата
                if (buffer == 0xFF)
                {
                    sens.d[1].dat=1;                // была сработка Д1
                    P1IFG &= ~BIT1;                 // Чистим флаг прерывания P1.1 IFG
                    P1IE  |= BIT1;                  // РАЗРЕШАЕМ прерывание P1.1
                    //
                    P3OUT |= BIT6;                  // HIBER  - включение питания от сети !!! высокий !!!
                    time_out= 0;
                    //
                    P2OUT |= BIT7;              // ОТКЛючаем светодиодный индикатор.
                    //
                    __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
                }
                //
                // Пауза 1 миллисекунда
                //
                __delay_cycles(MCLK_FREQUENCY / 1000);   // 1 ms
                __no_operation();
            }
            sens.d[1].dat=0;                        // не было сработки Д1
            P1IFG &= ~BIT1;                         // Чистим флаг прерывания P1.1 IFG
            P1IE  |= BIT1;                          // РАЗРЕШАЕМ прерывание P1.1

        }
        if(P1IFG & BIT2){                           // Если сработал датчик(геркон) на входе Д2(порт P1.2)
            //
            // Ждем, пока состояние кнопки станет ясным или закончится выяснение состояния
            //
            buffer = 0xAA;
            time_out = 100;

            while (time_out-- > 0)
            {
                // Содержание 8 местного (битного) положения буфера
                // Все предыдущие состояния (биты) сдвигаются влево и вправо
                // добавляется новое состояние (бит).
if(P1IES & BIT2){
    buffer <<= 1;
    buffer |= ((P1IN & BIT2) == 0) ? 0x01 : 0x00;
}else{
    buffer <<= 1;
    buffer |= (P1IN & BIT2) ? 0x01 : 0x00;
}


                // Если все 8 бит в верхнем состоянии,то
                // кнопка точно нажата
                if (buffer == 0xFF)
                {
                    sens.d[2].dat=1;                // была сработка Д2
                    P1IFG &= ~BIT2;                 // Чистим флаг прерывания P1.2 IFG
                    P1IE  |= BIT2;                  // РАЗРЕШАЕМ прерывание P1.2
                    //
                    P3OUT |= BIT6;                  // HIBER  - включение питания от сети !!! высокий !!!
                    time_out= 0;
                    //
                    P2OUT |= BIT7;              // ОТКЛючаем светодиодный индикатор.
                    //
                    __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
                }
                //
                // Пауза 1 миллисекунда
                //
                __delay_cycles(MCLK_FREQUENCY / 1000);   // 1 ms
            }
            sens.d[2].dat=0;                   // не было сработки Д2
            P1IFG &= ~BIT2;                         // Чистим флаг прерывания P1.2 IFG
            P1IE  |= BIT2;                          // РАЗРЕШАЕМ прерывание P1.2
        }
        if(P1IFG & BIT3){                           // (порт P1.3) ЕХЗ
            P1IFG &= ~BIT3;                         // Чистим флаг прерывания P1.3 IFG
            P1IE  &= ~BIT3;                         // ЗАПРЕЩАЕМ прерывание P1.4
        }
        if(P1IFG & BIT4){                           // (порт P1.4) Гпом
            P1IFG &= ~BIT4;                         // Чистим флаг прерывания P1.3 IFG
            P1IE  &= ~BIT4;                         // ЗАПРЕЩАЕМ прерывание P1.4
        }
        if(P1IFG & BIT5){                           // (порт P1.5) ПСК
            P1IFG &= ~BIT5;                         // Чистим флаг прерывания P1.5 IFG
            P1IE  &= ~BIT5;                         // ЗАПРЕЩАЕМ прерывание P1.4
        }
    }
    // Watchdog Timer interrupt service routine
    //#pragma vector=WDT_VECTOR
    __interrupt void WDT_ISR(void)
    {

        if(time_transmit <= pack_time){
            time_transmit++;                       // Глобальный счетчик - ожидаем передачу данных на сервер
            __no_operation();
        }
        if((time_transmit >= pack_time)&&(start_transmit == 0)){
            //
            P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
            P3OUT |= BIT7;                              // DATCHIKI  - включение питания датчиков !!! высокий !!!
            //        PJOUT |= BIT4;                              // OUT_1  - включение нагрузки 1 !!! высокий !!!
            //        PJOUT |= BIT5;                              // OUT_2  - включение нагрузки 2 !!! высокий !!!
            //
            start_transmit = 1;                     // Устанавливаем флаг "Начать передачу"
            start_work  = 0;
            __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
            //        __bic_SR_register_on_exit(LPM0_bits);   // Выходим из LPM4
        }
        if((pack_time > 2)&&(time_transmit >= pack_time-2)&&(start_transmit == 0)){ // Готовимся к передаче. Сначала включаем питание...
            //
            P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
            //
        }
        if((pack_time > 1)&&(time_transmit >= pack_time-1)&&(start_transmit == 0)){ // Готовимся к передаче. Затем включаем датчики
            //
            P3OUT |= BIT6;                              // HIBER  - включение питания от сети !!! высокий !!!
            P3OUT |= BIT7;                              // DATCHIKI  - включение питания датчиков !!! высокий !!!
            //        PJOUT |= BIT4;                              // OUT_1  - включение нагрузки 1 !!! высокий !!!
            //        PJOUT |= BIT5;                              // OUT_2  - включение нагрузки 2 !!! высокий !!!
            //
        }
        if(indi > 0){                               // если есть разрешение на индикацию
            time_ind++;                             // ведем таймер индикации
        }

        __bic_SR_register_on_exit(LPM3_bits);       // Выходим из LPM3 после каждого тика WDT
    }

    #pragma CODE_SECTION(USCI_A0_ISR, ".text:_isr")
    /* #pragma vector=USCI_A0_VECTOR */
    __interrupt void USCI_A0_ISR(void)
    {
        struct rxM_buf *p;
        U8 rbr;

        switch(__even_in_range(UCA0IV,USCI_UART_UCTXCPTIFG))
        {
            case USCI_NONE: break;
            case USCI_UART_UCRXIFG:

                P2OUT &= ~BIT7;                         // BКЛючаем LED

                UCA0IFG &= ~UCRXIFG;                    // Clear interrupt

                p = &RxMbuf;
                // Берем следующий символ из буфера ФИФО.
                rbr = UCA0RXBUF;
                /*        //if ((p->ind + 1) != p->max) {
                 *        if ((p->ind + 1) < p->max) {
                 *          p->buf [p->ind++] = (U8)rbr;
                 *          time_rxM = 0;
        }
        else{
            p->ind =0;
        }
        // */
                if (p->ind < p->max) {
                    p->buf[p->ind++] = (U8)rbr;
                    time_rxM = 0;
                }
                P2OUT |= BIT7;                          // ОТКЛючаем LED
                break;
            case USCI_UART_UCTXIFG: break;
            case USCI_UART_UCSTTIFG: break;
            case USCI_UART_UCTXCPTIFG: break;
        }
    }

    #pragma vector=ADC12_VECTOR
    __interrupt void ADC12ISR (void)
    {
        switch(__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG))
        {
            case ADC12IV_NONE:        break;            // Vector  0:  No interrupt
            case ADC12IV_ADC12OVIFG:  break;            // Vector  2:  ADC12MEMx Overflow
            case ADC12IV_ADC12TOVIFG: break;            // Vector  4:  Conversion time overflow
            case ADC12IV_ADC12HIIFG:                    // Vector  6:  ADC12BHI
                //
                // Disable the high side and enable the low side interrupt.
                //
                ADC12IER2  &= ~ADC12HIIE;
                ADC12IER2  |=  ADC12LOIE;
                ADC12IFGR2 &= ~ADC12LOIFG;
                break;
            case ADC12IV_ADC12LOIFG:                    // Vector  8:  ADC12BLO
                //
                // Enter device shutdown with 64ms timeout.
                //
                //        ctpl_enterShutdown(CTPL_SHUTDOWN_TIMEOUT_64_MS);

                // Disable the low side and enable the high side interrupt.
                ADC12IER2  &= ~ADC12LOIE;
                ADC12IER2  |=  ADC12HIIE;
                ADC12IFGR2 &= ~ADC12HIIFG;
                break;

            case ADC12IV_ADC12INIFG:  break;            // Vector 10:  ADC12BIN
            case ADC12IV_ADC12IFG0:   break;            // Vector 12:  ADC12MEM0 Interrupt
            //
            //
            //
            //    __bic_SR_register_on_exit(LPM4_bits); // Exit active CPU

            case ADC12IV_ADC12IFG22:  break;            // Vector 56:  ADC12MEM22
            case ADC12IV_ADC12IFG23:  break;            // Vector 58:  ADC12MEM23
            case ADC12IV_ADC12IFG24:  break;            // Vector 60:  ADC12MEM24
            case ADC12IV_ADC12IFG25:  break;            // Vector 62:  ADC12MEM25
            case ADC12IV_ADC12IFG26:  break;            // Vector 64:  ADC12MEM26
            case ADC12IV_ADC12IFG27:  break;            // Vector 66:  ADC12MEM27
            case ADC12IV_ADC12IFG28:  break;            // Vector 68:  ADC12MEM28
            case ADC12IV_ADC12IFG29:  break;            // Vector 70:  ADC12MEM29
            case ADC12IV_ADC12IFG30:  break;            // Vector 72:  ADC12MEM30
            case ADC12IV_ADC12IFG31:                    // Vector 74:  ADC12MEM31
                //
                //
                /*
                 *        sens.a[0].dat = ADC12MEM22;        // напряжение на a13 - Рвх
                 *        sens.a[1].dat = ADC12MEM23;        // напряжение на a14 - Рф
                 *        sens.a[2].dat = ADC12MEM24;        // напряжение на a15 - Рвих
                 *        sens.a[3].dat = ADC12MEM25;        // напряжение на a3  - ЭХЗ
                 *        sens.a[4].dat = ADC12MEM26;        // напряжение на a4  - Гпри
                 *        sens.a[5].dat = ADC12MEM27;        // напряжение на a5  - ПСК
                 *        sens.a[6].dat = ADC12MEM28;        // напряжение на a8  - Uскм
                 *        sens.a[7].dat = ADC12MEM29;        // напряжение на a9  - Uснс-датчиков
                 *        //sens.v[0].dat = ADC12MEM30; // значение температуры MSP430FR
                 *        sens.v[1].dat = ADC12MEM31; // значение напряжения питания MSP430FR
                 *        uuso = ADC12MEM28; // значение напряжения питания MSP430FR
                 * // */
                temp = ADC12MEM30; // значение температуры MSP430FR
                volt = ADC12MEM31; // значение напряжения питания MSP430FR
                //
                __bic_SR_register_on_exit(LPM3_bits);   // Выходим из LPM4
                //        __bic_SR_register_on_exit(LPM0_bits);   // Выходим из LPM4
                break;

            case ADC12IV_ADC12RDYIFG: break;            // Vector 76:  ADC12RDY
            default: break;
        }
    }

    //******************************************************************************
    // Интерфейс I2C **************************************************************
    //******************************************************************************
    #pragma vector = USCI_B0_VECTOR
    __interrupt void USCI_B0_ISR(void)
    {
        //Необходимо прочитать из UCB0RXBUF
        uint8_t rx_val;
        unsigned int timer;
        switch(__even_in_range(UCB0IV, USCI_I2C_UCBIT9IFG))
        {
            case USCI_NONE:          break;         // Vector 0: No interrupts
            case USCI_I2C_UCALIFG:   break;         // Vector 2: ALIFG
            case USCI_I2C_UCNACKIFG:                // Vector 4: NACKIFG
                UCB0CTLW0 |= UCTXSTP;               // send STOP condition
                MasterMode = IDLE_MODE;
                __bic_SR_register_on_exit(LPM0_bits); // exit sleep - prevent hang
                break;
            case USCI_I2C_UCSTTIFG:  break;         // Vector 6: STTIFG
            case USCI_I2C_UCSTPIFG:  break;         // Vector 8: STPIFG
            case USCI_I2C_UCRXIFG3:  break;         // Vector 10: RXIFG3
            case USCI_I2C_UCTXIFG3:  break;         // Vector 12: TXIFG3
            case USCI_I2C_UCRXIFG2:  break;         // Vector 14: RXIFG2
            case USCI_I2C_UCTXIFG2:  break;         // Vector 16: TXIFG2
            case USCI_I2C_UCRXIFG1:  break;         // Vector 18: RXIFG1
            case USCI_I2C_UCTXIFG1:  break;         // Vector 20: TXIFG1
            //
            case USCI_I2C_UCRXIFG0:                 // Vector 22: RXIFG0 - флаг начать прием RX
                //        RXData = UCB0RXBUF;                       // Get RX data
                rx_val = UCB0RXBUF;
                if (RXByteCtr && (ReceiveIndex < I2C_BUFFER_SIZE))
                {
                    ReceiveBuffer[ReceiveIndex++] = rx_val;
                    RXByteCtr--;
                }
                else if (RXByteCtr)
                {
                    RXByteCtr = 0;
                    UCB0IE &= ~UCRXIE;
                    UCB0CTLW0 |= UCTXSTP;
                    MasterMode = TIMEOUT_MODE;
                    __bic_SR_register_on_exit(LPM0_bits);
                    break;
                }

                if (RXByteCtr == 1)
                {
                    UCB0CTLW0 |= UCTXSTP;                         // Посылаем условие СТОП
                }
                else if (RXByteCtr == 0)
                {
                    UCB0IE &= ~UCRXIE;
                    UCB0CTLW0 |= UCTXSTP;                         // Посылаем условие СТОП
                    MasterMode = IDLE_MODE;
                    __bic_SR_register_on_exit(LPM0_bits);   // Выходим из LPM4
                }
                break;
                //
            case USCI_I2C_UCTXIFG0:                             // Vector 24: TXIFG0 - начать передачу TX
                switch (MasterMode)
                {
                    case TX_REG_ADDRESS_MODE:
                        if (RXByteCtr){
                            if(RXByteCtr && TXByteCtr==1){
                                UCB0TXBUF = 0x10;
                                MasterMode = TX_DATA_MODE;
                            }
                            else MasterMode = SWITCH_TO_RX_MODE;  // Переключиться на режим приема. Нужно начать получать сейчас
                        }
                        else{
                            UCB0TXBUF = TransmitRegAddr;          // Адрес регистра у подчиненного
                            MasterMode = TX_DATA_MODE;            // Продолжить передачу с данными в буфере передачи
                        }
                        break;

                    case SWITCH_TO_RX_MODE:
                        UCB0CTLW0 |= UCTXSTP;                     // Send stop condition
                        UCB0IE |= UCRXIE;                         // Включаем прерывание RX
                        UCB0IE &= ~UCTXIE;                        // ОТКЛючаем прерывание TX
                        UCB0CTLW0 &= ~UCTR;                       // Переключаемся на приемник
                        MasterMode = RX_DATA_MODE;                // Состояние - принимаемые данные State state is to receive data
                        UCB0CTLW0 |= UCTXSTT;                     // Посылаем повторный СТАРТ Send repeated start
                        if (RXByteCtr == 1)
                        {
                            timer=0;// ожидание проводим с таймером
                            //Must send stop since this is the N-1 byte
                            while((UCB0CTLW0 & UCTXSTT)){
                                timer++;if(timer>50000)break; // таймер прерывания цикла
                            };
                            UCB0CTLW0 |= UCTXSTP;      // Send stop condition
                        }
                        break;

                    case TX_DATA_MODE:
                        if (TXByteCtr)
                        {
                            if(RXByteCtr){
                                UCB0TXBUF = TransmitRegAddr;  // Адрес регистра у подчиненного
                                TXByteCtr--;
                                MasterMode = SWITCH_TO_RX_MODE;   // Need to start receiving now
                            }else{
                                UCB0TXBUF = TransmitBuffer[TransmitIndex++];
                                TXByteCtr--;
                            }
                        }
                        else
                        {
                            // Передача окончена. Done with transmission - Сделано с передачей
                            UCB0CTLW0 |= UCTXSTP;     // Send stop condition
                            MasterMode = IDLE_MODE;
                            UCB0IE &= ~UCTXIE;                       // disable TX interrupt
                            __bic_SR_register_on_exit(LPM0_bits);   // Выходим из LPM4
                        }
                        break;

                    default:
                        UCB0TXBUF = TXData++;
                        __no_operation();
                        break;
                }
                break;
                    default: break;
        }
    }
