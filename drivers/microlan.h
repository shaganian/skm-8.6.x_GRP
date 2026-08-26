/////////////////////////////////////////////////////////////////////////
// Имя файла:        system.h
//
// В этом файле собраны ссылки на все заголовочные файлы и содержится
// общая информация (настройки) для всех модулей.
//
/////////////////////////////////////////////////////////////////////////
// Автор                Дата       Примечание
//-----------------------------------------------------------------------
// Сергей Шаганян       10/08/12   Настройки для семейства Stellaris
/////////////////////////////////////////////////////////////////////////
// Интерфейс Микролан:
//#define SENSE       _RB10       // Вход  "Данные с МикроЛан"
//#define DRIVE       _LATB11     // Выход "Передача данных МикроЛан".
//#define DPU         _LATC6      // Выход "Динамическая подтяжка МикроЛан".
/////////////////////////////////////////////////////////////////////////
// Определения логических состояний:
#define FALSE	0
#define TRUE	1
//------ port С -----------------------------------------------------------
//#define DPU         GPIO_PIN_0 //66 Динамическая поддтяжка Микролан
//#define DRIVE       GPIO_PIN_1 //29 Выходной сигнал Микролан
//#define SENSE       GPIO_PIN_2 //30 вход данных Микролан
//------ port F -----------------------------------------------------------
//#define ---         GPIO_PIN_0 //47 ---
//#define LED1        GPIO_PIN_2 //60  ---
//#define LED2        GPIO_PIN_3 //59  ---
/////////////////////////////////////////////////////////////////////////
// Прототипы функций Modbus:
unsigned char MakeLRC(unsigned char *buffer, unsigned char len);
unsigned char ParseASCII_CommandFromRS485(void);
unsigned char d2c(char c);
//unsigned char C2D(unsigned char d);
//*****************************************************************/
// Прототипы функций MicroLAN
//*****************************************************************/
char init_devices(void);
char i_init_devices(void);
//void write_byte(unsigned char command);
//void i_write_byte(unsigned char command);
void write_0(void);
void i_write_0(void);
void write_1(void);
void i_write_1(void);
unsigned int read_byte(void);
unsigned int i_read_byte(void);
unsigned int read_bit(void);
unsigned int i_read_bit(void);
int calc_crc(int *buff, int num_vals);
void Loop_DS1820(void);
char Loop_DS1820_M(unsigned char mode);
void Loop_CTU_DAL_RFID(void);

char OWFirst(void);
char OWNext(void);
char OWSearch(void);
char OWVerify(void);
void OWTargetSetup(unsigned char family_code);
void OWFamilySkipSetup(void);
char OWReset(void);
char i_OWReset(void);
void OWWriteByte(unsigned char byte_value);
void OWWriteBit(unsigned char bit_value);
unsigned char OWReadByte(void);
unsigned char OWReadBit(void);
unsigned char OWReadBit80(void);
unsigned char docrc8(unsigned char value);
void MLanOutDS1820(void);
void MLanPrepareDS1820(void);
void OWWriteDS18B20(unsigned char number);
unsigned char Command_DS2408(unsigned char s, unsigned char flag);
signed char seach_DS1820(void);
void seach_DS2438(void);
void seach_DS2450(void);
void seach_DS2408(void);
signed char install_sensor_DS1820(void);
char MicroLanNetwork(void);

unsigned char ds_OWReset(void);
unsigned char ds_OWreadPoint(void);
unsigned char ds_OWwrConfig(unsigned char configure);
unsigned char ds_OWsetRDpoint(unsigned char point);
unsigned char ds_OWwrByte(unsigned char data);
unsigned char ds_OWReadByte(void);
unsigned char ds_OWsetTriplet(unsigned char dir);
unsigned char ds_OWSearch(unsigned char resetSearch, unsigned char *lastDevice,
                          unsigned char *deviceAddress);
unsigned char ds_owTriplet(unsigned char *dir, unsigned char *firstBit,
                           unsigned char *secondBit);

unsigned char i_ReadI2C(void);
unsigned char i_getsI2C(unsigned char *rdptr, unsigned char length);
unsigned char i_WriteI2C(unsigned char data_out);
unsigned char i_ds_OWreadPoint(void);
unsigned char i_ds_OWwrByte(unsigned char data);
unsigned char i_ds_OWReset(void);
unsigned char i_ds_OWReadByte(void);
unsigned char i_ds_OWwrAddress(unsigned char *point_addr);
unsigned char i_ds_OWsetRDpoint(unsigned char point);
unsigned char ds_OWRead_com96(void);

void search_ds(void);
void init_ds(void);
void set_ds(unsigned char index, unsigned char a, unsigned char b,
            unsigned char c, unsigned char d);
void setup_sensor_ds(unsigned char num);
void param_DS2450(unsigned char num);
void param_DS1820(unsigned char num);
void param_DS2408(unsigned int addr);
void param_DS2438(unsigned char num);
void Loop_DS2450(unsigned char mode);
void init_DS2450(void);
void Loop_DS2408(unsigned char mode);
void init_DS2408(void);
void Pr_ADC(void);
unsigned char Input_2408(unsigned char addr, unsigned char len);
unsigned char Output_2408(unsigned char addr, unsigned char data);
void Prepare_DS2450_ForLCD(void);
void Edit_Addr_Indikatora(unsigned int index);
void Mbus_send2450_toIndicator(void);
void Test_Mbus_toIndicator(void);
unsigned short ML_CRC16(unsigned char *puchMsg, unsigned short usDataLen);
void Edit_Kalibrovka(unsigned int index);

/////////////////////////////////////////////////////////////////////////
// EOF - Конец файла
/////////////////////////////////////////////////////////////////////////
