/* Графический модуль для работы с графическими индикаторами на драйвере UC1601S. */
#define LINE1		0
#define LINE2		1
#define LINE3		2
#define LINE4		3
#define LINE5		4
#define LINE6		5
#define LINE7		6
#define LINE8		7

// для работы с I2C
void i2c_open(void); // настройка MSSP модуля
void i2c_idle(void); // провекра на готовность I2C к работе
void i2c_stop(void); // формирование стоп
char i2c_start(char address, char C_D, char R_W); // адрес устройства и управление младшими битами
char i2c_restart(char address, char C_D, char R_W);
char i2c_write(unsigned char data); //запись байта

void i2c_writeChar(unsigned char c);        //запись символа
void i2c_writeChar_in(unsigned char c);	//	; формирования записи байта в устройство
//void i2c_writeChar_in_inv(unsigned char c);	//	; формирования записи байта в устройство
void i2c_writeChar_inv(unsigned char c); //  ; формирования записи байта в устройство
void i2c_PutStr(const char *data);
void i2c_PutStr_inv(const char *data);
void i2c_PutStrFullScreen(const char *data);
void i2c_PutfullStr(const char *data);
void i2c_SetAddress(unsigned char X, char Y);
void i2c_SetAddressFullScreen(signed char X, char Y);
void i2c_PutMultySimb(const char *data, char len);
void i2c_PutMultySimb_znach(const char *data, char len);

char i2c_read_ack(void);	//чтение с подтвеждением
char i2c_read_noack(void);	//чтение без подтеждения

// для работы с индикатором
void init_LCD(void); // инициализация

void clear_LCD(char tip); // очиска всего дисплея
// установка курсора
void curcorG_LCD(unsigned char X, char Y);
// вывод строк
void String_LCD(const char *str, char inv, char width, char height,
                signed char X, char Y); // , char строка, ширина, высота, центрирование (22 символа в строке)
void Stringp_LCD(const char *str, char inv, char width, char height);
// вывод символа или числа
void symbol_LCD(unsigned char cod, char tip, char inv, char shi, char vis);
// бегущая строка
void ticker_LCD(const char *str, char start, char ends, char Y); //с ПЗУ
void tickerB_LCD(char dlinok, char start, char Y); // с озу

// для работы с индикатором графические примитивы
void point(int tip, int X, int Y);
void line(int tip, int x0, int y0, int x1, int y1);
void rectangle(int ugl, int tip, int zal, int tipzal, int x0, int y0, int x1,
               int y1);
void strip(int ugl, int tip, int tipzal, int x0, int y0, int sh, int vs,
           char vol);

// декларация массива
//char ca[];
