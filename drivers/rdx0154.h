/* ����������� ������ ��� ������ � ������������ ������������ �� �������� UC1601S. */
#define LINE1		0
#define LINE2		1
#define LINE3		2
#define LINE4		3
#define LINE5		4
#define LINE6		5
#define LINE7		6
#define LINE8		7

// ��� ������ � I2C
void i2c_open(void); // ��������� MSSP ������
void i2c_idle(void); // �������� �� ���������� I2C � ������
void i2c_stop(void); // ������������ ����
char i2c_start(char address, char C_D, char R_W); // ����� ���������� � ���������� �������� ������
char i2c_restart(char address, char C_D, char R_W);
char i2c_write(unsigned char data); //������ �����

void i2c_writeChar(unsigned char c);        //������ �������
void i2c_writeChar_in(unsigned char c);	//	; ������������ ������ ����� � ����������
//void i2c_writeChar_in_inv(unsigned char c);	//	; ������������ ������ ����� � ����������
void i2c_writeChar_inv(unsigned char c); //  ; ������������ ������ ����� � ����������
void i2c_PutStr(const char *data);
void i2c_PutStr_inv(const char *data);
void i2c_PutStrFullScreen(const char *data);
void i2c_PutfullStr(const char *data);
void i2c_SetAddress(unsigned char X, char Y);
void i2c_SetAddressFullScreen(signed char X, char Y);
void i2c_PutMultySimb(const char *data, char len);
void i2c_PutMultySimb_znach(const char *data, char len);

char i2c_read_ack(void);	//������ � �������������
char i2c_read_noack(void);	//������ ��� �����������

// ��� ������ � �����������
void init_LCD(void); // �������������

void clear_LCD(char tip); // ������ ����� �������
// ��������� �������
void curcorG_LCD(unsigned char X, char Y);
// ����� �����
void String_LCD(const char *str, char inv, char width, char height,
                signed char X, char Y); // , char ������, ������, ������, ������������� (22 ������� � ������)
void Stringp_LCD(const char *str, char inv, char width, char height);
// ����� ������� ��� �����
void symbol_LCD(unsigned char cod, char tip, char inv, char shi, char vis);
// ������� ������
void ticker_LCD(const char *str, char start, char ends, char Y); //� ���
void tickerB_LCD(char dlinok, char start, char Y); // � ���

// ��� ������ � ����������� ����������� ���������
void point(int tip, int X, int Y);
void line(int tip, int x0, int y0, int x1, int y1);
void rectangle(int ugl, int tip, int zal, int tipzal, int x0, int y0, int x1,
               int y1);
void strip(int ugl, int tip, int tipzal, int x0, int y0, int sh, int vs,
           char vol);

// ���������� �������
//char ca[];

void lcd_power_off(void);
void i2c_PutStrUtf8(const char *data);
void i2c_PutStrUtf8_inv(const char *data);
