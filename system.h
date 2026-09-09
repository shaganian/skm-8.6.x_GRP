#include <stdio.h>
#include <stdint.h>
//
// определение типов переменных
//*************************************************************************
// Пользовательские настройки типов переменных
typedef signed char S8;
//typedef unsigned char   uint8_t;
typedef unsigned char U8;
typedef short S16;
typedef unsigned short U16;
typedef long S32;
typedef unsigned long U32;
typedef long long S64;
typedef unsigned long long U64;
typedef unsigned char BIT;
typedef unsigned int BOOL;

typedef unsigned char u_char; // портированный тип данных
typedef unsigned short u_int;  // портированный тип данных
typedef unsigned int u_long; // портированный тип данных

#define I2C_BUFFER_SIZE       20u
#define ENCRDATA_SIZE        272u
#define CPU_CLOCK_HZ          1000000UL
#define DELAY_MS_CYCLES       (CPU_CLOCK_HZ / 1000UL)
#define MODEM_TCP_CONNECT_TIMEOUT_MS  60000u
#define MODEM_POLL_INTERVAL_MS        10u
#define MODEM_GPRS_BRINGUP_TIMEOUT_MS 85000u
#define MODEM_IP_ADDRESS_TIMEOUT_MS   10000u
#define DEFAULT_PACK_TIME_TICKS       424u
#define OTA_STAGE_ORIGIN              0x8400UL
#define OTA_STAGE_SIZE                0x7B80UL
#define OTA_FORMAT_VERSION            1u
/*
const char vers_num[]="8.60"; // 31.05.2026 - Працює від зовнішнього живлення. Якщо його нема, то від малого акумулятора.
const char vers_type[]="aes."; // 31.05.2026 - Працює від зовнішнього живлення. Якщо його нема, то від малого акумулятора.
const char vers_dev[]="-ШГРП"; // 31.05.2026 - Працює від зовнішнього живлення. Якщо його нема, то від малого акумулятора.
const char vers[]   ="СКМ-"+vers_num+vers_type+vers_dev; // 31.05.2026 - Працює від зовнішнього живлення. 
Якщо його нема, то від малого акумулятора.
*/


/*
  Устройство имеет два архива часовой и суточный.
  Часовой архив расчитан на 2 месяца. Имеет две зоны по 1 месяцу
   31 день * 24 часа = 744 записи для каждого месяца.
    Сюда запись производится один раз в час.
  Суточный архив рассчитан на 2 года. Имеет две зоны по 1 году:
    -  366 дней для каждого года.
    Сюда запись производится один раз в сутки,
    В 0 часов текущего дня записывается значение прошедшего дня.*/

// */

typedef struct archive
{ // Структура архива.
    char date[3];   // Дата записи.
    char cnt1[3];   // Счетчик.
    char dio;       // состояние входа "ВТРУЧАННЯ" - "0"
} archive;

typedef struct calibr
{ // Структура измерения.
    float koeff;    // Множитель.
    float value;    // Значение измеренное.
} calibr;

/*
 * Формат пакета PAC, формируемого для передачи на сервер.
 *
 * END_DEV_ADDR | TYPE | CSQ | BAT | TEMP | DIO | CNT1 | TIME
 *       4         4      5     4     5      1      9     6    == 38 байт ASCII
 *
 * END_DEV_ADDR – адрес конечного устройства отправителя пакета (1234). Длина – 4 байта ASCII.
 * TYPE - тип устройства - идентификатор (0008). Длина – 4 байта ASCII.
 * CSQ  – Сила сигнала GSM (23.00). Длина – 5 байт ASCII.
 * BAT  – напряжение на элементе питания Конечного устройства (3.15). Длина – 4 байта ASCII.
 * TEMP – температура в градусах Цельсия в месте установки Конечного устройства
 *        по показаниям встроенного датчика. Число со знаком (+25.5). Длина – 5 байт ASCII.
 * DIO  - состояние цифровых входов/выходов. Битовая маска длиной 8 бит (F2). Длина – 1 байт ASCII.
 * CNT1 – количество импульсов, поступившее на первый счетный вход. физическая длина 24 бита (00054321).
 *        Передается в виде 8 байт ASCII.
 * TIME - Период между передачами на сервер. В минутах: 30дней * 24 часа * 60 мин. = 43200 мин.
 *
 */
typedef struct skm8_packet
{    // Пакет данных для передачи на сервер.
    char addr[5];   // Адрес устройства в системе - "9999"
    char type[4];   // тип устройства - идентификатор "0008"
    char bat[4];   // напряжение на элементе питания "3.15"
    char temp[5];   // температура в градусах Цельсия со знаком - "+25.5"
    char csq[5];   // Сила сигнала GSM - "31.99"
    char koef[5];   // Коэффициент счетчика эл.энергии "6400"
    char time[6]; // Период между передачами на сервер - "43200". [6]- здесь 0 - конец строки
    char dio;   // цифровые входы Р1.0|Р1.1|Р1.2|Р3.0
    char a1[4];   // напряжение на a13 - Рвх
    char a2[4];   // напряжение на a14 - Рф
    char a3[4];   // напряжение на a15 - Рвих
    char a4[4];   // напряжение на a3  - ЭХЗ
    char a5[4];   // напряжение на a4  - Гпри
    char a6[4];   // напряжение на a5  - ПСК
    char a7[4];   // напряжение на a8  - Uскм
    char a8[4];   // напряжение на a9  - Uснс
    char uuso[4]; // напряжение rms  - Uскм
} skm8_packet;
//
typedef struct sens_str
{    // Структура данных сенсора
    unsigned char ind;          // Номер параметра
    unsigned char stat; // Код состояния параметра. 1 - неиспользуется, 2 - используется, 3 - обрыв датчика
    unsigned long dat;         // Данные.
} sens_str;
//
typedef struct buf_str
{ // Буфер данных устройства (TxMbuf)
    U16 ind; // текущий индекс в массиве данных. Указывает, куда записывать принимаемые символы
    U16 max; // Максимально допустимое значение длины данных. Настаивается для каждого устройства.
    U8 buf[192];     // массив данных.
    //U8 buf[420];     // массив данных.
} buf_str;
//*
typedef struct rxM_buf
{ // Буфер данных устройства (RxMbuf)
    U16 ind; // текущий индекс в массиве данных. Указывает, куда записывать принимаемые символы
    U16 max; // Максимально допустимое значение длины данных. Настаивается для каждого устройства.
    //U8 buf[274];     // массив данных.
    U8 buf[550];     // массив данных.
} rxM_buf;
typedef struct ads
{     // Буфер данных устройства
    U16 ind; // текущий индекс в массиве данных. Указывает, куда записывать принимаемые символы
    U32 max; // Максимально допустимое значение длины данных. Настаивается для каждого устройства.
    U8 buf[3];       // массив данных.
} ads;
// */
typedef struct remote_data
{ // Буфер данных устройства
    struct
    {    // Буфер данных устройства
        U8 year;
        U8 month;
        U8 day;
        U8 hour;
        U8 min;
    } date;
} remote_data;

typedef struct remote_command
{  // структура удаленной команды
    U8 port;          // номер последовательного порта. Диапазон: 0 - F;
    U8 cnt; // длина команды. Для двоичных данных - байт=два символа, для ascii - байт=один символ;
    U8 cmd[50];      // команда для передачи.
} remote_command;

#define DRIVE       GPIO_PIN_5 //14 Выходной сигнал Микролан
#define SENSE       GPIO_PIN_6 //15 вход данных Микролан

/******************************************************************************
 Файл Modbus.h - Описывает структуру данных протокола MODBUS
 (в режимах RTU и ASCII)
 для управления внешними устройствами.
 ###############################################################################
 Раздел Определений для команд MODBUS
 ###############################################################################
 */
#define READ_COIL_STATUS	(0x01)	    // Код в RTU.   Получение текущего состояния (ON/OFF)
#define A_READ_COIL_STATUS	(0x3031)    // Код в ASCII. группы логических ячеек.

#define READ_INPUT_STATUS	(0x02)	    // Код в RTU.   Получение текущего состояния (ON/OFF)
#define A_READ_INPUT_STATUS	(0x3032)    // Код в ASCII. группы дискретных входов.

#define READ_HOLDING_REG 	(0x03)      // Код в RTU.   Получение текущего значения одного
#define A_READ_HOLDING_REG 	(0x3033)	// Код в ASCII. или нескольких регистров хранения.

#define READ_INPUT_REG		(0x04)	    // Код в RTU.   Получение текущего значения одного или
#define A_READ_INPUT_REG	(0x3034)	// Код в ASCII. нескольких входных регистров.

#define WRITE_SINGLE_COIL	(0x05)	    // Код в RTU.   Изменение логической ячейки в состояние
#define A_WRITE_SINGLE_COIL	(0x3035)	// Код в ASCII. ON или OFF.

#define WRITE_SINGLE_REG	(0x06)	    // Код в RTU.   Запись нового значения в регистр
#define A_WRITE_SINGLE_REG	(0x3036)	// Код в ASCII. хранения.

#define READ_EXCEP_STATUS	(0x07)	    // Код в RTU.   Получение состояния (ON/OFF) восьми
#define A_READ_EXCEP_STATUS	(0x3037)    // Код в ASCII. внутренних логических ячеек, чье
//              назначение зависит от типа контроллера.
//              Пользователь может использовать эти
//              ячейки по своему выбору.

#define LOOPBACK_DIAGN_TEST	  (0x08)    // Код в RTU.   Тестовое сообщение, посылаемое SL(Ведомым)
#define A_LOOPBACK_DIAGN_TEST (0x3038)  // Код в ASCII. для получения данных о связи.

#define FETCH_EVENT_CNT_COM	  (0x0B)    // Код в RTU.   Позволяет MS(Мастеру) путем
#define A_FETCH_EVENT_CNT_COM (0x3042)  // Код в ASCII. последовательной посылки одного сообщения
//              определить выполнение операции.

#define FETCH_COM_EVENT_LOG	  (0x0C)    // Код в RTU.   Позволяет MS(Мастеру) получить журнал
#define A_FETCH_COM_EVENT_LOG (0x3043)  // Код в ASCII. связи, который содержит информацию
//              о каждой Modbus транзакции данного SL(Ведомого).
//              Если транзакция не выполнена, в журнал
//              заносится информация об ошибки.

#define PROGRAM				(0x0D)	    // Код в RTU.   Позволяет MS(Мастеру) программировать
#define A_PROGRAM			(0x3044)	// Код в ASCII. SL(Ведомый).

#define POLL_PROGR_COMPLETE	  (0x0E)    // Код в RTU.   Позволяет MS(Мастеру) связываться с другими
#define A_POLL_PROGR_COMPLETE (0x3045)  // Код в ASCII. SL(Ведомыми), если один SL выполняет
//              долговременную операцию программирования.
//              SL периодически опрашивается на момент
//              завершения программирования. Данный
//              запрос посылается только в том случае,
//              если предварительно был послан запрос
//              PROGRAM.

#define FORCE_MULT_COILS	(0x0F)	    // Код в RTU.   Изменить состояние (ON/OFF) нескольких
#define A_FORCE_MULT_COILS	(0x3046)	// Код в ASCII. последовательных логических ячеек.

#define FORCE_MULT_REG		(0x10)	    // Код в RTU.   Установить новые значения нескольких
#define A_FORCE_MULT_REG	(0x3130)	// Код в ASCII. последовательных регистров.

#define REPORT_SLAVE_ID		(0x11)	    // Код в RTU.   Позволяет MS(Мастеру) определить тип адресуемого
#define A_REPORT_SLAVE_ID	(0x3131)	// Код в ASCII. SL(Ведомого) и его рабочее состояние.

#define RESET_COM_LINK		(0x13)	    // Код в RTU.   Сбрасывает SL(Ведомый) в известное состояние
#define A_RESET_COM_LINK	(0x3133)    // Код в ASCII. после неустранимой ошибки. Сбрасывает
//              счетчик принятых байт.
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 20-64 Зарезервировано под расширения Modbus

#define READ_DATA			(0x45)	    // Код в RTU.   Получение текущего значения одного или
#define A_READ_DATA			(0x3435)    // Код в ASCII. нескольких входных регистров со

#define A_WR_CONFIG			(0x3438)    // Код в ASCII. '48'-Запись конфигурации ЩИТА
#define A_RD_CONFIG			(0x3441)    // Код в ASCII. '4A'-Чтение конфигурации ЩИТА

#define READ_REAL_DATA	    (0x4C)      // Код в RTU.   нескольких входных регистров со
#define A_READ_REAL_DATA	(0x3443)    // Код в ASCII. нескольких входных регистров со
//              щита управления котельной ЩСМ.

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 65-72 Зарезервировано под пользовательские функции.
//		 В дальнейшем не будет использоваться в продуктах Modicon.
#define CHECK_BC_KEY		(0x42)	    // Код в RTU.   (066) – Проверить аппаратный ключ
#define CHECK_USD_STATE		(0x43)	    // Код в RTU.   (067) – Проверить состояние УСД
#define SEND_REMOTE_REQUEST	(0x44)	    // Код в RTU.   (068) – Отправить запрос удаленному объекту
#define A_REMOTE_MODEM_SERVICE	(0x3436)// Код в ASCII. (070) – Обслуживание удаленного модема

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Команды для Теплосчетчика SKU-01,SKM-01
#define TCh_RealPR        (0x20)      // «Считывание текущих параметров прибора»
#define TCh_FullHP        (0x21)      // «Считывание среднечасовых »
#define TCh_FullDP        (0x22)      // «Считывание среднесуточных »
#define TCh_FullMP        (0x23)      // «Считывание среднемесячных »
#define TCh_AllStopStat   (0x24)      // «Считывание остановок работы прибора»

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Команды для счетчика Энергия-9
#define EN9_CONNECT       (0x03)      // Команда «Открыть доступ к счетчику»
#define EN9_CLOSE         (0x05)      // Команда «Закрыть доступ к счетчику с подтверждением»
#define EN9_GETOUT        (0x02)      // Команда «Закрыть доступ к счетчику без подтверждением»
#define EN9_GETTIME       (0x07)      // Команда «Чтение времени и даты из счетчика»
#define EN9_GETALL        (0x0D)      // Команда «Чтение мощности, напряжения и тока»

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Команды для Корректора газа СПГ-741
#define SPG_INIT          (0x3F)      // Команда «Открыть доступ к счетчику»
#define SPG_FLASH         (0x45)      // Команда 2.4.2 Чтение FLASH памяти
#define SPG_FLASH_1       (0x05)      // Условный код для выбора адреса 0000 Чтение FLASH памяти
#define SPG_FLASH_2       (0x06)      // Условный код для выбора адреса 0084 Чтение FLASH памяти
#define SPG_FLASH_3       (0x07)      // Условный код для выбора адреса 0008 Чтение FLASH памяти
#define SPG_RAM_1         (8)         // Условный код для выбора адреса - Чтение ОЗУ
#define SPG_RAM_2         (9)         // Условный код для выбора адреса - Чтение ОЗУ
#define SPG_RAM_3         (10)        // Условный код для выбора адреса - Чтение ОЗУ
#define SPG_RAM_4         (11)        // Условный код для выбора адреса - Чтение ОЗУ
#define SPG_RAM_5         (12)        // Условный код для выбора адреса - Чтение ОЗУ
#define SPG_RAM           (0x52)      // Команда 2.4.3 Чтение ОЗУ
#define SPG_HOUR          (0x48)      // Команда «Поиск записи в часовом архиве»
#define SPG_DAY           (0x59)      // Команда «Поиск записи в суточном архиве»
#define SPG_DEC           (0x41)      // Команда «Поиск записи в декадном архиве»
#define SPG_MONTH         (0x4D)      // Команда «Поиск записи в месячном архиве»
#define SPG_DB            (0x44)      // Команда «Ввод параметра в БД»
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Команды для Корректора газа КПЛГ
#define KPLG_DATE         (0x01)      // Команда "Чтение текущей даты"
#define KPLG_ARX0         (0x02)      // Команда "Чтение первой записи в часовом архиве"
#define KPLG_ARX1         (0x03)      // Команда "Выборочное чтение в часовом архиве"
#define KPLG_ARX2         (0x04)      // Команда "Чтение первой записи в СУТОЧНОМ архиве"
#define KPLG_ARX3         (0x05)      // Команда "Выборочное чтение в СУТОЧНОМ архиве"
#define KPLG_ARX4         (0x06)      // Команда "Чтение первой записи в МЕСЯЧНОМ архиве"
//#define KPLG_ARX5         (0x07)      // Команда "Выборочное чтение в МЕСЯЧНОМ архиве"
#define KPLG_VOL          (0x3F)      // Команда «»
#define KPLG_TEK          (0x37)      // Команда "Чтение текущих данных"
#define KPLG_HOUR         (0x38)      // Команда «Поиск записи в часовом архиве»
#define KPLG_DAY          (0x39)      // Команда «Поиск записи в суточном архиве»
#define KPLG_MONTH        (0x42)      // Команда «Поиск записи в месячном архиве»
#define KPLG_SETING       (0x40)      // Команда «Чтение настроек КПЛГ»
#define KPLG_NESH         (0x22)      // Команда «Чтение в архиве НЕШТАТНЫХ СИТУАЦИЙ»
#define KPLG_ARX6         (0x08)      // Команда "Выборочное чтение в архиве НЕШТАТНЫХ СИТУАЦИЙ"
#define KPLG_ARX7         (0x09)      // Команда "Чтение ПОСЛЕДНЕЙ ЗАПИСИ В МЕСЯЧНОМ АРХИВЕ"
#define KPLG_ARX8         (0x23)      // Команда "Чтение ПОСЛЕДНЕЙ ЗАПИСИ В АРХИВЕ ИЗМЕНЕНИЙ"
#define KPLG_VTR          (0x46)      // Команда "Поиск ЗАПИСИ В АРХИВЕ ИЗМЕНЕНИЙ"
#define KPLG_SETS         (0x50)      // Команда "ЧТЕНИЕ НАСТРОЕК КПЛГ"
#define KPLG_SET_1        (0x51)      // Команда "ЧТЕНИЕ НАСТРОЕК КПЛГ. БЛОК 1"
#define KPLG_SET_2        (0x52)      // Команда "ЧТЕНИЕ НАСТРОЕК КПЛГ. БЛОК 2"
#define KPLG_SET_3        (0x53)      // Команда "ЧТЕНИЕ НАСТРОЕК КПЛГ. БЛОК 3"
#define KPLG_SET_4        (0x54)      // Команда "ЧТЕНИЕ НАСТРОЕК КПЛГ. БЛОК 4"
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Команды прямого управления. Для всех видов устройств.
#define DIRECT            (1)         // Флаг "пришла прямая команда"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 73-119 ILLEGAL FUNCTION
// 120-127 Зарезервировано //Зарезервировано Modicon для внутреннего использования.
// 128-255 Зарезервировано //Зарезервировано для исключительных ситуаций.

//*****************************************************************/

//********************************************************************
// Прототипы функций
void init_vars(void);
void init_io(void);
void restart_io(void);
void close_io(void);
void init_display(void);
void upd_display(void);
void dhcp_check(void);

void LED_LIGHT(U32 pin, U32 val);
//void LED_TX_Out (U32 pin, U32 val);
//void LED_RX_Out (U32 pin, U32 val);
//void LED_STATUS_Out (U32 pin, U32 val);
void LCD_LIGHT_Out(U32 pin, U32 val);
void ALARM_Out(U32 pin, U32 val);
//void LED_RED_Out (U32 pin, U32 val);
void OUT_A(U32 pin, U32 val);
void DIR_485_Out(U32 pin, U32 val);
void MODEM_POWER(U32 pin, U32 val);
void RELE_CONTROL(U32 pin, U32 val);

//int fputc (int ch, FILE *f);
void UART1_PutString(const char *Str);
void UART1_PutStringLn(const char *Str);
void HTOA(unsigned char byData, unsigned char *buf);

void kbd_process(void);
//void Desk_Kbd_level_0(void);
void Desk_Kbd_level_2(void);
void Kbd_level_1(void);
void Kbd_level_A(void);
void Kbd_level_B(void);
void Kbd_level_C(void);
void Kbd_level_D(void);

void EditPropertyPSI(u_char index);
void EditPropertyMID(u_char index);
//*************************************************************************
u_char invASCII(u_char byData);
void handleDTR(u_char mode);
//*************************************************************************
void SetGPRSServer(void);
void RecieveDataCom1(void);	// Просто принимает данные с порта СОМ1 и отвечает на запрос
//*************************************************************************

char UCS2_ReadChar(unsigned char *c);         // Чтение символа в кодировке UCS2
void MessageForSMS(void);               // Формирует СМС-сообщение для отправки.
//
void InitPIC18Fxxxx(void);
void SendResultToLinkDevice(char unit);

//*************************************************************************
// Файл control.h
//
void Desk_Kbd_level_0(void);
void Desk_Kbd_level_2(void);
void Kbd_level_1(void);
void Kbd_level_A(void);
void Kbd_level_B(void);
void Kbd_level_C(void);
void Kbd_level_D(void);

//*************************************************************************
// Файл control.h
// Содержит прототипы процедур для управления объектами
void PrepareStringForLCD(char line);
void PrepareByteForLCD(u_char data, u_char *index);
void FlashStringForLCD(char line);
void Config_Load(void);               //char Config_Load(void);
void PrepareTemperatureForLCD(void);
void PreparePresureForLCD(void);
void PrepareGazForLCD(void);
void PrepareECLforLCD(void);
void IntToDec(unsigned int val, unsigned char *asc, unsigned char d);
void ScreenOne(void);
void ScreenMain(void);
void ScreenMenuGSM(void);
void ScreenMenuUSO(void);
void ScreenMenuGPRS(void);
void ScreenMenuSetup(void);
void Run_Cat_OnLCD(void);
void LookAtMeasureOnLCD(u_char index);
//
void ChangeIdle(void);
void ChangeIdleGPRS(void);
void ChangeIdle_T(void);
void Change_Operator(void); // Утилита изменения языка передачи СМС
void Edit_Unit(void);

char Insert_MID(u_char index);
void EditPropertyPSI(u_char index);
void EditPropertyMID(u_char index);
void Edit_OwnAddress(void);
void Edit_TelSMS(void);
void Edit_PIN_CODE(void);
void Edit_CntCmd(void);
void GPRS_Enable(void); // Вкл.-Откл. режим GPRS
//
char EEP_Write(unsigned int addr, unsigned char d);
char EEP_WriteBytes(unsigned int addr, char *d, int len);// Write 'len' sized data into appropriate address(addr)
char EEP_Read(unsigned int addr, char *d);// Read from AT24C02 of appropriate address(addr) and Return into 'd' value.
char EEP_ReadBytes(unsigned int addr, char *d, int len);// Read 'len' sized data from appropriate address(addr), write into starting address of data string and return 

char ParseGPRS_Command(u_char request, u_int typunit);
void Mbus_ASCII_SendCommand(u_char shsm, u_int func, u_char direct);
//void Mbus_RTU_SendCommand(u_char shsm,u_int func, u_char direct, u_int start, u_char len_com);
char Mbus_RTU_SendCommand(u_char shsm, u_int func, u_int start, u_char len_com);
void SendCommand_RTU(u_char shsm, u_int func, u_int addr, u_int len,
                     u_char direct);
u_char ParseRTU_Command(void);
unsigned short CRC16(unsigned char *puchMsg, unsigned short usDataLen);

void HTOA(u_char byData, u_char *buf);
void init_uart0(void);
void init_uart1(void);
void init_uart2(void);
void InitRS485(void); /* Initialization of Serial Port(Ex.Baud Rate setting) */
void PutByte485(u_char byData); /* Output 1 character through Serial Port */

void InitSerial(void); /* Initialization of Serial Port(Ex.Baud Rate setting) */
void PutByte(u_char byData); /* Output 1 character through Serial Port */

void UART0_PutString(const char *Str);
void UART1_PutString(const char *Str);
void UART2_PutString(const char *Str);
void UART0_PutStringLn(const char *Str);
void UART1_PutStringLn(const char *Str);
void UART2_PutStringLn(const char *Str);
void PutString(const char *Str); /* Output to Serial. */
void PutStringLn(const char *Str); /* Output to Serial and then specific character,'Carrage Return & New Line'. */

//*****************************************************************/
u_int WriteBuf(u_char *TX_Buf, const char *Stream);
char ASCII_Dec_ToDig_INT_INV(u_int ADDR, u_char *addr, u_char mode);
char ASCII_Dec_ToDig_Hex_INV(u_int ADDR, u_char *addr, u_char mode);
char ASCII_Dec_Hex_INV(u_int ADDR, u_char *addr);
char HexASCII_ToDecimalASCII(char *addr_in, char *addr_out);
void HexASCII_ToDigital(u_char *addr_in, u_char *addr_out);
void Hex_ToDecimalASCII(char *addr_in, char *addr_out);
void Hex_ToDecimalASCII_2(char input, char *addr_out);
//char ASCII_Direct(u_char* addr,u_int eeprom,u_char quant);
char ASCII_Direct(u_char *addr, u_int eeprom, u_char quant, u_char hex);
char ascii_Direct(u_char *addr, u_int eeprom, u_char quant, u_char lf_cr);
//char ASCII_DecimalToHex(u_char ADDR, u_char* addr);
char ASCII_DecimalToHex(u_char address, u_char *addr, u_char index);
char ascii_Dec_Hex(u_int ADDR, u_char *addr);

u_int ATOI(u_char *str, char base);
u_int Atoi(u_char *str, char base);
u_char AtoH(u_char *str, char base);
u_char ATOH(unsigned char *buf);

//void  ITOA(u_int byData,u_char *buf);
void ItoDecA(unsigned int Dec, unsigned char *str);
void ItoDecAShot_bn(unsigned int Dec, unsigned char *str);
void ItoDecAShot_bn2(unsigned int Dec, unsigned char *str);
void ItoDecAShot_pr(unsigned int Dec, unsigned char *str);
void ItoDecAShot_1dot(unsigned int Dec, char *str);
void ItoDec2AShot_1dot(unsigned int Dec, unsigned char *str);
void ItoDecAShot(unsigned int Dec, unsigned char *str);
void ItoDecA_4_Shot_zero(unsigned int Dec, unsigned char *str);
void ItoDecAShot_2dot(unsigned int Dec, char *str);

char ItoDecAShot_znach(unsigned int Dec, char *str);
//void ItoDec_8(unsigned long Dec, char *str);
//void ItoDecAShot_zero(unsigned int Dec, unsigned char *str);//void  ItoDecAShot_zero(unsigned int Dec, unsigned char* str);
void ItoDecACHAR(unsigned char Dec, unsigned char *str);
void ItoDecACHAR_zero(unsigned char Dec, unsigned char *str);
void ItoDecACHAR_sh(unsigned char Dec, unsigned char *str);

unsigned long ATOL(unsigned char *str, char base);
unsigned long Atol(unsigned char *str, char base);
void LTOA(unsigned long byData, u_char *buf);
void LtoDecA(unsigned long Dec, unsigned char *str);
void LtoDecA_sh(unsigned long Dec, unsigned char *str);
void IPdecToHex(unsigned char* str,unsigned char* out);

u_char MakeLRC(u_char *buffer, u_char len);
u_char C2D(u_char c);
u_char D2C(char c);
//int cmp_str(const char * s, unsigned char *buf );
unsigned int cmp_str(const char *string, unsigned char *buf);
int cmp_str_rx(const char *s, unsigned char *buf);
int cmp_str_short(const char *s, unsigned char *buf);

void ShowTableUnit(void);
void ShowTable_Meter(void);
void ShowTable_ID(void);
void SetMarkerPosition(void);
void UpdateMarkerPosition(void);

void detect_error_psi(unsigned char unit);

void PrepareEnergy9forLCD(void);
void PrepareSKUforLCD(void);
void PrepareSPGforLCD(char LWork);	//void PrepareSPGforLCD(void);
void PrepareKPLGforLCD(char LWork);	//void PrepareKPLGforLCD(void);

unsigned char CRC8(unsigned char *buf, unsigned char size);
void CodePacket(unsigned char *buf, unsigned char *coder);
unsigned short CompressPacket(unsigned char *buf, unsigned char *new_buf,
                              unsigned short length);
void DecompressPacket(unsigned char *buf, unsigned char *new_buf);
void Mbus_Energy9(u_char shsm, u_int func, u_char direct);
void TCh_SKU01(u_char shsm, u_int func, u_char direct);
char SPG741(u_char idx, u_int func, u_char direct);
char KPLG(u_char shsm, u_int func, u_char direct);

void httpreq(void);
void httpreq_smart(void);
void httpreq_security(void);

void TickGetUSO(void);

char PrepareCommandForModbus(char unit, char cur_cikl);
char ParseRepliesDevices(char unit);
//void Send_from_UART0 (unsigned char* buffer);
void Send_from_UART0(char *buffer);
//void Send_from_UART0_bin_code (unsigned char *buffer, unsigned int len);

void Send_to_modem(int len);
void Send_from_UART1(char *buffer);
void Send_from_UART2(int len);
char parse_csq_from_rx(unsigned int pos);

void IdleAndProgressLine(char line, unsigned int stay);
void IdleAndProgressLineCMP(const char *string, char line, unsigned int stay);
unsigned int wait_compl(const char *string, unsigned char *buf,
                        unsigned long time);
void delay_ms(unsigned int ms);

u_int WriteBuf(u_char *buf, const char *stream);
u_int WriteBufMultySimb(u_char *buf, const char *data, char dlina);
u_int WriteBufMultySimbASCII(unsigned char *buf, unsigned char *data,
                             char dlina);
u_int WriteBufMultySimb_znach(u_char *buf, const char *data, char dlina);

char Hibernation_Wake(void);
void Hibernation_Start(void);

char parse_remote_command(void);
unsigned char ota_get_status(void);
unsigned long ota_get_received(void);
//************************************************************************                   
// Прототипы функций для LCD
//************************************************************************                   
//
void lcd_cmd(unsigned char);
//char lcd_cmd_safe(unsigned char cmd);
void lcd_power_off(void);
void lcd_data(unsigned char);
void lcd_puts(const char *s);
void lcd_init(unsigned char);
void lcd_HTOA(unsigned char byData);
void lcd_ITOA(unsigned int byData);
void lcd_LTOA(unsigned long byData);
void LCDprString(unsigned char row, unsigned char col, const char *string);
void LCDprfull(void);
void LCDcls(void);

void conv_float(void);
void save_rom(void);
void init_timer0(void);
//************************************************************************                   
void input_8_digit(char *fourdigit);
void input_8_dig(unsigned long cnt);
void input_4_digit(char *fourdigit);
void input_2_digit(unsigned int fourdigit);
void input_characters(unsigned int num_chars);
void input_type(void);
void input_MicroLAN(void);

void directremout(void);

void deep_sleep_start(void);
//
void HibernateHandler(void);
void PortA_IntHandler(void);
//void uart0_isr (void); //__irq 

/*----------------------- MODEM_POWER_ON-----------------------------*/
char init_sim900(void);
char send_packet(void);
char test_packet(void); // для налаштування передачі пакетів даних
void rePWR_sim900(void);

#define  MODEM_POWER_ON()  P4OUT =  BIT4;
#define  MODEM_POWER_OFF() P4OUT = ~BIT4;

void init_periphery(void);
void init_measure(void);
void make_measure(void);
void average_measure(void);
void LtoChars(unsigned long LongData, unsigned char *buff);
void ITOA(unsigned int intData, unsigned char *buff);
void get_temperatura(void);
void LtoDecA_couter(unsigned long Dec, unsigned char *str);
void calibr_seti(void);            // Калибр.датчика сети
calibr calibrovka(calibr koeff);     // Процедура Калибровки Датчиков и значений
unsigned long AtoLong(unsigned char *str);
void ItoDec_12(unsigned long Dec, char *str);

//void init_periphery(void);
//void init_measure(void);
//void make_measure(void);
void modern_measure(void);
//void average_measure(void);
void make_measure_bat(void);
//void LtoChars(unsigned long LongData, unsigned char *buff);
//void ITOA(unsigned int intData, unsigned char *buff);
//void get_temperatura(void);

//******************************************************************************
// General I2C State Machine ***************************************************
//******************************************************************************

typedef enum I2C_ModeEnum
{
    IDLE_MODE,
    NACK_MODE,
    TX_REG_ADDRESS_MODE,
    RX_REG_ADDRESS_MODE,
    TX_DATA_MODE,
    RX_DATA_MODE,
    SWITCH_TO_RX_MODE,
    SWITHC_TO_TX_MODE,
    TIMEOUT_MODE
} I2C_Mode;

/* Функции записи и чтения I2C */

/* Для ведомого устройства с dev_addr записывает данные, указанные в *reg_data
 *
 * dev_addr: Адрес ведомого устройства.
 *           Example: ADDR_ADS7128
 * reg_addr: Регистр или команда для отправки ведомому.
 *           Example: CMD_TYPE_0_MASTER
 * *reg_data: Буфер для записи
 *           Example: MasterType0
 * count: Длина *reg_data
 *           Example: TYPE_0_LENGTH
 *  */
I2C_Mode I2C_Master_WriteReg(uint8_t dev_addr, uint8_t reg_addr,
                             uint8_t *reg_data, uint8_t count);

/* Для ведомого устройства с dev_addr считайте данные, указанные в регистре reg_addr на ведомом устройстве.
 * Полученные данные доступны в ReceiveBuffer
 *
 * dev_addr: Адрес ведомого устройства
 *           Example: ADDR_ADS7128
 * reg_addr: Регистр или команда для отправки ведомому.
 *           Example: CMD_TYPE_0_SLAVE
 * count: Длина данных для чтения
 *           Example: TYPE_0_LENGTH
 **/
I2C_Mode I2C_Master_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t count);
void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count);
void initGPIO();
void initClockTo16MHz();
void initI2C();

unsigned int measure_RMS();
calibr calibrovka(calibr koeff);                          // Калибр.датчика сети
void LtoDecA_couter(unsigned long Dec, unsigned char *str);
void ItoDec_12(unsigned long Dec, char *str);
void read_Status_LCD(void);
void ItoDecAShot4(unsigned int Dec, unsigned char *str);
void aes256(unsigned char *rawdata, U8 *outdata, unsigned int index_outdata);
uint8_t hex_to_bytes( U8 *hex, uint8_t *out, uint16_t out_max);
uint8_t hex_nibble(char c);




