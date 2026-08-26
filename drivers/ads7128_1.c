//********************************************************************
//  utilites.c
//  Различные вспомогательные функции
//--------------------------------------------------------------------
//
//--------------------------------------------------------------------
//
#include "system.h"
//#include "driverlib/MSP430FR5xx_6xx/driverlib.h"
#include <driverlib.h>


//******************************************************************************
// Example Commands ************************************************************
//******************************************************************************

#define ADDR_ADS7128        0x10

/* CMD_TYPE_X_SLAVE - это пример команд, которые мастер отправляет подчиненному.
 * Подчиненное устройство отправит в ответ пример буферов SlaveTypeX.
 *
 * CMD_TYPE_X_MASTER - это пример команд, которые мастер отправляет подчиненному.
 * Подчиненное устройство инициализируется для получения буферов примеров MasterTypeX.
 */
#define CMD_TYPE_0_SLAVE  0
#define CMD_TYPE_1_SLAVE  1
#define CMD_TYPE_2_SLAVE  2

#define CMD_TYPE_0_MASTER 3
#define CMD_TYPE_1_MASTER 4
#define CMD_TYPE_2_MASTER 5
//
// Команды для ADS7128
//
#define CMD_RD_ONE_REG    0x10 //0001 0000b Чтение одного регистра
#define CMD_WR_ONE_REG    0x08 //0000 1000b Запись в один регистр
#define CMD_ST_BIT_REG    0x18 //0001 1000b Установить бит
#define CMD_CL_BIT_REG    0x20 //0010 0000b Очистить бит
#define CMD_RD_MUL_REG    0x30 //0011 0000b Чтение непрерывного блока регистров
#define CMD_WR_MUL_REG    0x28 //0010 1000b Запись непрерывного блока регистров
//
// Регистры ADS7128
//
#define SYSTEM_STATUS     0x00
#define GENERAL_CFG       0x01
#define DATA_CFG          0x02
#define OSR_CFG           0x03
#define OPMODE_CFG        0x04
#define PIN_CFG           0x05
#define GPIO_CFG          0x06
#define SEQUENCE_CFG      0x10
#define CHANNEL_SEL       0x11
#define AUTO_SEQ_CH_SEL   0x12
#define ALERT_CH_SEL      0x14
#define ALERT_MAP         0x16

#define RECENT_CH0_LSB    0xA0
#define RECENT_CH0_MSB    0xA1
#define RECENT_CH1_LSB    0xA2
#define RECENT_CH1_MSB    0xA3
#define RECENT_CH2_LSB    0xA4
#define RECENT_CH2_MSB    0xA5
#define RECENT_CH3_LSB    0xA6
#define RECENT_CH3_MSB    0xA7

#define RMS_CFG           0xC0
#define RMS_LSB           0xC1
#define RMS_MSB           0xC2

#define LENGTH_1          1
#define LENGTH_2          2
#define LENGTH_3          3
#define LENGTH_6          6

#define MAX_BUFFER_SIZE   I2C_BUFFER_SIZE
#define ADS7128_I2C_RECOVERY_DELAY       1000u
#define ADS7128_I2C_TRANSACTION_TIMEOUT  60000u

#define ADS7128_I2C_WAIT_STEP_US       100u
#define ADS7128_I2C_TIMEOUT_MS          20u
#define ADS7128_I2C_TIMEOUT_STEPS \
    ((ADS7128_I2C_TIMEOUT_MS * 1000u) / ADS7128_I2C_WAIT_STEP_US)
//
// Данные для записи
//
#pragma PERSISTENT(Reset)
uint8_t Reset [LENGTH_2]        = {GENERAL_CFG,     0x01};//
#pragma PERSISTENT(Set_AIN3)
uint8_t Set_AIN3 [LENGTH_2]     = {AUTO_SEQ_CH_SEL, 0x08};//
#pragma PERSISTENT(Set_AIN6)
uint8_t Set_AIN6 [LENGTH_2]     = {AUTO_SEQ_CH_SEL, 0x40};//
#pragma PERSISTENT(SEQ_MODE_0)
uint8_t SEQ_MODE_0 [LENGTH_2]   = {SEQUENCE_CFG,    0x00};// SEQ_MODE[1:0] = 0 – Ручной режим в регистре  SEQUENCE_CFG
#pragma PERSISTENT(SEQ_MODE_1)
uint8_t SEQ_MODE_1 [LENGTH_2]   = {SEQUENCE_CFG,    0x01};// SEQ_MODE[1:0] 01b = режим автопоследовательности в регистре  SEQUENCE_CFG
#pragma PERSISTENT(SEQ_START)
uint8_t SEQ_START  [LENGTH_2]   = {SEQUENCE_CFG,    0x11};// SEQ_START[4]-1b+SEQ_MODE[1:0] 01b = Начать чередование каналов
                                                          // для каналов, включенных в регистре AUTO_SEQ_CH_SEL.
#pragma PERSISTENT(CONV_MODE_0)
uint8_t CONV_MODE_0 [LENGTH_2]  = {OPMODE_CFG,      0x00};// рег.0x04 OPMODE_CFG:(биты 6,5)-CONV_MODE[1:0]=00b-ручной и авто режимы
#pragma PERSISTENT(CONV_MODE_1)
uint8_t CONV_MODE_1 [LENGTH_2]  = {OPMODE_CFG,      0x20};// рег.0x04 OPMODE_CFG:(биты 6,5)-CONV_MODE[1:0]=01b-Автономный режим
#pragma PERSISTENT(MANUAL_CHID_3)
uint8_t MANUAL_CHID_3 [LENGTH_2]= {CHANNEL_SEL,     0x03};// Пишем в регистр CHANNEL_SEL: 0011b = AIN3
#pragma PERSISTENT(MANUAL_CHID_6)
uint8_t MANUAL_CHID_6 [LENGTH_2]= {CHANNEL_SEL,     0x06};// Пишем в регистр CHANNEL_SEL: 0110b = AIN6
#pragma PERSISTENT(ALERT_ENABLE)
uint8_t ALERT_ENABLE [LENGTH_2] = {ALERT_MAP,       0x02};// Пишем в регистр: 10b = вывод ALERT выдается, когда RMS_DONE = 1b
//
#pragma PERSISTENT(RMS_cfg_3)
uint8_t RMS_cfg_3 [LENGTH_2]    = {RMS_CFG,         0x33};// Конфигурация режима RMS - 7-4=0х3 - третий канал, 1-0=11b = 65536 замеров
#pragma PERSISTENT(RMS_cfg_6)
uint8_t RMS_cfg_6 [LENGTH_2]    = {RMS_CFG,         0x60};// Конфигурация режима RMS - 7-4=0х6 - шестой канал, 1-0=00b = 1024 замеров
#pragma PERSISTENT(RMS_cfg_7)
uint8_t RMS_cfg_7 [LENGTH_2]    = {RMS_CFG,         0x70};// Конфигурация режима RMS - 7-4=0х7 - седьмой канал, 1-0=00b = 1024 замеров
#pragma PERSISTENT(RMS_ENABLE)
uint8_t RMS_ENABLE [LENGTH_2]   = {GENERAL_CFG,     0x80};// Запускаем вычисление RMS
#pragma PERSISTENT(RMS_DISABLE)
uint8_t RMS_DISABLE [LENGTH_2]  = {GENERAL_CFG,     0x00};// Отключаем RMS
#pragma PERSISTENT(RMS_CLEAR)
uint8_t RMS_CLEAR [LENGTH_2]    = {SYSTEM_STATUS,   0x10};// Снимаем бит RMS_DONE
//
// cycle[48]- набор команд для RMS-опроса восьми входов ADS7128
#pragma PERSISTENT(cycle)
uint8_t cycle[48]={
        AUTO_SEQ_CH_SEL,0x01, CHANNEL_SEL,0x00, RMS_CFG,0x00,
        AUTO_SEQ_CH_SEL,0x02, CHANNEL_SEL,0x01, RMS_CFG,0x10,
        AUTO_SEQ_CH_SEL,0x04, CHANNEL_SEL,0x02, RMS_CFG,0x20,
        AUTO_SEQ_CH_SEL,0x08, CHANNEL_SEL,0x03, RMS_CFG,0x30,
        AUTO_SEQ_CH_SEL,0x10, CHANNEL_SEL,0x04, RMS_CFG,0x40,
        AUTO_SEQ_CH_SEL,0x20, CHANNEL_SEL,0x05, RMS_CFG,0x50,
        AUTO_SEQ_CH_SEL,0x40, CHANNEL_SEL,0x06, RMS_CFG,0x60,
        AUTO_SEQ_CH_SEL,0x80, CHANNEL_SEL,0x07, RMS_CFG,0x70
};
//
/* Используется для отслеживания состояния конечного автомата программного обеспечения*/
volatile I2C_Mode MasterMode = IDLE_MODE;

/* Регистр адреса/команды для использования*/
uint8_t TransmitRegAddr;

/* ReceiveBuffer: Буфер, используемый для приема данных в ISR
 * RXByteCtr: количество байтов, оставшихся для получения
 * ReceiveIndex: индекс следующего байта, который должен быть получен в ReceiveBuffer
 * TransmitBuffer: буфер, используемый для передачи данных в ISR
 * TXByteCtr: количество байтов, оставшихся для передачи
 * TransmitIndex: индекс следующего байта, который будет передан в TransmitBuffer.
 **/
uint8_t ReceiveBuffer[MAX_BUFFER_SIZE];
volatile uint8_t RXByteCtr;
volatile uint8_t ReceiveIndex;
uint8_t TransmitBuffer[MAX_BUFFER_SIZE];
volatile uint8_t TXByteCtr;
volatile uint8_t TransmitIndex;
extern unsigned char error;
unsigned char rms_cfg,channel, alert,system_status,general_cfg,rms_lsb,rms_msb;
unsigned int cnt, rms_resume;

static I2C_Mode I2C_Master_CheckDone(void)
{
    unsigned int timer;

    if (MasterMode == IDLE_MODE) {
        return MasterMode;
    }

    UCB0IE &= ~(UCTXIE | UCRXIE);
    UCB0CTLW0 |= UCTXSTP;

    timer = ADS7128_I2C_RECOVERY_DELAY;
    while ((UCB0CTLW0 & UCTXSTP) && timer--) {
        __no_operation();
    }

    RXByteCtr = 0;
    TXByteCtr = 0;
    ReceiveIndex = 0;
    TransmitIndex = 0;
    MasterMode = TIMEOUT_MODE;

    return MasterMode;
}

static I2C_Mode I2C_Master_WaitDone(void)
{
    unsigned int timeout = ADS7128_I2C_TIMEOUT_STEPS;

    __bis_SR_register(GIE);

    while ((MasterMode != IDLE_MODE) &&
           (MasterMode != TIMEOUT_MODE) &&
           timeout)
    {
        __delay_cycles(100);   // 100 us при MCLK = 1 MHz
        timeout--;
    }

    return I2C_Master_CheckDone();
}

/* MasterTypeX - это пример буферов, инициализированных в мастере,
 * они будут отправлены мастером подчиненному.
 * SlaveTypeX - это примеры буферов, инициализированных на ведомом устройстве,
 * они будут отправлены ведомым устройством на ведущее устройство.
 ** /
uint8_t MasterType2 [LENGTH_6] = {'F', '4', '1', '9', '2', 'B'};
uint8_t MasterType1 [LENGTH_2] = { 8, 9};
uint8_t MasterType0 [LENGTH_1] = { 11};


uint8_t SlaveType2 [LENGTH_6] = {0};
uint8_t SlaveType1 [LENGTH_2] = {0};
uint8_t SlaveType0 [LENGTH_1] = {0};
// */

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

I2C_Mode I2C_Master_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t count)
{
//  unsigned int timer;
    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }

    /* Инициализация движка I2C */
    MasterMode = TX_REG_ADDRESS_MODE;
    TransmitRegAddr = reg_addr;               // Адрес регистра у подчиненного
    //CommandReadFromADS7128 = 0x10;
    RXByteCtr = count;
    TXByteCtr = 1;      // обязательно для ads7128 - передача адреса регистра в устройстве, который читаем
    ReceiveIndex = 0;
    TransmitIndex = 0;

    /* Инициализация  адреса подчиненного и прерывания */
    UCB0I2CSA = dev_addr;
    UCB0IFG &= ~(UCTXIFG + UCRXIFG);          // Очистка всех прерываний
    UCB0IE &= ~UCRXIE;                        // Отключаем прерывания RX
    UCB0IE |= UCTXIE;                         // Включаем прерывания TX

    UCB0CTLW0 |= UCTR + UCTXSTT;              // I2C TX, start condition (UCTR - Transmitter/receiver: 0b = Receiver; 1b = Transmitter)
    //
/*
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }
// */
    //
    return I2C_Master_WaitDone();

}

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

I2C_Mode I2C_Master_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t count)
{
//  unsigned int timer;
    if (count > MAX_BUFFER_SIZE) {
        count = MAX_BUFFER_SIZE;
    }

    /* Инициализация движка I2C */
    MasterMode = TX_REG_ADDRESS_MODE;
    TransmitRegAddr = reg_addr;               // Адрес регистра у подчиненного

    // Копируем данные регистра в буфер передачи TransmitBuffer
    CopyArray(reg_data, TransmitBuffer, count);

    TXByteCtr = count;
    RXByteCtr = 0;
    ReceiveIndex = 0;
    TransmitIndex = 0;

    /* Инициализация  адреса подчиненного и прерывания */
    UCB0I2CSA = dev_addr;
    UCB0IFG &= ~(UCTXIFG + UCRXIFG);        // Очистка всех прерываний
    UCB0IE &= ~UCRXIE;                      // Отключаем прерывания RX
    UCB0IE |= UCTXIE;                       // Включаем прерывания TX

    UCB0CTLW0 |= UCTR + UCTXSTT;            // I2C TX, start условие (UCTR - Transmitter/receiver: 0b = Receiver; 1b = Transmitter)

/*
    // Обязательная процедура: Проверяем и ждем, если надо освобождения шины данных
    timer=0;// ожидание проводим с таймером
    while(EUSCI_B_I2C_isBusBusy(EUSCI_B0_BASE)){   //ожидание окончания формирования бита стоп
           timer++;
           if(timer>5000)break; // таймер прерывания цикла
    }
// */

    return I2C_Master_WaitDone();
}

void CopyArray(uint8_t *source, uint8_t *dest, uint8_t count)
{
    uint8_t copyIndex = 0;
    for (copyIndex = 0; copyIndex < count; copyIndex++) {dest[copyIndex] = source[copyIndex];}
}


//******************************************************************************
// Инициализация Устройства ****************************************************
//******************************************************************************
void initGPIO()
{
    // Конфигурация GPIO
    P2OUT &= ~BIT7;                           // Чистим P2.6 выходную защелку
    P2DIR |= BIT7;                            // для LED
    P1SEL1 |= BIT6 | BIT7;                    // Вывода для I2C
// Отключить режим высокого сопротивления по умолчанию при включении GPIO,
// чтобы активировать ранее настроенные параметры порта
    PM5CTL0 &= ~LOCKLPM5;
}
void initI2C()
{
    UCB0CTLW0 = UCSWRST;                        // Enable SW reset
    UCB0CTLW0 |= UCMODE_3 | UCMST | UCSSEL__SMCLK | UCSYNC; // I2C master mode, SMCLK
    //UCB0CTLW1 |= 0x08;
    UCB0CTLW1 = UCASTP_0;   // automatic STOP disabled
    UCB0BRW = 160;                              // fSCL = SMCLK/160 = ~100kHz
    UCB0I2CSA = ADDR_ADS7128;                   // Slave Address
    UCB0CTLW0 &= ~UCSWRST;                      // Clear SW reset, resume operation
    UCB0IE |= UCNACKIE;
}
unsigned int measure_RMS(unsigned char *data)
{
    //------------------------------------------------------------------------
    //    1. СБРОС.
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_ST_BIT_REG, Reset,      LENGTH_2);
    __delay_cycles(10000);  // ждем завершения выполнения ДЛИННОЙ команды!!!
    //------------------------------------------------------------------------
    //     2. Выбираем канал AINx
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, &data[0],    LENGTH_2);
    //
    //------------------------------------------------------------------------
    //     3.0. SEQ_MODE[1:0] = 0 – Преобразования в каналах остановлены
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, SEQ_MODE_1,    LENGTH_2);//Включаем SEQ_MODE[1:0] 01b = режим автопоследовательности в регистре  SEQUENCE_CFG,
    //------------------------------------------------------------------------
    //     3.1. CONV_MODE = 0 -  Ручной режим. Преобразования инициируются хостом.
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, CONV_MODE_1, LENGTH_2);//Пишем в регистр: (биты 6,5)-CONV_MODE[1:0]=01b - Автономный режим
    //------------------------------------------------------------------------
    //     4. Настроиваем желаемый Идентификатор канала в поле MANUAL_CHID ( Регистр CHANNEL_SEL (Address = 0x11)
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, &data[2],    LENGTH_2);//
    //
    //------------------------------------------------------------------------
    //     4. Даем команду читать регистр CHANNEL_SEL
    //
    I2C_Master_ReadReg(ADDR_ADS7128, CHANNEL_SEL, LENGTH_1);                  // Получаем данные из регистра 0x11 CHANNEL_SEL
    CopyArray(ReceiveBuffer, &channel, LENGTH_1);
    //
    //    Процедура использования модуля RMS описана в следующих шагах:
    //------------------------------------------------------------------------
    //    1. Выбираем канал для вычисления RMS, используя поле RMS_CHID в регистре RMS_CFG (Адрес = 0xC0).
    //    2. Определяем время, в течение которого будет вычисляться RMS, настроив поле RMS_SAMPLES[1:0] в регистре RMS_CFG .
    //
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, &data[4],    LENGTH_2);//
    //
    //------------------------------------------------------------------------
    //     4. Даем команду читать регистр RMS_CFG
    //                 Адрес       Регистр        Длина
    I2C_Master_ReadReg(ADDR_ADS7128, RMS_CFG, LENGTH_1);                  // читаем RMS_CFG
    //        Откуда         Куда      Длина
    CopyArray(ReceiveBuffer, &rms_cfg, LENGTH_1);
    //
    //------------------------------------------------------------------------
    //     Читаем регистр GENERAL_CFG
    //
    I2C_Master_ReadReg(ADDR_ADS7128, GENERAL_CFG, LENGTH_1);              // Получаем данные из регистра GENERAL_CFG
    CopyArray(ReceiveBuffer, &general_cfg, LENGTH_1);
    //
    __no_operation();
    //------------------------------------------------------------------------
    //    3. Запускаем вычисление RMS, установив для RMS_EN значение 1 в регистре GENERAL_CFG (Адрес = 0x01).
    //                  Адрес         Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, RMS_ENABLE,  LENGTH_2); // Пишем в бит 7: RMS_EN
    //
    //                  Адрес       Команда         Данные       Длина
    I2C_Master_WriteReg(ADDR_ADS7128, CMD_WR_ONE_REG, SEQ_START,   LENGTH_2);// SEQ_START - 1b = Начать чередование каналов

    __no_operation();

        __delay_cycles(10000);  // ждем завершения выполнения ДЛИННОЙ команды!!!

    __no_operation();

    P2OUT &= ~BIT7; //led
        I2C_Master_ReadReg(ADDR_ADS7128, RMS_LSB, LENGTH_1);
        rms_lsb = ReceiveBuffer[0];
        I2C_Master_ReadReg(ADDR_ADS7128, RMS_MSB, LENGTH_1);
        rms_msb = ReceiveBuffer[0];
        rms_resume  = rms_msb<<8;
        rms_resume |= rms_lsb;
        rms_resume  = rms_resume>>4;
    P2OUT = BIT7;   //led

        __no_operation();

        return  rms_resume;
}
