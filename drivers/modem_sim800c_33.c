//********************************************************************
//  sim800с.c
//  Файл процедур обслуживания модема
//--------------------------------------------------------------------
// Автоматически определяет тип СИМ-карті (МТС или КИЕВСТАР)
// modem_sim800с_31.c - зміна алгоритму включення модема. строки 680-700
//--------------------------------------------------------------------
//
#include <stdlib.h>
//#include "driverlib/MSP430FR5xx_6xx/driverlib.h"
#include <driverlib.h>
#include "system.h"
#include "rdx0154.h"              // Утилиты обслуживания LCD и клавиатуры

//#define SW_RESET()      PMMCTL0 = PMMPW + PMMSWBOR + (PMMCTL0 & 0x0003);  // software BOR reset
#define SW_RESET()      rePWR_sim800c ();return 0;  // software BOR reset
//********************************************************************
#pragma PERSISTENT(try)
char try=0;                         // флаг режима работы СКМ8:
//
extern signed char server;                             // флаг Выбор сервера
//
extern struct skm8_packet pack;               // Пакет данных для передачи на сервер.
unsigned char operat_GSM=0;                     //!!!! Код оператора GSM связи.
unsigned int repl;                              // результат сравнения командой cmp_str();
extern struct rxM_buf RxMbuf;                   // буфер модема
extern struct buf_str TxMbuf;
unsigned int time_w1;                           // таймер ожидания процесса 1
unsigned int result_remote_command;
extern uint16_t u,y;
extern unsigned int pack_time;
extern  struct {
    sens_str a[8];  // Аналоговые входы устройства
    sens_str d[8];  // Цифровые входы устройства
    sens_str v[2];  // Внутренние параметры устройства: температура и питание.
}sens;
extern  unsigned char display;  // наличие дисплея:     1-дисплей есть, 0-нет.
unsigned char ip[11];           // IP адреса СКМ, отримана при підключені до мережі
extern  unsigned int dataplace;


//
//#pragma PERSISTENT( ind_chas_arch1)
//extern unsigned int ind_chas_arch1;             // текущий индекс в часовом архиве 1
//#pragma PERSISTENT( ind_chas_arch2)
//extern unsigned int ind_chas_arch2;             // текущий индекс в часовом архиве 2
//
//#pragma PERSISTENT( ind_after_last_transmit)  // индекс в часовом архиве (1 или 2) после прошлой передачи
//extern unsigned int ind_after_last_transmit;    // используется для подсчета количества часовых записей,
                                                // которые нужно передать на сервер их часового (1 или 2) архива.
//
//#pragma PERSISTENT( ind_chas_arch1_max)       // Максимальное количество записей в архиве 744
//extern const unsigned int ind_chas_arch1_max;
//#pragma PERSISTENT( ind_chas_arch2_max)       // Максимальное количество записей в архиве 744
//extern const unsigned int ind_chas_arch2_max;   // сюда переключается после заполнения архива 1
//
extern unsigned char start_init_job;            // флаг "НАЧАТЬ РАБОТУ"
extern unsigned char ipdec[12];    // IP адреса для тестування
extern unsigned char iphex[3];
//extern uint8_t encrdata[272];
//extern U8 decrdata[272];
extern unsigned int lenreq,lenhead,lendata;
extern unsigned char encrdata[ENCRDATA_SIZE];
extern uint8_t CipherKey[32];
extern uint8_t DataAESencrypted[16];       // Зашифровані дані
extern uint8_t DataAESdecrypted[16];       // Розшифровані дані
//
static char is_dec_digit(unsigned char c)
{
    return ((c >= '0') && (c <= '9'));
}

static char is_hex_digit(unsigned char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}

static char remote_payload_is_hex(unsigned int start,
                                  unsigned int hex_len)
{
    unsigned int i;

    if ((start + hex_len) > RxMbuf.max) {
        return 0;
    }

    for (i = 0; i < hex_len; i++) {
        if (!is_hex_digit(RxMbuf.buf[start + i])) {
            return 0;
        }
    }

    return 1;
}

static char remote_payload_find_before_closed(
    unsigned int closed_pos,
    unsigned int hex_len,
    unsigned int *payload_start)
{
    unsigned int gap;
    unsigned int start;

    /*
     * CLOSED знаходиться трохи після HTTP body.
     * Не покладаємося на фіксоване зміщення 73/137:
     * шукаємо суцільний HEX payload у невеликому
     * вікні безпосередньо перед CLOSED.
     */
    for (gap = 0u; gap <= 32u; gap++) {
        if (closed_pos < (hex_len + gap)) {
            continue;
        }

        start = closed_pos - hex_len - gap;

        if (remote_payload_is_hex(start, hex_len)) {
            *payload_start = start;
            return 1;
        }
    }

    return 0;
}


static unsigned int remote_cmp_str_limited(
    const char *string,
    unsigned char *buf,
    unsigned int limit)
{
    unsigned int ind_str = 0u;
    unsigned int pos;

    for (pos = 0u; pos < limit; pos++)
    {
        if ((unsigned char)string[ind_str] == buf[pos]) {
            ind_str++;
        }
        else {
            ind_str = 0u;
        }

        if (string[ind_str] == 0) {
            return pos;
        }
    }

    return 0u;
}


static void clear_csq(void)
{
    unsigned char i;

    for (i = 0; i < 5; i++) {
        pack.csq[i] = '0';
    }
}

char parse_csq_from_rx(unsigned int pos)
{
    unsigned char i;
    unsigned int rx_pos;
    char csq_text[6];
    unsigned char digit_count = 0;
    unsigned char rssi_digit_count = 0;
    unsigned char comma_seen = 0;
    unsigned int rssi_value = 0;
    unsigned long csq_value;

    clear_csq();
    if (pos == 0) {
        sens.d[7].dat = 0;
        return 0;
    }

    rx_pos = pos + 1;
    while ((rx_pos < RxMbuf.max) && (RxMbuf.buf[rx_pos] == ' ')) {
        rx_pos++;
    }

    for (i = 0; i < 5; i++) {
        if (rx_pos >= RxMbuf.max) {
            break;
        }
        if ((RxMbuf.buf[rx_pos] == 0x0d) || (RxMbuf.buf[rx_pos] == 0x0a) ||
            (RxMbuf.buf[rx_pos] == 0)) {
            break;
        }
        if (RxMbuf.buf[rx_pos] == ',') {
            if ((digit_count == 0) || comma_seen) {
                clear_csq();
                sens.d[7].dat = 0;
                return 0;
            }
            comma_seen = 1;
            pack.csq[i] = '.';
        }
        else if (is_dec_digit(RxMbuf.buf[rx_pos])) {
            digit_count++;
            if (!comma_seen) {
                rssi_digit_count++;
                if (rssi_digit_count > 2) {
                    clear_csq();
                    sens.d[7].dat = 0;
                    return 0;
                }
                rssi_value = (rssi_value * 10) + (RxMbuf.buf[rx_pos] - '0');
            }
            pack.csq[i] = RxMbuf.buf[rx_pos];
        }
        else {
            clear_csq();
            sens.d[7].dat = 0;
            return 0;
        }
        rx_pos++;
    }
    if ((digit_count == 0) || !comma_seen || (digit_count == rssi_digit_count)) {
        clear_csq();
        sens.d[7].dat = 0;
        return 0;
    }

    if ((rssi_value > 31) && (rssi_value != 99)) {
        clear_csq();
        sens.d[7].dat = 0;
        return 0;
    }

    for (i = 0; i < 5; i++) {
        csq_text[i] = pack.csq[i];
    }
    csq_text[5] = 0;
    csq_value = atol(csq_text);
    sens.d[7].dat = (csq_value == 99) ? 0 : csq_value;
    return 1;
}

/*
 * ----------------- Send_from_UART0 ---------------------------------
 */
//void Send_from_UART0 (unsigned char* buffer) {
void Send_from_UART0 (char* buffer) {
U16 i;
    i=0;
    while(buffer[i]!=0){
        // Загружаем данные в буфер
        //
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, buffer[i]);
        i++;
    }
    __delay_cycles(50);  // ждем...// ожидаем завершения передачи пакета
}
/*
void Send_from_UART0_bin_code (unsigned char* buffer, unsigned int len) {
unsigned char binary[2];
unsigned int i;
    GPIO_setOutputHighOnPin (GPIO_PORT_P2, GPIO_PIN7);  // ВКЛючаем светодиод

    for (i = 0; i < len; i++) {
        HTOA( buffer[i], &binary[0]);       // Переписуємо бінарні дані в масив у вигляді ASCII коду
        // Загружаем данные в буфер модема:
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, binary[0]);
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, binary[1]);
    }
    __delay_cycles(50);  // ждем...// ожидаем завершения передачи пакета

}
// */
//
//********************************************************************/
// Инициализация порта UART0
void init_uart0 (void)
{
    RxMbuf.ind = 0;
    RxMbuf.max = 512;           //255;
    //
    __delay_cycles(100000);                    // ждем...
    //
    // Конфигурируем выводы UART
    // устанавливаем P2.0 и P2.1 на второй режим работы (UCA0TXD/UCA0SIMO, UCA0RXD/UCA0SOMI)...
    //
    GPIO_setAsPeripheralModuleFunctionInputPin(
        GPIO_PORT_P2,
        GPIO_PIN0 + GPIO_PIN1,
        GPIO_SECONDARY_MODULE_FUNCTION
    );
/*
    //
    // Настраиваем UART на скорость 9600 бот как драйвер модема
    //
    EUSCI_A_UART_initParam param = {0};
    param.selectClockSource = EUSCI_A_UART_CLOCKSOURCE_SMCLK;   // тактирование от главных часов
//    param.clockPrescalar = 52;          //4800==104; 9600==52; 19200==26;  115200==4;
//    param.firstModReg =   1;            //4800==2;   9600==1;  19200==0;   115200==5;
//    param.secondModReg = 73;            //4800==182; 9600==73; 19200==214; 115200==85;
    param.clockPrescalar = 4;   //115200
    param.firstModReg = 5;
    param.secondModReg = 85;
    param.parity = EUSCI_A_UART_NO_PARITY;
    param.msborLsbFirst = EUSCI_A_UART_LSB_FIRST;
    param.numberofStopBits = EUSCI_A_UART_ONE_STOP_BIT;
    param.uartMode = EUSCI_A_UART_MODE;
    //param.overSampling = EUSCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION;
    param.overSampling = 1;//4800==1;9600==1;19200==1;115200==1;

    if(STATUS_FAIL == EUSCI_A_UART_init(EUSCI_A0_BASE, &param))
    {
        return;
    }

    EUSCI_A_UART_enable(EUSCI_A0_BASE);
    EUSCI_A_UART_clearInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
    // разрешаем прерывание USCI_A0 RX
    EUSCI_A_UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
// */
    // Configure USCI_A0 for UART mode
    UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
    UCA0CTL1 |= UCSSEL__SMCLK;                // CLK = SMCLK
    UCA0BR0 = 8;                              // 1000000/115200 = 8.68
    UCA0MCTLW = 0xD600;                       // 1000000/115200 - INT(1000000/115200)=0.68
                                              // UCBRSx value = 0xD6 (See UG)
    UCA0BR1 = 0;
    UCA0CTL1 &= ~UCSWRST;                     // release from reset
    UCA0IE |= UCRXIE;                         // Enable USCI_A0 RX interrupt


}
//
/********************************************************************/
/*
 * Communication retry timing.
 *
 * During OTA we retry quickly and preserve the OTA session.
 * Outside OTA the legacy delay/error code remains unchanged.
 */
static void set_comm_retry_pack_time(unsigned int normal_pack_time)
{
    if ((ota_get_status() == 1u) ||
        (ota_get_status() == 2u))
    {
        pack_time = 3u;
    }
    else
    {
        pack_time = normal_pack_time;
    }
}

/********************************************************************/
// Процедура передачи пакета данных на сервер
char send_packet (void)
{
unsigned char index;
unsigned char connected;
unsigned long ota_session_start_received;
unsigned char gprs_recover_try = 0;
unsigned int lenreq,i,remote_payload_start;//
unsigned int remote_payload_hex_len;
unsigned int aes_block;

    ota_session_start_received = ota_get_received();

    __delay_cycles(100000);                    // ждем...

    index = 0;
//
// 7. Проверяем силу сигнала              //////////////////////////////////////////
//
    for(i=0; i< 512; i++) RxMbuf.buf[i] = 0;
    RxMbuf.ind = 0;
    // Отправляем пакет на передаче
    Send_from_UART0("AT+CSQ\r\n");
    repl = wait_compl ("+CSQ:", &RxMbuf.buf[0], 1000);

    if (!parse_csq_from_rx(repl)) {
        i2c_SetAddress(100,LINE6); i2c_PutStr("eCSQ ");
    }
    else {
        i2c_SetAddress(100,LINE6); i2c_PutStr("OK 1 ");
    }
        //
        // Подключаем GPRS сервис
        //
        RxMbuf.ind = 0;
        Send_from_UART0("AT+CIPSHUT\r\n");          // ќтправл¤ем пакет на передачу
        __delay_cycles(500000);  // ждем... 1с=8,000,000

        for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
        RxMbuf.ind = 0;
        Send_from_UART0("AT+CIPSTATUS\r\n");          // ќтправл¤ем пакет на передачу
        __delay_cycles(500000);  // ждем... 1с=8,000,000

        if(cmp_str("IP INITIAL",&RxMbuf.buf[0])){
            index++;
            //
            // Подключаем GPRS сервис
            //
            for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
            RxMbuf.ind = 0;
            Send_from_UART0("AT+CGATT?\r\n");               // Отправляем пакет на передаче

            repl = wait_compl ("OK", &RxMbuf.buf[0], 500);

            if(repl){
                index++;
                i2c_SetAddress(100,LINE6); i2c_PutStr("OK 2 ");
            }
            else  {
                if(start_init_job != 0){
                    start_init_job=1;
                    i2c_SetAddress(100,LINE6); i2c_PutStr("OK 2\xe5");
                }
                if ((ota_get_status() == 1u) ||
                    (ota_get_status() == 2u))
                {
                    set_comm_retry_pack_time(424u);
                    P4OUT &= ~BIT4;
                    return 0;
                }

                SW_RESET();  // программный сброс
            }
            //
            // указываем точку входа:
            //
gprs_recover_cstt:
            lenreq  = WriteBuf( &TxMbuf.buf[0], "AT+CSTT=");

            if(operat_GSM == '1')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"www.umc.ua\"");           // МТС      AT+CIPCSGP=1,"www.umc.ua"
       //else if(operat_GSM == '2')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"ks\"");     // Киевстар AT+CIPCSGP=1,"www.kyivstar.net"
       else if(operat_GSM == '2')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"www.kyivstar.net\"");     // Киевстар AT+CIPCSGP=1,"www.kyivstar.net"
       else if(operat_GSM == '3')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"SPEED\"");                // Лайф     AT+CIPCSGP=1,"SPEED"
       else if(operat_GSM == '4')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"internet.beeline.ua\"");  // Билайн   AT+CIPCSGP=1,"internet.beeline.ua"
       else if(operat_GSM == '5')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"3g.utel.ua\"");           // Утел     AT+CIPCSGP=1,"3g.utel.ua"
       else if(operat_GSM == '6')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"internet\"");             // Водафон  AT+CIPCSGP=1,"Vodafone"
       else if(operat_GSM == '7')lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\"internet\"");             // lifecell  AT+CIPCSGP=1,"lifecell"

             lenreq += WriteBuf( &TxMbuf.buf[lenreq], "\r\n");         //
             TxMbuf.buf[lenreq] = 0;// обозначаем конец строки

             for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
             RxMbuf.ind = 0;

             Send_from_UART0((char*)TxMbuf.buf);  // ѕодключаем GPRS сервис

             repl = wait_compl ("OK", &RxMbuf.buf[0], 500);

             if(repl){
                 index++;
                 i2c_SetAddress(100,LINE6); i2c_PutStr("OK 3 ");
             }
             else  {
                 if(start_init_job != 0){
                     start_init_job=1;
                 }
                 if ((ota_get_status() == 1u) ||
                     (ota_get_status() == 2u))
                 {
                     set_comm_retry_pack_time(424u);
                     P4OUT &= ~BIT4;
                     return 0;
                 }

                 SW_RESET();  // программный сброс
             }
             //
             // активируем подключение через GPRS
             //
             for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
             RxMbuf.ind = 0;

             Send_from_UART0("AT+CIICR\r\n");       // активируем подключение через GPRS

             repl = wait_compl ("OK", &RxMbuf.buf[0], MODEM_GPRS_BRINGUP_TIMEOUT_MS);

             //if(cmp_str("PDP DEACT",&RxMbuf.buf[0])){
             if(cmp_str("ERROR",&RxMbuf.buf[0]) || !repl){

                 /*
                  * GPRS/PDP recovery.
                  *
                  * Після короткого збою мережі SIM800 може показувати
                  * CGATT:1, але AT+CIICR відповідає PDP DEACT / ERROR.
                  * Один раз примусово перепідключаємо packet service
                  * без вимкнення живлення модема.
                  */
                 if(gprs_recover_try == 0u)
                 {
                     gprs_recover_try = 1u;

                     for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;
                     RxMbuf.ind = 0;
                     Send_from_UART0("AT+CGATT=0\r\n");
                     wait_compl("OK", &RxMbuf.buf[0], 1000);
                     delay_ms(2000);

                     for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;
                     RxMbuf.ind = 0;
                     Send_from_UART0("AT+CGATT=1\r\n");
                     wait_compl("OK", &RxMbuf.buf[0], 2000);
                     delay_ms(5000);

                     goto gprs_recover_cstt;
                 }

                 if(start_init_job != 0){
                     start_init_job=1;
                 }
                 P2OUT = 0;      //
                 P2DIR = 0xFF;   //
                 //
                 // отключаем DTR вверх (P2.2)
                 //
                 P2OUT &= ~BIT2;
                 //
                 // снимаем PWRKEY вниз (P3.5)
                 //
                 P3OUT &= ~BIT5;
                 //
                 // 4 - M_ON - включение модема
                 // ОТКЛючаем БЛОК питания модема
                 //
                 P4OUT &= ~BIT4;
                 //
                   i2c_SetAddress(100,LINE6); i2c_PutStr("eGPRS");
                 delay_ms(1000);
                 try++;
                 if(try >=3){            // .
                     set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                     operat_GSM = 0;     //
                     try = 0;            // чистим количество попыток
                     //
                     // 4 - M_ON - включение модема
                     // ОТКлючаем БЛОК питания модема
                     //
                     P4OUT &= ~BIT4;              //
                     //
                     return 0;   // выходим из инициализации
                 }
                 //
                 return 0;   // выходим из инициализации
             }
             else{
                 if(repl){      // получили "ОК"
                     index++;
                     i2c_SetAddress(100,LINE6); i2c_PutStr("OK 4 ");

                     //
                     // берем локальный IP адрес
                     //
                     for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
                     RxMbuf.ind = 0;

                     Send_from_UART0("AT+CIFSR\r\n");   // берем локальный IP адрес
                     delay_ms(1000);
                     repl = wait_compl (".", &RxMbuf.buf[0], MODEM_IP_ADDRESS_TIMEOUT_MS); // Очікуємо ІР адресу, там обов'язково повина бути крапка '.' !
                     if(!repl){
                         if(start_init_job != 0){
                             start_init_job=1;
                         }
                         try++;
                         if(try >=3){            // .
                             set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                             operat_GSM = 0;     //
                             try = 0;            // чистим количество попыток
                         }
                         P4OUT &= ~BIT4;
                         i2c_SetAddress(100,LINE6); i2c_PutStr("eIP  ");
                         return 0;
                     }

                     for(i=0; i < 12; i++) ipdec[i]=0;
                     for(i=0; i < 12; i++) {
                         if(RxMbuf.buf[i+repl+1]== 0x0d) break;
                         ipdec[i] = RxMbuf.buf[i+repl+1];  // чистим буфер приема
                     }
                     __no_operation();

                     IPdecToHex(ipdec,iphex); //Формуємо ключ з ІР-адреси. Він у iphex[]
                     i2c_SetAddress(100,LINE6); i2c_PutStr("OK 5 ");

                     try = 0;                           // обнуляем попытки
                     //break;                             // прерываем цикл while()
                 }
                 else  {
                     if(start_init_job != 0){
                         start_init_job=1;
                     }
                     try++;
                     if(try >=3){            // .
                         set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                         operat_GSM = 0;     //
                         try = 0;            // чистим количество попыток
                         //
                         // 4 - M_ON - включение модема
                         // ОТКлючаем БЛОК питания модема
                         //
                         P4OUT &= ~BIT4;              //
                         //
                         return 0;   // выходим из инициализации
                     }
                     if ((ota_get_status() == 1u) ||
                         (ota_get_status() == 2u))
                     {
                         set_comm_retry_pack_time(424u);
                         P4OUT &= ~BIT4;
                         return 0;
                     }

                     SW_RESET();  // программный сброс
                 }
             }
        }
        // повторная передача, если соединение не закрыто
        else if(cmp_str("TCP CLOSED",&RxMbuf.buf[0])){
            index++;
        }
        else if(cmp_str("IP GPRSACT",&RxMbuf.buf[0])){
            P2OUT = 0;      //
            P2DIR = 0xFF;   //
            //
            // отключаем DTR вверх (P2.2)
            //
            P2OUT &= ~BIT2;
            //
            // снимаем PWRKEY вниз (P3.5)
            //
            P3OUT &= ~BIT5;
            //
            // 4 - M_ON - включение модема
            // ОТКЛючаем БЛОК питания модема
            //
            P4OUT &= ~BIT4;
            //
            __delay_cycles(10000000);  // ждем... 1с=8,000,000
            if(start_init_job != 0){
                start_init_job=1;
            }
            try++;
            if(try >=3){            // .
                set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                operat_GSM = 0;     //
                try = 0;            // чистим количество попыток
                //
                // 4 - M_ON - включение модема
                // ОТКлючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;              //
                //
                return 0;   // выходим из инициализации
            }
            //
            if ((ota_get_status() == 1u) ||
                (ota_get_status() == 2u))
            {
                set_comm_retry_pack_time(424u);
                P4OUT &= ~BIT4;
                return 0;
            }

            SW_RESET();  // программный сброс
            //
        }
        else if(cmp_str("PDP DEACT",&RxMbuf.buf[0])){
            if(start_init_job != 0){
                start_init_job=1;
            }
            P2OUT = 0;      //
            P2DIR = 0xFF;   //
            //
            // отключаем DTR вверх (P2.2)
            //
            P2OUT &= ~BIT2;
            //
            // снимаем PWRKEY вниз (P3.5)
            //
            P3OUT &= ~BIT5;
            //
            // 4 - M_ON - включение модема
            // ОТКЛючаем БЛОК питания модема
            //
            P4OUT &= ~BIT4;
            //
            __delay_cycles(10000000);  // ждем... 1с=8,000,000
            try++;
            if(try >=3){            // .
                set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                operat_GSM = 0;     //
                try = 0;            // чистим количество попыток
                //
                // 4 - M_ON - включение модема
                // ОТКлючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;              //
                //
                return 0;   // выходим из инициализации
            }
            //
            if ((ota_get_status() == 1u) ||
                (ota_get_status() == 2u))
            {
                set_comm_retry_pack_time(424u);
                P4OUT &= ~BIT4;
                return 0;
            }

            SW_RESET();  // программный сброс
            //
        }
        else  {
            if(start_init_job != 0){
                start_init_job=1;
            }
            P2OUT = 0;      //
            P2DIR = 0xFF;   //
            //
            // отключаем DTR вверх (P2.2)
            //
            P2OUT &= ~BIT2;
            //
            // снимаем PWRKEY вниз (P3.5)
            //
            P3OUT &= ~BIT5;
            //
            // 4 - M_ON - включение модема
            // ОТКЛючаем БЛОК питания модема
            //
            P4OUT &= ~BIT4;
            //
            __delay_cycles(10000000);  // ждем... 1с=8,000,000
            try++;
            if(try >=3){    // СИМ-карта не найдена.
                set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
                operat_GSM = 0;     //
                try = 0;            // чистим количество попыток
                //
                // 4 - M_ON - включение модема
                // ОТКлючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;              //
                //
                return 0;   // выходим из инициализации
            }
            //
            if ((ota_get_status() == 1u) ||
                (ota_get_status() == 2u))
            {
                set_comm_retry_pack_time(424u);
                P4OUT &= ~BIT4;
                return 0;
            }

            SW_RESET();  // программный сброс
            //
        }
//    }

        //********************************************************************************************
                if(operat_GSM == '7'){  // М2M Express AT+CIPCSGP=1,"internet.emt.ee"
                    //__delay_cycles(10000000);  // 10сек ждем долго!
                    __delay_cycles(20000000);  // 20сек ждем долго!
                }
        //********************************************************************************************

    //
    // обработка отсутствия Входа в  добавлена 08.09.2018
    // уязвимость - постоянный поиск СИМ приводит к разрядке аккумулятора!
    //
//*
// */
//
//  открываем соединение:
//
ota_next_tcp:

     for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;
     RxMbuf.ind = 0;

     if(server==0) Send_from_UART0("AT+CIPSTART=\"TCP\",\"www.skydom.info");        // СКАЙДОМ:
else if(server==1) Send_from_UART0("AT+CIPSTART=\"TCP\",\"sky.kgaz.com.ua");        // КРЕМЕНЧУКГАЗ:
//else if(server==1) Send_from_UART0("AT+CIPSTART=\"TCP\",\"193.109.249.254");      // КРЕМЕНЧУКГАЗ:
else if(server==2) Send_from_UART0("AT+CIPSTART=\"TCP\",\"metro.gazbil.ks.ua");     // ХЕРСОНГАЗ:
else if(server==3) Send_from_UART0("AT+CIPSTART=\"TCP\",\"sky.kirgas.com");         // КІРОВОГРАДГАЗ:
//else if(server==4) Send_from_UART0("AT+CIPSTART=\"TCP\",\"grmu.skydom.info");       // сервер grmu.skydom.info        СКАЙДОМ
else if(server==4) Send_from_UART0("AT+CIPSTART=\"TCP\",\"78.27.235.144");          // сервер skydom
else if(server==5) Send_from_UART0("AT+CIPSTART=\"TCP\",\"grmu-skydom.grmu.com.ua");// сервер ГАЗМЕРЕЖІ УКРАЇНИ
//else if(server==5) Send_from_UART0("AT+CIPSTART=\"TCP\",\"185.2.109.40");         // сервер ГАЗМЕРЕЖІ УКРАЇНИ
else if(server==6) Send_from_UART0("AT+CIPSTART=\"TCP\",\"31.42.179.214");          // сервер Шепетівкагаз

     if(server==3){
         Send_from_UART0("\",\"8181\"\r\n");  // открываем соединение:
     }
     else if(server==4 || server==5 || server==6){
         Send_from_UART0("\",\"61003\"\r\n");  // открываем соединение:
     }
     else{
         Send_from_UART0("\",\"80\"\r\n");  // открываем соединение:
     }
//

    i2c_SetAddress(100,LINE6); i2c_PutStr("OK 6 ");

    delay_ms(1000);
    //--------------------------------------------------------------
time_w1 = 0;
connected = 0;

while(time_w1 < MODEM_TCP_CONNECT_TIMEOUT_MS){
    if(cmp_str("CONNECT OK",&RxMbuf.buf[0])){
        connected = 1;
        index++;                                // !!! порядок !!!
        break;                                  // переходим к передаче данных
    }

    if(cmp_str("TCP CLOSED",&RxMbuf.buf[0])){
        if(start_init_job != 0){
            start_init_job=1;
        }
        try++;
        if(try >=3){            // .
            set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
            operat_GSM = 0;     //
            try = 0;            // чистим количество попыток
            P4OUT &= ~BIT4;     //
            return 0;           // выходим из инициализации
        }
        if ((ota_get_status() == 1u) ||
            (ota_get_status() == 2u))
        {
            set_comm_retry_pack_time(424u);
            P4OUT &= ~BIT4;
            return 0;
        }

        SW_RESET();             // программный сброс
    }

    if(cmp_str("CONNECT FAIL",&RxMbuf.buf[0])){
        if(start_init_job != 0){
            start_init_job=1;
        }
        try++;
        if(try >=3){            // .
            set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
            operat_GSM = 0;     //
            try = 0;            // чистим количество попыток
            P4OUT &= ~BIT4;     //
            return 0;           // выходим из инициализации
        }
        if ((ota_get_status() == 1u) ||
            (ota_get_status() == 2u))
        {
            set_comm_retry_pack_time(424u);
            P4OUT &= ~BIT4;
            return 0;
        }

        SW_RESET();             // программный сброс
    }

    delay_ms(MODEM_POLL_INTERVAL_MS);
    time_w1 += MODEM_POLL_INTERVAL_MS;
}

if(!connected){
    if(start_init_job != 0){
        start_init_job=1;
    }
    try++;
    if(try >=3){            // .
        set_comm_retry_pack_time(424u);    // Устанавливаем время ожидания 1 час
        operat_GSM = 0;     //
        try = 0;            // чистим количество попыток
        P4OUT &= ~BIT4;     //
        return 0;           // выходим из инициализации
    }
    if ((ota_get_status() == 1u) ||
        (ota_get_status() == 2u))
    {
        set_comm_retry_pack_time(424u);
        P4OUT &= ~BIT4;
        return 0;
    }

    SW_RESET();             // программный сброс
}
    delay_ms(100);
//
// запускаем передачу данных
//
    Send_from_UART0("AT+CIPSEND\r\n");  //

    repl = wait_compl (">", &RxMbuf.buf[0], 500);

    if(repl > 0){
//
//
//---------------------------------------------------------------------------------------------
        for(i=0; i<sizeof(TxMbuf.buf); i++){ TxMbuf.buf[i]= 0; }   // Чистимо буфер передачі

        httpreq_smart();                            // формуемо HTTP-запит на передачу даних.
                                                    // 136 байт - заголовок в TxMbuf.buf
                                                    // 274 байт - данні в RxMbuf.buf
        __no_operation();
//
//---------------------------------------------------------------------------------------------
//
//
        delay_ms(1000);  // ждемо... поки модем не вгамується...

        //void aes256(U8 *rawdata, U8 *outdata, U8 index_outdata)
        // rawdata       - Вхідні дані
        // outdata       - Вихідні, кодовані дані
        // index_outdata - Довжина вихідних даних
        //
        aes256(&encrdata[0], &RxMbuf.buf[0], 0);            // Виконуемо кодування пакета в aes256
                                                            // 272 байт - кодовані дані в encrdata
        ItoDecAShot_bn(lendata-2,  &TxMbuf.buf[dataplace]); // записываем длину данных !!! ItoDecAShot_bn !!!
        //
        __no_operation();

        for(i=0; i < lenhead; i++) {                        // Записуємо НТТР заголовок в модем
            EUSCI_A_UART_transmitData(EUSCI_A0_BASE, TxMbuf.buf[i]);
        }

        for(i=0; i < lendata-2; i++) {                      // Записуємо НТТР дані в модем
            EUSCI_A_UART_transmitData(EUSCI_A0_BASE, RxMbuf.buf[i]);
        }

        delay_ms(10);

        for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
        RxMbuf.ind = 0;
        //
        //  код '26' равносилен Ctrl+Z - команде завершения ввода текста и отправки 0x1A.
        //
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, 0x1A);

        /*
         * Wait for confirmation that SIM800 has actually sent
         * the TCP payload.
         *
         * Do not reset the MSP430 on timeout: OTA receive state
         * is held in RAM and must survive a transient GSM failure.
         */
        time_w1 = 0;
        while(time_w1 < 20000u){
            delay_ms(1);

            if(cmp_str("SEND OK",&RxMbuf.buf[0])){
                RxMbuf.ind = 0;
                index++;                                // !!! порядок !!!
                i2c_SetAddress(100,LINE6); i2c_PutStr("OK 7 ");
                break;
            }

            time_w1++;
        }

        if(time_w1 >= 20000u){
            /*
             * SEND OK was not received.
             *
             * The server may not have received this request, so:
             *  - do not advance OTA state;
             *  - do not reset MSP430;
             *  - tear down the modem IP session;
             *  - retry later with the same OTA ACK.
             */
            for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;
            RxMbuf.ind = 0;

            Send_from_UART0("AT+CIPSHUT\r\n");
            wait_compl("SHUT OK", &RxMbuf.buf[0], 5000);

            P4OUT &= ~BIT4;

            if(ota_get_status() == 2u){
                pack_time = 3u;
            }

            return 0;
        }
        // "SEND OK" - данные успешно переданы.
        // ожидаем состояние CLOSED - закрытие соединения

        time_w1 =0;
        while(time_w1 < 25){  //while(time_w1 < 5000){

            delay_ms(1000);


            result_remote_command =
                remote_cmp_str_limited(
                    "CLOSED\r\n",
                    &RxMbuf.buf[0],
                    RxMbuf.ind);

            if (result_remote_command) {

                /*
                 * Remote payload transport:
                 *
                 * old protocol:
                 *   64 HEX chars = 32 encrypted bytes
                 *                = 2 AES blocks
                 *
                 * extended protocol:
                 *   128 HEX chars = 64 encrypted bytes
                 *                 = 4 AES blocks
                 *
                 * 9 bytes between the end of encrypted payload
                 * and the position returned for CLOSED remain unchanged.
                 */

                if (remote_payload_find_before_closed(
                        result_remote_command,
                        128u,
                        &remote_payload_start))
                {
                    remote_payload_hex_len = 128u;
                }
                else if (remote_payload_find_before_closed(
                             result_remote_command,
                             64u,
                             &remote_payload_start))
                {
                    remote_payload_hex_len = 64u;
                }
                else
                {
                    set_comm_retry_pack_time(421u);
                    index++;
                    time_w1 = 0;
                    break;
                }

                /*
                 * Clear plaintext area so that the short
                 * 32-byte protocol cannot leave stale bytes.
                 */
                for (i = 0; i < 64u; i++) {
                    encrdata[i] = 0;
                }

                /*
                 * Every AES block is represented by
                 * 32 hexadecimal characters.
                 */
                for (aes_block = 0;
                     aes_block < (remote_payload_hex_len / 32u);
                     aes_block++)
                {
                    unsigned int j;

                    for (j = 0; j < 16u; j++)
                    {
                        unsigned int hex_pos =
                            remote_payload_start +
                            aes_block * 32u +
                            j * 2u;

                        uint8_t hi =
                            hex_nibble(RxMbuf.buf[hex_pos]);
                        uint8_t lo =
                            hex_nibble(RxMbuf.buf[hex_pos + 1u]);

                        RxMbuf.buf[j] =
                            (uint8_t)((hi << 4) | lo);
                    }

                    AES256_setDecipherKey(
                        AES256_BASE,
                        CipherKey,
                        AES256_KEYLENGTH_256BIT);

                    AES256_decryptData(
                        AES256_BASE,
                        &RxMbuf.buf[0],
                        DataAESdecrypted);

                    for (j = 0; j < 16u; j++)
                    {
                        encrdata[aes_block * 16u + j] =
                            DataAESdecrypted[j];
                    }
                }

                // Устанавливаем время интервалов между передачами на сервер
                result_remote_command = parse_remote_command();

                /*
                 * OTA multi-chunk:
                 * залишаємо GPRS активним і відразу відкриваємо
                 * наступний TCP-сеанс.
                 *
                 * Перший тест: максимум 4 OTA chunks за один
                 * виклик send_packet().
                 */
                if ((ota_get_status() == 2u) &&
                    ((ota_get_received() - ota_session_start_received) < 512UL))
                {
                    for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;
                    RxMbuf.ind = 0;

                    delay_ms(5000);

                    goto ota_next_tcp;
                }

                i2c_SetAddress(100,LINE6); i2c_PutStr("OK*  ");
                index++;                                // !!! порядок !!!
                time_w1 =0;
                break;
            }
            if(cmp_str("packet error",&RxMbuf.buf[0])){
                set_comm_retry_pack_time(421u);
                //
                // 4 - M_ON - включение модема
                // ОТКлючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;              //
                //
                index++;                                // !!! порядок !!!
                break;
            }
            time_w1++;
        }
    }
    if(time_w1 >= 25){
        /*
         * During OTA a lost HTTP response must not introduce
         * a long normal telemetry delay.
         *
         * Keep the current OTA ACK/received state and retry soon.
         */
        if ((ota_get_status() == 1u) ||
            (ota_get_status() == 2u))
        {
            pack_time = 3u;
        }
        else
        {
            pack_time = DEFAULT_PACK_TIME_TICKS;
        }
    }
    // Коды ошибок:
    // 420 - норма utilites_skz.c строка 1217
    // 421 - "packet error" здесь, строка 571
    // 422 - не определена длина паузы utilites_skz.c строка 1209
    // 423 - нет команды о задержке utilites_skz.c строка 1220
    // 424 - старт системы
    // 425 - переполнение буфера. здесь, строка 571

    return index;
}


void rePWR_sim800c (void)
{
    //
    i2c_SetAddress(100,LINE6); i2c_PutStr("rePWR");
    //
    // отключаем DTR вверх (P2.2)
    //
    P3OUT |= BIT4;                              // P3.4 - DTR
    //
    // снимаем и поднимаем PWRKEY вниз (P3.5)
    //
    P3OUT &= ~BIT5;                             // P3.5 - PWRKEY вниз
    __delay_cycles(100000);                     // ждем... 3 секунды
    P3OUT |= BIT5;                              // P3.5 - PWRKEY вверх
    //
    __delay_cycles(2000000);                    // ждем...
    //
    // снимаем и поднимаем PWRKEY вниз (P3.5)
    //
    P3OUT &= ~BIT5;                             // P3.5 - PWRKEY
    __delay_cycles(100000);                     // ждем... 3 секунды
    P3OUT |= BIT5;                              // P3.5 - PWRKEY вверх
    //
    __delay_cycles(1000000);                    // ждем... 3 секунды
    //
    // включаем DTR вниз (P2.2)
    //
    P3OUT &= ~BIT4;                              // P3.4 - DTR
    //
    __delay_cycles(4000000);  // ждем... 3 секунды
    //
}


//
/********************************************************************/
// Инициализация модема  SIM-800с
// выход 0 - нет сим карты
// выход 1 - успешная инициализация
//
char init_sim800c (void)
{
//unsigned char str[20];
unsigned char op;
unsigned int i;
    //
        P2OUT &= ~BIT7;             // BКЛючаем LED
    //
    i2c_SetAddress(100,LINE6); i2c_PutStr("ini 1");
//    __delay_cycles(2000000);                    // ждем...
    //
    // включаем БЛОК питания модема
    //
    P4OUT |= BIT4;              // 4 - M_ON - включение модема
    //
    P3OUT |= BIT5;              // включаем PWRKEY вверх
    //
    __delay_cycles(1000000);    // ждем... T>0.5s
    //
    P3OUT &= ~BIT5;             // опускаем PWRKEY вниз и ждем > 1 секунды
    //
    __delay_cycles(2000000);    // ждем... T>1.0s
    //
    P3OUT |= BIT5;              // включаем PWRKEY вверх и опять ждем > 2,2 секунды
    //
    __delay_cycles(4000000);    // ждем... > 3 секунды
    //
    P3OUT &= ~BIT4;             // включаем DTR вниз (P2.2)
//
        P2OUT |= BIT7;              // ОТКЛючаем светодиодный индикатор.
    //
    i2c_SetAddress(100,LINE6); i2c_PutStr("ini 2");
    __delay_cycles(1000000);  // ждем... > 2,2 секунды (1с=8,000,000)

    for(i=0; i < 550; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема

    for(i=0; i< 10; i++){  // Цикл синхронизации скорости модема

        RxMbuf.ind = 0;
        Send_from_UART0("AT\r\n");          // Отправляем символ синхронизации

        __delay_cycles(100000);  // ждем... 1с=8,000,000

        if(cmp_str("AT",&RxMbuf.buf[0])){
                break;
        }
    }
    //
    RxMbuf.ind = 0;
    Send_from_UART0("AT+IPR=115200\r\n");  // Устанавливаем скорость 9600 и записываем в ПЗУ
    i2c_SetAddress(100,LINE6); i2c_PutStr("ini 3");
    __delay_cycles(2000000);  // ждем... 1с=8,000,000
    //
    // 4. Настройка на оператора сети        //////////////////////////////////////////
    //
    i2c_SetAddress(100,LINE6); i2c_PutStr("ini 4");
            op = 0;
            while(op < 3){
                for(i=0; i< 500; i++) RxMbuf.buf[i] = 0;
                RxMbuf.ind = 0;
                Send_from_UART0("AT+CSPN?\r\n");               // Отправляем пакет на передаче

                repl = wait_compl ("CSPN: ", &RxMbuf.buf[0], 200);

                if(repl > 0){
                    if(cmp_str("KYIV",&RxMbuf.buf[0])){
                        operat_GSM = '2';
               //         i2c_PutStr("КИЕВСТАР");    // == 3 ==
                        break;
                    }
                    else if(cmp_str("MTS",&RxMbuf.buf[0])){
                        operat_GSM = '1';
               //         i2c_PutStr("МТС     ");    // == 3 ==
                        break;
                    }
                    else if(cmp_str("Vodafone",&RxMbuf.buf[0])){
                        operat_GSM = '6';
               //         i2c_PutStr("Vodafone");    // == 3 ==
                        break;
                    }
                    op++;
                    if(op>=10){
                        if(start_init_job != 0){
                            start_init_job=1;
                        }
                        SW_RESET();  // программный сброс
                    }
                }
                else if(cmp_str("ERROR",&RxMbuf.buf[0])){
                    operat_GSM = 0;
                    //
                    // обработка отсутствия СИМ карты добавлена 08.09.2018
                    // уязвимость - постоянный поиск СИМ приводит к разрядке аккумулятора!
                    //
                    if(display == 1){                               // Если есть дисплей, то:
                        //***********************************-0123456789012345678901
                        i2c_SetAddress(0, LINE8); i2c_PutStr("**** HET SIM-KAPT.****");
                        return 0;
                    }
                    op++;
                }
                else  {
                    op++;
                    if(op>=10){
                        if(start_init_job != 0){
                            start_init_job=1;
                        }
                        SW_RESET();  // программный сброс
                    }
                }
                __delay_cycles(4000000);  // ждем... 1с=8,000,000
            }
            //
            // обработка отсутствия СИМ карты добавлена 08.09.2018
            // уязвимость - постоянный поиск СИМ приводит к разрядке аккумулятора!
            //
            i2c_SetAddress(100,LINE6); i2c_PutStr("ini 5");
            if(op >=10){    // СИМ-карта не найдена.
                pack_time = 424;    // Устанавливаем время ожидания 1 час
                operat_GSM = 0;     // Нет оператора GSM связи.
                try = 0;            // чистим количество попыток
                //
                // 4 - M_ON - включение модема
                // ОТКлючаем БЛОК питания модема
                //
                P4OUT &= ~BIT4;              //
                //
                return 0;   // выходим из инициализации
            }
                __delay_cycles(10000000);  // ждем... чтобы установился сигнал CSQ
            //
            // 7. Проверяем силу сигнала              //////////////////////////////////////////
            //
                i2c_SetAddress(100,LINE6); i2c_PutStr("ini 6");

                for(i=0; i< 500; i++) RxMbuf.buf[i] = 0;
                RxMbuf.ind = 0;
                // Отправляем пакет на передаче
                Send_from_UART0("AT+CSQ\r\n");
                repl = wait_compl ("+CSQ:", &RxMbuf.buf[0], 1000);

                if (!parse_csq_from_rx(repl)) {
                    i2c_SetAddress(100,LINE6); i2c_PutStr("eCSQ ");
                }
                else {
                    i2c_SetAddress(100,LINE6); i2c_PutStr("ini* ");
                }

        return 1;
}
