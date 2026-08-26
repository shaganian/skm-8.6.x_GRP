//********************************************************************
//  sim900.c
//  Файл процедур обслуживания модема
//--------------------------------------------------------------------
// Автоматически определяет тип СИМ-карті (МТС или КИЕВСТАР)
// modem_sim900_31.c - зміна алгоритму включення модема. строки 680-700
//--------------------------------------------------------------------
//
#include <stdlib.h>
//#include "driverlib/MSP430FR5xx_6xx/driverlib.h"
#include <driverlib.h>
#include "system.h"
#include "rdx0154.h"              // Утилиты обслуживания LCD и клавиатуры

//#define SW_RESET()      PMMCTL0 = PMMPW + PMMSWBOR + (PMMCTL0 & 0x0003);  // software BOR reset
#define SW_RESET()      rePWR_sim900 ();return 0;  // software BOR reset
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
extern unsigned char encrdata[272];
extern uint8_t Data[16];
extern uint8_t CipherKey[32];
extern uint8_t DataAESencrypted[16];       // Зашифровані дані
extern uint8_t DataAESdecrypted[16];       // Розшифровані дані
//
/*
 * ----------------- Send_from_UART0 ---------------------------------
 */
//void Send_from_UART0 (unsigned char* buffer) {
void Send_from_UART0 (char* buffer) {
U16 i;
unsigned long len;

    GPIO_setOutputHighOnPin (GPIO_PORT_P2, GPIO_PIN7);  // ВКЛючаем светодиод
    len =0;
    while(buffer[len]!=0){
        TxMbuf.buf[len] = buffer[len];
        len++;
    }
    i=0;
    while(buffer[i]!=0){
        // Загружаем данные в буфер
        //
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, buffer[i]);
        i++;
    }
    __delay_cycles(50);  // ждем...// ожидаем завершения передачи пакета

}

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

//
//********************************************************************/
// Инициализация порта UART0
void init_uart0 (void)
{
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
// Процедура передачи пакета данных на сервер
char send_packet (void)
{
unsigned char index,s;
unsigned int lenreq,i;
//
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

    for(s=0; s< 5; s++){
        if((RxMbuf.buf[repl+s+2]==0x0d)||(RxMbuf.buf[repl+s+2]==0x0a)){
            pack.csq[s] = '0';
            break;
        }
        if(RxMbuf.buf[repl+s+2]==','){
            pack.csq[s] = '.';
        }
        pack.csq[s] = RxMbuf.buf[repl+s+2];  // и сохраняем в буфере качества сигнала
    }
    sens.d[7].dat = atol(pack.csq);
    i2c_SetAddress(100,LINE6); i2c_PutStr("OK 1 ");
    //pack.csq[s] = 0;

//    while(try < 5){

        //
        // Подключаем GPRS сервис
        //
        RxMbuf.ind = 0;
        Send_from_UART0("AT+CIPSHUT\r\n");          // ќтправл¤ем пакет на передачу
        __delay_cycles(500000);  // ждем... 1с=8,000,000

        /*
                             Send_from_UART0("AT+CIFSR\r\n");   // берем локальный IP адрес
                             __delay_cycles(1000000);           // ждем... 1с=8,000,000
                             repl = wait_compl (".", &RxMbuf.buf[0], 1500); // Очікуємо ІР адресу, там обов'язково повина бути крапка '.' !

                             for(i=0; i < 12; i++) ipdec[i]=0;
                             for(i=0; i < 12; i++) {
                                 if(RxMbuf.buf[i+repl+1]== 0x0d) break;
                                 ipdec[i] = RxMbuf.buf[i+repl+1];  // чистим буфер приема
                             }
                             __no_operation();

                             IPdecToHex(ipdec,iphex); //Формуємо ключ з ІР-адреси. Він у iphex[]
        // */



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
                    i2c_SetAddress(100,LINE6); i2c_PutStr("OK 2е");
                }
                SW_RESET();  // программный сброс
            }
            //
            // указываем точку входа:
            //
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
                 SW_RESET();  // программный сброс
             }
             //
             // активируем подключение через GPRS
             //
             for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
             RxMbuf.ind = 0;

             Send_from_UART0("AT+CIICR\r\n");       // активируем подключение через GPRS

             repl = wait_compl ("OK", &RxMbuf.buf[0], 1500);

             //if(cmp_str("PDP DEACT",&RxMbuf.buf[0])){
             if(cmp_str("ERROR",&RxMbuf.buf[0])){
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
                 //clear_LCD (0);                      // * очищаем экран
                 //i2c_SetAddress(0, LINE4); i2c_PutStr(" ERROR conn with GPRS "); // Выводим время
                   i2c_SetAddress(100,LINE6); i2c_PutStr("eGPRS");
                 __delay_cycles(10000000);  // ждем... 1с=8,000,000
                 try++;
                 if(try >=3){            // .
                     pack_time = 424;    // Устанавливаем время ожидания 1 час
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
                 SW_RESET();  // программный сброс
                 //
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

//                     Send_from_UART0("AT+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"\r\n");
//                     __delay_cycles(1000000);           // ждем... 1с=8,000,000

                     //Send_from_UART0("AT+CDNSGIP=\"grmu-skydom.grmu.com.ua\"\r\n");
//                     Send_from_UART0("AT+CDNSGIP=?\r\n");
//                     __delay_cycles(1000000);           // ждем... 1с=8,000,000

//*
                     Send_from_UART0("AT+CIFSR\r\n");   // берем локальный IP адрес
                     __delay_cycles(1000000);           // ждем... 1с=8,000,000
                     repl = wait_compl (".", &RxMbuf.buf[0], 1500); // Очікуємо ІР адресу, там обов'язково повина бути крапка '.' !

                     for(i=0; i < 12; i++) ipdec[i]=0;
                     for(i=0; i < 12; i++) {
                         if(RxMbuf.buf[i+repl+1]== 0x0d) break;
                         ipdec[i] = RxMbuf.buf[i+repl+1];  // чистим буфер приема
                     }
                     __no_operation();

                     IPdecToHex(ipdec,iphex); //Формуємо ключ з ІР-адреси. Він у iphex[]
// */
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
                         pack_time = 424;    // Устанавливаем время ожидания 1 час
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
                pack_time = 424;    // Устанавливаем время ожидания 1 час
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
                pack_time = 424;    // Устанавливаем время ожидания 1 час
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
                pack_time = 424;    // Устанавливаем время ожидания 1 час
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
     if(server==0) Send_from_UART0("AT+CIPSTART=\"TCP\",\"www.skydom.info");        // СКАЙДОМ:
else if(server==1) Send_from_UART0("AT+CIPSTART=\"TCP\",\"sky.kgaz.com.ua");        // КРЕМЕНЧУКГАЗ:
//else if(server==1) Send_from_UART0("AT+CIPSTART=\"TCP\",\"193.109.249.254");      // КРЕМЕНЧУКГАЗ:
else if(server==2) Send_from_UART0("AT+CIPSTART=\"TCP\",\"metro.gazbil.ks.ua");     // ХЕРСОНГАЗ:
else if(server==3) Send_from_UART0("AT+CIPSTART=\"TCP\",\"sky.kirgas.com");         // КІРОВОГРАДГАЗ:
else if(server==4) Send_from_UART0("AT+CIPSTART=\"TCP\",\"grmu.skydom.info");       // сервер grmu.skydom.info        СКАЙДОМ
else if(server==5) Send_from_UART0("AT+CIPSTART=\"TCP\",\"grmu-skydom.grmu.com.ua");// сервер ГАЗМЕРЕЖІ УКРАЇНИ
//else if(server==5) Send_from_UART0("AT+CIPSTART=\"TCP\",\"185.2.109.40");// сервер ГАЗМЕРЕЖІ УКРАЇНИ

     if(server==3){
         Send_from_UART0("\",\"8181\"\r\n");  // открываем соединение:
     }
     else if(server==5){
         Send_from_UART0("\",\"61003\"\r\n");  // открываем соединение:
         //Send_from_UART0("\",\"80\"\r\n");  // открываем соединение:
     }
     else{
         Send_from_UART0("\",\"80\"\r\n");  // открываем соединение:
     }
//

    i2c_SetAddress(100,LINE6); i2c_PutStr("OK 6 ");

    __delay_cycles(1000000);  // ждем 3мс, пока разрядится конденсатор
    //--------------------------------------------------------------
    time_w1 =0;
    while(time_w1 < 100000){ //while(time_w1 < 500){//
        if(cmp_str("CONNECT OK",&RxMbuf.buf[0])){
            index++;                                // !!! порядок !!!
            break;                       // переходим к передаче данных
        }
        if(cmp_str("TCP CLOSED",&RxMbuf.buf[0])){
            if(start_init_job != 0){
                start_init_job=1;
            }
            try++;
            if(try >=3){            // .
                pack_time = 424;    // Устанавливаем время ожидания 1 час
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
            SW_RESET();  // программный сброс
            //break;
        }
        if(cmp_str("CONNECT FAIL",&RxMbuf.buf[0])){
            if(start_init_job != 0){
                start_init_job=1;
            }
            try++;
            if(try >=3){            // .
                pack_time = 424;    // Устанавливаем время ожидания 1 час
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
            SW_RESET();  // программный сброс
            //break;
        }
        time_w1++;
    }
    if(time_w1 >= 1000){  //if(time_w1 >= 500){ //
        if(start_init_job != 0){
            start_init_job=1;
        }
        try++;
        if(try >=3){            // .
            pack_time = 424;    // Устанавливаем время ожидания 1 час
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
        SW_RESET();  // программный сброс
    }
    __delay_cycles(100000);  // ждем 3мс, пока разрядится конденсатор
//
// запускаем передачу данных
//
    Send_from_UART0("AT+CIPSEND\r\n");  //

    repl = wait_compl (">", &RxMbuf.buf[0], 500);

    if(repl > 0){
//
//
//---------------------------------------------------------------------------------------------
        for(i=0; i<410; i++){ TxMbuf.buf[i]= 0; }   // Чистимо буфер передачі

        httpreq_smart();                            // формуемо HTTP-запит на передачу даних.
                                                    // 136 байт - заголовок в TxMbuf.buf
                                                    // 274 байт - данні в RxMbuf.buf
        __no_operation();
//
//---------------------------------------------------------------------------------------------
//
//
        __delay_cycles(1000000);  // ждемо... поки модем не вгамується...

        //void aes256(U8 *rawdata, U8 *outdata, U8 index_outdata)
        // rawdata       - Вхідні дані
        // outdata       - Вихідні, кодовані дані
        // index_outdata - Довжина вихідних даних
        //
        //aes256(&encrdata[0], &TxMbuf.buf[0], lenhead);  // Виконуемо кодування пакета в aes256
        aes256(&encrdata[0], &RxMbuf.buf[0], 0);  // Виконуемо кодування пакета в aes256
                                                        // 272 байт - кодовані дані в encrdata
        //
        //ItoDecAShot_bn(lendata - lenhead,  &TxMbuf.buf[dataplace]);// записываем длину данных !!! ItoDecAShot_bn !!!
        ItoDecAShot_bn(lendata,  &TxMbuf.buf[dataplace]);// записываем длину данных !!! ItoDecAShot_bn !!!
        //

//        for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
//        RxMbuf.ind = 0;

        __no_operation();

        for(i=0; i < lenhead; i++) {                    // Записуємо НТТР заголовок в модем
            EUSCI_A_UART_transmitData(EUSCI_A0_BASE, TxMbuf.buf[i]);
        }
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, 'u');
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, '=');
        for(i=0; i < lendata-2; i++) {                    // Записуємо НТТР заголовок в модем
            EUSCI_A_UART_transmitData(EUSCI_A0_BASE, RxMbuf.buf[i]);
        }

        __delay_cycles(10000);  // ждем 3мс, пока разрядится конденсатор

        for(i=0; i < 512; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
        RxMbuf.ind = 0;
        //
        //  код '26' равносилен Ctrl+Z - команде завершения ввода текста и отправки 0x1A.
        //
        //UARTCharPut (UART0_BASE, 0x1A);  // передаем данные через UART0
        EUSCI_A_UART_transmitData(EUSCI_A0_BASE, 0x1A);

        time_w1 =0;
        while(time_w1 < 1500){
            __delay_cycles(10000);  // ждем 3мс, пока разрядится конденсатор
            if(cmp_str("SEND OK",&RxMbuf.buf[0])){

                RxMbuf.ind = 0;
                index++;                                // !!! порядок !!!
                i2c_SetAddress(100,LINE6); i2c_PutStr("OK 7 ");
                break;
            }
            time_w1++;
        }
        if(time_w1 >= 1500){
            if(start_init_job > 0){
                start_init_job=1;
                try++;
                if(try >=3){            // .
                    pack_time = 424;    // Устанавливаем время ожидания 1 час
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
                SW_RESET();  // программный сброс
            }
        }
        // "SEND OK" - данные успешно переданы.
        // ожидаем состояние CLOSED - закрытие соединения

        time_w1 =0;
        while(time_w1 < 25){  //while(time_w1 < 5000){

            __delay_cycles(1000000);  // ждем 3мс...


            if(cmp_str("CLOSED\r\n",&RxMbuf.buf[0])){

                result_remote_command = cmp_str("CLOSED\r\n",&RxMbuf.buf[0]);

// 1 *****************************************************************************************
               for(i=0; i<32; i++){
                    encrdata[i] = RxMbuf.buf[result_remote_command-73+i];
                }
                uint16_t i = 0;
                uint16_t j = 0;

//                hex_to_bytes(RxMbuf.buf[result_remote_command-73], encrdata, 32);
                for (i = 0, j = 0; j < 64; j++, i += 2) {
                    uint8_t hi = hex_nibble(encrdata[i]);
                    uint8_t lo = hex_nibble(encrdata[i+1]);
                    RxMbuf.buf[j] = (uint8_t)((hi << 4) | lo);
                }
            //*
                    // Завантажуємо ключ шифрування в модуль
                    AES256_setDecipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);

                    // Розшифровуємо дані
                    //AES256_decryptData(AES256_BASE, DataAESencrypted, DataAESdecrypted);
                    AES256_decryptData(AES256_BASE, &RxMbuf.buf[0], DataAESdecrypted);

                    for (i=0; i < 16; i++){
                        encrdata[i] = DataAESdecrypted[i];
                        //HTOA(DataAESdecrypted[i], &decrdata[q]); q += 2;
                    }
// 2 *****************************************************************************************

                for(i=0; i<32; i++){
                    encrdata[i+100] = RxMbuf.buf[result_remote_command-73+32+i];
                }
                i = 0;
                j = 0;

//                hex_to_bytes(RxMbuf.buf[result_remote_command-73], encrdata, 32);
                for (i = 0, j = 0; j < 64; j++, i += 2) {
                    uint8_t hi = hex_nibble(encrdata[i+100]);
                    uint8_t lo = hex_nibble(encrdata[i+101]);
                    RxMbuf.buf[j] = (uint8_t)((hi << 4) | lo);
                }
                // Завантажуємо ключ шифрування в модуль
                AES256_setDecipherKey(AES256_BASE, CipherKey, AES256_KEYLENGTH_256BIT);

                // Розшифровуємо дані
                //AES256_decryptData(AES256_BASE, DataAESencrypted, DataAESdecrypted);
                AES256_decryptData(AES256_BASE, &RxMbuf.buf[0], DataAESdecrypted);

                for (i=0; i < 16; i++){
                    encrdata[i+16] = DataAESdecrypted[i];
                    //HTOA(DataAESdecrypted[i], &decrdata[q]); q += 2;
                }
// end *****************************************************************************************

            // */


                // Устанавливаем время интервалов между передачами на сервер
                result_remote_command = parse_remote_command();

                i2c_SetAddress(100,LINE6); i2c_PutStr("OK*  ");
                index++;                                // !!! порядок !!!
                time_w1 =0;
                break;
            }
            if(cmp_str("packet error",&RxMbuf.buf[0])){
                pack_time = 421;
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
        // оставляем частоту передачи 1час
        pack_time  = 5;//pack_time  = 425;
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


void rePWR_sim900 (void)
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
// Инициализация модема  SIM-900
// выход 0 - нет сим карты
// выход 1 - успешная инициализация
//
char init_sim900 (void) 
{
//unsigned char str[20];
unsigned char op,s;
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
  
    for(i=0; i < 700; i++) RxMbuf.buf[i] = 0;  // чистим буфер приема
        
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
//    RxMbuf.ind = 0;
//    Send_from_UART0("AT+CMGD=?\r\n");  // Удаляем все СМС
//    __delay_cycles(2000000);  // ждем... 1с=8,000,000
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
                        //i2c_open();                         // * Открываем порт
                        //init_LCD ();                        // * Инициализируем дисплей
                        //clear_LCD (0);                      // * очищаем экран
                        //***********************************-0123456789012345678901
                        //i2c_SetAddress(0, LINE5); i2c_PutStr("**********************"); // Выводим значени  Р1.4  Р1.5 Р4.0  Р4.1
                        i2c_SetAddress(0, LINE8); i2c_PutStr("**** HET SIM-KAPT.****");
                        //i2c_SetAddress(0, LINE7); i2c_PutStr("**********************"); // Выводим значени  Р1.4  Р1.5 Р4.0  Р4.1
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
                        //break;
                        //return 0;
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

                for(s=0; s< 5; s++){
                    if((RxMbuf.buf[repl+s+2]==0x0d)||(RxMbuf.buf[repl+s+2]==0x0a)){
                        pack.csq[s] = '0';
                        break;
                    }
                    if(RxMbuf.buf[repl+s+2]==','){
                        pack.csq[s] = '.';
                    }
                    pack.csq[s] = RxMbuf.buf[repl+s+2];  // и сохраняем в буфере качества сигнала
                }
                sens.d[7].dat = atol(pack.csq);
                //pack.csq[s] = 0;
                i2c_SetAddress(100,LINE6); i2c_PutStr("ini* ");

        return 1;
}

