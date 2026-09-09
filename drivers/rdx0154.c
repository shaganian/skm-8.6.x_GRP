//********************************************************************
//  rdx0154.c
//  функции для работы с дисплеем rdx0154
//
//--------------------------------------------------------------------
//
//#include "driverlib/MSP430FR5xx_6xx/driverlib.h"
#include <driverlib.h>
#include "system.h"
#include "rdx0154.h"        // Утилиты обслуживания LCD и клавиатуры
#include "font5x7.h"
#include "Font5x7_ru.h"

#define SLAVE_ADDRESS 0x38

//extern unsigned char nokeypad;          // наличие клавиатуры: 0-клава есть, 1-нет.
extern unsigned char display;        // наличие дисплея:     1-дисплей есть, 0-нет.
extern unsigned char indi;                          // разрешить индикацию
extern struct rxM_buf RxMbuf;                       // буфер модема

//*****************************************************************************************
// настройка модуля I2C в режиме мастер
void i2c_open (void)
{
    // Configure Pins for I2C
    //Set P1.6 and P1.7 as Secondary Module Function Input.
    /*
     * Select Port 1
     * Set Pin 6, 7 to input Secondary Module Function, (UCB0SIMO/UCB0SDA, UCB0SOMI/UCB0SCL).
     */
    GPIO_setAsPeripheralModuleFunctionInputPin(
        GPIO_PORT_P1,
        GPIO_PIN6 + GPIO_PIN7,
        GPIO_SECONDARY_MODULE_FUNCTION
        );

    /*
     * Disable the GPIO power-on default high-impedance mode to activate
     * previously configured port settings
     */
    PMM_unlockLPM5();

    //
    // Инициализируем модуль как Мастер
    //
    EUSCI_B_I2C_initMasterParam param = {0};
    param.selectClockSource = EUSCI_B_I2C_CLOCKSOURCE_SMCLK;
    param.i2cClk = CS_getSMCLK();
    param.dataRate = EUSCI_B_I2C_SET_DATA_RATE_100KBPS;
    //param.dataRate = EUSCI_B_I2C_SET_DATA_RATE_400KBPS;
//    param.byteCounterThreshold = 1;
    param.autoSTOPGeneration = EUSCI_B_I2C_NO_AUTO_STOP;
    //
    // Инициализируем I2C как мастер
    //
    EUSCI_B_I2C_initMaster(EUSCI_B0_BASE, &param);
    //
}//

//*****************************************************************************************
void read_Status_LCD (void)
{
unsigned int timer;
    //
    // Устанавливаем адрес ведомого устройства
    //
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS);
    //
    // Устанавливаем режим передачи. Set in transmit mode
    //
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);
    //
    // Включаем модуль I2C. Enable I2C Module to start operations
    //
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);
    //
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
                timer++;
                if(timer>5000)break; // таймер прерывания цикла
    }
    //
    __no_operation();
    /*
    bool EUSCI B I2C masterSendSingleByteWithTimeout (uint16 t baseAddress, uint8 t txData, uint32 t timeout)

        Выполняет однобайтовую передачу от мастера к подчиненному с таймаутом.
        Эта функция используется модулем Master для отправки одного байта.
        Эта функция отправляет запуск, затем передает байт ведомому, а затем отправляет останов.
        Параметры baseAddress - это базовый адрес модуля I2C Master.
        txData - это байт данных, который должен быть передан.
        Тайм-аут - это время ожидания до отказа.

        Модифицированные биты регистра UCBxTXBUF, биты регистра UCBxCTLW0,
        биты регистра UCBxIE и биты регистра UCBxIFG.

        Возвращает STATUS SUCCESS или STATUS FAILURE процесса передачи.
     */

    //EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE,0xE2 ); // 0xE2 СИСТЕМНЫЙ СБРОС дисплея
    if(EUSCI_B_I2C_masterSendSingleByteWithTimeout (EUSCI_B0_BASE,0xAF,10000 )){
        display = 1;
        indi=20;                // * разрешить индикацию на 10 секунд
        P2IE  = BIT3 | BIT4;    // Разрешаем прерывание P2.3,4 от кнопок
    }
    else{
        display = 0;
        indi=0;                // * разрешить индикацию на 10 секунд
        P2IE  &= ~BIT3;         // ЗАПРЕЩАЕМ прерывание от P2.3
        P2IE  &= ~BIT4;         // ЗАПРЕЩАЕМ прерывание от P2.4

    }
            //EUSCI_B_I2C_masterSendSingleByteWithTimeout (EUSCI_B0_BASE,0xE2,10000 ); // 0xE2 СИСТЕМНЫЙ СБРОС дисплея
    __no_operation();
    RxMbuf.ind = 0;
    RxMbuf.max = 512;           //255;

    __no_operation();


}//

void lcd_power_off(void)
{
    unsigned int timer;

    if(!display) return;

    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS);
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);

    timer = 0;
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){
        timer++;
        if(timer > 5000) break;
    }

    EUSCI_B_I2C_masterSendSingleByteWithTimeout(
        EUSCI_B0_BASE,
        0xAE,
        10000);

    __delay_cycles(100000);

    EUSCI_B_I2C_masterSendSingleByteWithTimeout(
        EUSCI_B0_BASE,
        0xAE,
        10000);

    __delay_cycles(100000);

    indi = 0;
}

//*****************************************************************************************
//*
void clear_LCD (char tip)
{
int a;
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------

    //i2c_start(0x70,0,0);    // 0x70 адрес индикатора, команда, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS);         // Устан.Адрес дисплея и флаг "Запись команды"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }
    //i2c_write(0xb0);    //0b10110000 страница 0
    EUSCI_B_I2C_masterSendMultiByteStart (EUSCI_B0_BASE, 0xb0);
    //    i2c_write(0x00);    //0b00000000 колонка 0
    EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0x00);
    //    i2c_write(0x10);    //0b00010000
    EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0x10);
    //HWREG(I2C0_MASTER_BASE + I2C_O_MCS) = I2C_MASTER_CMD_BURST_SEND_STOP;  //i2c_stop();
    EUSCI_B_I2C_masterSendMultiByteStop (EUSCI_B0_BASE);


    //i2c_start(0x70,1,0);    // адрес индикатора, данные, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS | 0x01 );  // Устан.Адрес дисплея и флаг "Запись данных"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }

    if (tip==0){
        EUSCI_B_I2C_masterSendMultiByteStart (EUSCI_B0_BASE, 0x00);
        for (a=0;a<1056;a++){
            //i2c_write(0x00);//0b00000000
            EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0x00);
        }
        EUSCI_B_I2C_masterSendMultiByteStop (EUSCI_B0_BASE);
    }
    else if(tip==1){
        EUSCI_B_I2C_masterSendMultiByteStart (EUSCI_B0_BASE, 0xff);
        for (a=0;a<1056;a++){
            //i2c_write(0xff);//0b11111111
            EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0xff);
        }
        EUSCI_B_I2C_masterSendMultiByteStop (EUSCI_B0_BASE);
    }
    else{
        EUSCI_B_I2C_masterSendMultiByteStart (EUSCI_B0_BASE, 0xAA);
        for (a=0;a<528;a++)   {
            //i2c_write(0xAA);
            EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0xAA);
            //i2c_write(0x55);
            EUSCI_B_I2C_masterSendMultiByteNext (EUSCI_B0_BASE, 0x55);
        }
        EUSCI_B_I2C_masterSendMultiByteStop (EUSCI_B0_BASE);
    }
    //HWREG(I2C0_MASTER_BASE + I2C_O_MCS) = I2C_MASTER_CMD_BURST_SEND_STOP;  //i2c_stop();
}
// */
//*****************************************************************************************
void init_LCD (void)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //
    // Устанавливаем адрес ведомого устройства
    //
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS);
    //
    // Устанавливаем режим передачи. Set in transmit mode
    //
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);
    //
    // Включаем модуль I2C. Enable I2C Module to start operations
    //
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);
    //
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
                timer++;
                if(timer>5000)break; // таймер прерывания цикла
    }
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE,0xE2 ); // 0xE2 СИСТЕМНЫЙ СБРОС дисплея

    __delay_cycles(300000);  // ждем 3мс, пока разрядится конденсатор

    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS );
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
                timer++;
                if(timer>5000)break; // таймер прерывания цикла
    }
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE,0xEB );      // 0xEB   BIAS 6
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE,0x81 );      // настройка Vbias
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE, 124 );   // координата x
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE, 0xC6 );  // настройка типа разветки сверху в низ, и слева на право
    //EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE, 0xC0 );// настройка типа разветки снизу вверх, и с права-на лево
    //
    EUSCI_B_I2C_masterSendSingleByte(EUSCI_B0_BASE, 0xAF );    // Включаем дисплей
    //
    __delay_cycles(300000);  // ждем 3мс, пока разрядится конденсатор
}//

//*****************************************************************************************
void i2c_writeChar(unsigned char c)
{
    unsigned char i;
    unsigned char value;

    if(!display) return;

    EUSCI_B_I2C_setSlaveAddress(
        EUSCI_B0_BASE,
        SLAVE_ADDRESS | 0x01
    );

    EUSCI_B_I2C_setMode(
        EUSCI_B0_BASE,
        EUSCI_B_I2C_TRANSMIT_MODE
    );

    EUSCI_B_I2C_enable(EUSCI_B0_BASE);

    if(c < 0x80)
    {
        for(i = 0; i < 5; i++)
        {
            value = Font5x7[((c - 0x20) * 5) + i];

            EUSCI_B_I2C_masterSendSingleByteWithTimeout(
                EUSCI_B0_BASE,
                value,
                10000
            );
        }
    }
    else if(c >= 0xC0)
    {
        for(i = 0; i < 5; i++)
        {
            value = Font5x7_ru[((c - 0xC0) * 5) + i];

            EUSCI_B_I2C_masterSendSingleByteWithTimeout(
                EUSCI_B0_BASE,
                value,
                10000
            );
        }
    }

    /* колонка-пробіл між символами */
    EUSCI_B_I2C_masterSendSingleByteWithTimeout(
        EUSCI_B0_BASE,
        0x00,
        10000
    );
}

//*****************************************************************************************
void i2c_writeChar_inv(unsigned char c)
{
    unsigned char i;
    unsigned char value;

    if(!display) return;

    EUSCI_B_I2C_setSlaveAddress(
        EUSCI_B0_BASE,
        SLAVE_ADDRESS | 0x01
    );

    EUSCI_B_I2C_setMode(
        EUSCI_B0_BASE,
        EUSCI_B_I2C_TRANSMIT_MODE
    );

    EUSCI_B_I2C_enable(EUSCI_B0_BASE);

    if(c < 0x80)
    {
        for(i = 0; i < 5; i++)
        {
            value = (unsigned char)~Font5x7[((c - 0x20) * 5) + i];

            EUSCI_B_I2C_masterSendSingleByteWithTimeout(
                EUSCI_B0_BASE,
                value,
                10000
            );
        }
    }
    else if(c >= 0xC0)
    {
        for(i = 0; i < 5; i++)
        {
            value = (unsigned char)~Font5x7_ru[((c - 0xC0) * 5) + i];

            EUSCI_B_I2C_masterSendSingleByteWithTimeout(
                EUSCI_B0_BASE,
                value,
                10000
            );
        }
    }

    /* інверсний пробіл між символами */
    EUSCI_B_I2C_masterSendSingleByteWithTimeout(
        EUSCI_B0_BASE,
        0xFF,
        10000
    );
}

//*****************************************************************************/
void i2c_PutMultySimb(const char *data, char len)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //i2c_start(0x70,1,0);    // адрес индикатора, данные, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS | 0x01 );  // Устан.Адрес дисплея и флаг "Запись данных"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }


    while (len) {
        if(*data == 0) break;
        //i2c_writeChar_in(*data);
        i2c_writeChar(*data);
        len--;
        data++;
    }
}
//*****************************************************************************/
/* Выводит только значащие символы, ведущие нули опускаются.
*/
void i2c_PutMultySimb_znach(const char *data, char len)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //i2c_start(0x70,1,0);    // адрес индикатора, данные, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS | 0x01 );  // Устан.Адрес дисплея и флаг "Запись данных"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }

    while (len) {
        if(*data == 0) break;
        if(*data > '0') break;
        data++;
        len--;
    }
    while (len) {
        if(*data == 0) break;
        i2c_writeChar(*data);
        data++;
        len--;
    }
}

//*****************************************************************************/
void i2c_PutStr(const char *data)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //i2c_start(0x70,1,0);    // адрес индикатора, данные, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS | 0x01 );  // Устан.Адрес дисплея и флаг "Запись данных"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }


    while (*data) {
        if(*data == 0) break;
        //i2c_writeChar_in(*data);
        i2c_writeChar(*data);
        data++;
    }
}

static unsigned char utf8_to_lcd_char(const unsigned char **data)
{
    unsigned char c;
    unsigned char d;

    c = **data;
    (*data)++;

    if (c < 0x80) {
        return c;
    }

    d = **data;
    if (d != 0) {
        (*data)++;
    }

    if (c == 0xD0) {
        if ((d >= 0x90) && (d <= 0xBF)) {
            return d + 0x30;
        }
        if (d == 0x81) return 0xC5;
        if (d == 0x84) return 0xC5;
        if ((d == 0x86) || (d == 0x87)) return 'I';
    }
    else if (c == 0xD1) {
        if ((d >= 0x80) && (d <= 0x8F)) {
            return d + 0x70;
        }
        if (d == 0x91) return 0xE5;
        if (d == 0x94) return 0xE5;
        if ((d == 0x96) || (d == 0x97)) return 'i';
    }
    else if (c == 0xD2) {
        if (d == 0x90) return 0xC3;
        if (d == 0x91) return 0xE3;
    }

    while ((**data & 0xC0) == 0x80) {
        (*data)++;
    }

    return '?';
}

void i2c_PutStrUtf8(const char *data)
{
    const unsigned char *utf8;

    if(!display) return;

    utf8 = (const unsigned char *)data;

    while (*utf8) {
        i2c_writeChar(utf8_to_lcd_char(&utf8));
    }
}

void i2c_PutStrUtf8_inv(const char *data)
{
    const unsigned char *utf8;

    if(!display) return;

    utf8 = (const unsigned char *)data;

    while (*utf8) {
        i2c_writeChar_inv(utf8_to_lcd_char(&utf8));
    }
}
//*****************************************************************************/
void i2c_PutStr_inv(const char *data)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //i2c_start(0x70,1,0);    // адрес индикатора, данные, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS | 0x01 );  // Устан.Адрес дисплея и флаг "Запись данных"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }


    while (*data) {
        if(*data == 0) break;
        i2c_writeChar_inv(*data);
        data++;
    }
}

//*****************************************************************************/
// 1 - X [кордината по X][Х 0-132] - значение в пикселях.
// 2 - Y [номер строки дисплея 1-8]
void i2c_SetAddress(unsigned char X, char Y)
{
unsigned int timer;
    //-----------------------------------------------------
    if(!display) return;  // Если нет клавиатуры - выходим.
    //-----------------------------------------------------
    //i2c_start(0x70,0,0);                // 0x70 адрес индикатора, команда, запись
    EUSCI_B_I2C_setSlaveAddress(EUSCI_B0_BASE, SLAVE_ADDRESS );  // Устан.Адрес дисплея и флаг "Запись команд"
    EUSCI_B_I2C_setMode(EUSCI_B0_BASE, EUSCI_B_I2C_TRANSMIT_MODE);      // Устанавливаем режим "Передача"
    EUSCI_B_I2C_enable(EUSCI_B0_BASE);                                  // включаем модуль I2C
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }
    EUSCI_B_I2C_masterSendMultiByteStart (EUSCI_B0_BASE, (X & 0x0f));       // координата x (X3,X2,X1,X0)
    EUSCI_B_I2C_masterSendMultiByteNext  (EUSCI_B0_BASE, (X & 0x0f));       // координата x (X7,X6,X5,X4)
    EUSCI_B_I2C_masterSendMultiByteNext  (EUSCI_B0_BASE, ((X >> 4) | 0x10));// координата x (X7,X6,X5,X4)
    EUSCI_B_I2C_masterSendMultiByteNext  (EUSCI_B0_BASE, (0xb0 | Y));       // координата y
    EUSCI_B_I2C_masterSendMultiByteStop  (EUSCI_B0_BASE); //i2c_stop();

}

