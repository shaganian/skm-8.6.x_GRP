//********************************************************************
//  menu.c
//  функции для работы с дисплеем rdx0154
//
#include <driverlib.h>
#include "system.h"
#include "rdx0154.h"        // Утилиты обслуживания LCD и клавиатуры

#define SLAVE_ADDRESS 0x38

//
extern signed char server;                             // флаг Выбор сервера
//
u_char LCDbuffer[8][22];
struct
{
    u_char level_1;     // Первый уровень меню
    u_char level_2;     // Второй уровень
    u_char level_3;     // Третий уровень
} menu;
unsigned char nokeypad;              // наличие клавиатуры: 0-клава есть, 1-нет.
//extern unsigned char display;        // наличие дисплея:     0-дисплей есть, 1-нет.
extern struct skm8_packet pack;      // Пакет данных для передачи на сервер.
extern struct skm8_packet config;      // Пакет данных для передачи на сервер.
extern unsigned long cnters;
extern volatile uint16_t start_indi;
extern volatile uint16_t time_transmit; // Глобальный счетчик - передача данных на сервер
extern volatile uint16_t start_transmit;
extern unsigned char restart;
//#pragma PERSISTENT(start_init_job)
extern unsigned char start_init_job;             // флаг "НАЧАТЬ РАБОТУ"
//extern char eco; // флаг режима работы СКМ8: 1 - экономный режим, 0- Максимальныйный режим
extern struct rxM_buf RxMbuf;                   // буфер модема

//********************************************************************
char keypadread(void)
// Находит кнопку и выходит с кодом кнопки.
{
    char key;
    key = 0;                      // Читаем клавиатуру
    // проверяем наличие дисплея с клавиатурой
    //
    //GPIO_setAsInputPin(GPIO_PORT_PJ, GPIO_PIN0 + GPIO_PIN1 + GPIO_PIN2 + GPIO_PIN3);
    //PJDIR = 0xFF ^ (BIT0 | BIT1 | BIT2 | BIT3);     // Устанавливаем выводы PJ.0,1,2,3 направлением на ввод
    //
    __delay_cycles(100);                            // ждем...
    if (PJIN & 0x01)
    {                        // Если нажата кнопка PJ.0 КН1  [ * - - - ]
        key |= 0x01;                      // Читаем клавиатуру
    }
    if (PJIN & 0x02)
    {                        // Если нажата кнопка PJ.1 КН2  [ - * - - ]
        key |= 0x02;                      // Читаем клавиатуру
    }
    if (P2IN & 0x08)
    {                        // Если нажата кнопка P2.3 КН3  [ - - * - ]
        key |= 0x04;                      // Читаем клавиатуру
    }
    if (P2IN & 0x10)
    {                        // Если нажата кнопка P2.4 КН4  [ - - - * ]
        key |= 0x08;                      // Читаем клавиатуру
    }

    return key; // Читаем порт и выходим...
}
//*************************************************************************/
// Процедура Сканирования Клавиатуры
// при проверке датчиков температуры
// В режиме ПУЛЬТА
//                          012345678901234567890
const char level1_str1[] = "1.Номер объекта      ";         // "1.Номер объекта      ";
//const char level1_str2[] = "2.Режим роботи СКМ-8 ";         // "2.Режим роботи СКМ-8 ";
const char level1_str3[] = "2.Рестарт пристрою   ";         // "3.Рестарт пристрою   ";
const char level1_str4[] = "3.Перевiрка GSM сигн.";         // "4.Перевiрка GSM сигн.";
//const char level1_str5[] = "5.Калибровка датчиков";
//const char level1_str5[] = "5.Вибiр сервера      ";         // "5.Вибiр сервера      ";
const char level1_str_[] = "                     ";

void kbd_process(void)
{
    char knop, i;
    unsigned long timer = 0;
    unsigned int tamr; //, arch;
    unsigned int repl;                // результат сравнения командой cmp_str();

    knop = keypadread();
    //
    // проверяем наличие дисплея и кнопок
    //
    if (knop == 0xFF)
    {
        nokeypad = 1;         // нет дисплея и кнопок
        return;
    }
    else
    {                 // кнопки есть
        knop &= 0x0F;     // обнуляем старшие разряды
        nokeypad = 0;       // есть дисплей и кнопки
    }

    i2c_open();                         // * Открываем порт
    init_LCD();                        // * Инициализируем дисплей
    clear_LCD(0);                      // * очищаем экран

    //                                  012345678901234567890
    //                                  012345678901234567890
    i2c_SetAddress(0,LINE3);i2c_PutStrUtf8("  Для входа в меню  ");
    i2c_SetAddress(0,LINE4);i2c_PutStrUtf8("  отпустите кнопку: ");
    i2c_SetAddress(0,LINE5);i2c_PutStrUtf8("            . О . . ");

    knop = keypadread() & 0x0F;

    if (knop == 0x0D)
    {                 // Если нажата кнопка ..0.
        timer = 0;
        while (knop != 0x0F)
        {            // ее нужно отпустить
            knop = keypadread();
            timer++;
            if (timer > 80000000)
            {
                return; // таймер прерывания цикла
            }
        }
//*
        P2IFG &= ~BIT3;                 // Чистим флаг прерывания P2.3 IFG
        P2IE &= ~BIT3;                 // ЗАПРЕЩАЕМ прерывание от P2.3
        P2IFG &= ~BIT4;                 // Чистим флаг прерывания P2.4 IFG
        P2IE &= ~BIT4;                 // ЗАПРЕЩАЕМ прерывание от P2.4

        //                                  012345678901234567890
        i2c_SetAddress(6, LINE1); i2c_PutStrUtf8("Настройки модуля   ");                     // текст на экран
        i2c_SetAddress(0, LINE2); i2c_PutStrUtf8(level1_str1);
        //i2c_SetAddress(0, LINE3); i2c_PutStrUtf8(level1_str2);
        i2c_SetAddress(0, LINE3); i2c_PutStrUtf8(level1_str3);
        i2c_SetAddress(0, LINE4); i2c_PutStrUtf8(level1_str4);
        //i2c_SetAddress(0, LINE6); i2c_PutStrUtf8(level1_str5);
        i2c_SetAddress(0, LINE5); i2c_PutStrUtf8(level1_str_);
        i2c_SetAddress(0, LINE6); i2c_PutStrUtf8(level1_str_);
        i2c_SetAddress(0, LINE7); i2c_PutStrUtf8(level1_str_);
        i2c_SetAddress(0, LINE8); i2c_PutStrUtf8(level1_str_);
// */
        menu.level_1 = 0;

//--------------------------------------->01234567890123456789
        while (1)
        {
            knop = ~keypadread() & 0x0F;
            if (knop)
            {

                while (keypadread() != 0x0F)
                    ;

                if (knop & 0x02)
                {  // кнопка "ВНИЗ"
                    if (menu.level_1 >= 5)
                    {
                        menu.level_1 = 1;
                    }
                    else
                    {
                        menu.level_1++;
                    }
                }
                if (knop & 0x01)
                {  // кнопка "ВВЕРХ"
                    if (menu.level_1 <= 1)
                    {
                        menu.level_1 = 5;
                    }
                    else
                    {
                        menu.level_1--;
                    }
                }
                if (knop & 0x04)
                {  // кнопка "ВВОД" или переход на следующий уровень меню
                    if (menu.level_1 == 1)
                    {

                        input_4_digit(pack.addr);             // 1.Номер объекта

                    }
                    if (menu.level_1 == 3)
                    {

                        time_transmit = 65500; // Устанавливаем максимально возможное значение счетчика
                        start_transmit = 1; // Устанавливаем флаг "Начать передачу"
                        restart = 1;
                        clear_LCD(0);                  // заливка - чистим экран
                        //                                  012345678901234567890
                        i2c_SetAddress(6, LINE4);
                        i2c_PutStrUtf8(" Рестарт пристрою! ");   // текст на экран
                        __delay_cycles(2000000);  // обязательно подождать!!!

                        P2IFG &= ~BIT3;       // Чистим флаг прерывания P2.3 IFG
                        P2IFG &= ~BIT4;       // Чистим флаг прерывания P2.4 IFG
                        P2IE |= BIT3 | BIT4; // Разрешаем прерывание от P2.3 и P2.4
                        return;

                    }
                    if (menu.level_1 == 4)
                    {
                        //*
                        clear_LCD(0);                      // * очищаем экран
                        tamr = 0;
                        i2c_SetAddress(1, LINE2);
                        i2c_PutStrUtf8("Перевiрка GSM сигналу");    // текст на экран
                        //
                        // Настраиваем UART
                        //
                        i2c_SetAddress(50 + tamr, LINE4);
                        i2c_PutStrUtf8("ждемо.");
                        init_uart0();
                        i2c_SetAddress(50 + tamr, LINE4);
                        i2c_PutStrUtf8("ждемо..");
                        //
                        // инициализируем модем
                        //
                        init_sim800c();
                        i2c_SetAddress(50 + tamr, LINE4);
                        i2c_PutStrUtf8("ждем0...");
                        while (1)
                        {
                            //
                            // Проверяем силу сигнала              //////////////////////////////////////////
                            //
                            //for(i=0; i< 500; i++) RxMbuf.buf[i] = 0;
                            RxMbuf.ind = 0;
                            // Отправляем пакет на передаче
                            Send_from_UART0("AT+CSQ\r\n");
                            repl = wait_compl("+CSQ:", &RxMbuf.buf[0], 1000);

                            if (!parse_csq_from_rx(repl))
                            {
                                for (i = 0; i < 5; i++) pack.csq[i] = '0';
                            }

                            i2c_SetAddress(50 + tamr, LINE4);
                            i2c_PutStr("       ");
                            i2c_SetAddress(50 + tamr, LINE4);
                            i2c_PutStr("c:");
                            i2c_PutMultySimb((const char*) &pack.csq, 5);
                            //
                            __delay_cycles(1000000);           // ждем 1 секунду
                            // сдвигаемо вивід результату на tamr позицій
                            if (tamr < 14)
                                tamr++;
                            else
                                tamr = 0;

                            knop = ~keypadread() & 0x0F;
                            if (knop & 0x08)
                            {      // кнопка "МЕНЮ" или выход на верхний уровень
                                clear_LCD(0);          // заливка - чистим экран
                                while (keypadread() != 0x0F)
                                    ;
                                break;
                            }
                        }

                        // */
                    }
               }
                if (knop & 0x08)
                {  // кнопка "МЕНЮ" или выход на верхний уровень
                    clear_LCD(0);                      // заливка - чистим экран
                    //
                    // предустанавливаем конфигурацию:
                    //
                    //for (i = 5; i > 0; i--)
                    for(i=0; i<5; i++)
                    {
                        pack.addr[i] = config.addr[i];
                    }  // char addr[4];   // Адрес устройства в системе - "19999"
                    // for(i=0; i<9; i++){pack.cnt1[i] = config.cnt1[i];}  // char cnt1[9];   // счетчик импульсов - "00054321"
                    // for(i=0; i<4; i++){pack.type[i] = config.type[i];}  // char type[4];   // тип устройства - идентификатор "0008"
                    //for(i=0; i<5; i++)
                    for (i = 0; i < 6; i++)
                    {
                        pack.time[i] = config.time[i];
                    } // char time[6];   //Период между передачами на сервер - "43200"

                    start_indi = 1;     // Устанавливаем флаг "Начать индикацию"

                    P2IFG &= ~BIT3;           // Чистим флаг прерывания P2.3 IFG
                    P2IFG &= ~BIT4;           // Чистим флаг прерывания P2.4 IFG
                    P2IE |= BIT3 | BIT4;  // Разрешаем прерывание от P2.3 и P2.4
                    return;
                }

                i2c_SetAddress(0, LINE2);  // Первая строка меню
                if (menu.level_1 == 1){i2c_PutStrUtf8_inv(level1_str1);}
                else{i2c_PutStrUtf8(level1_str1);}
/*
                i2c_SetAddress(0, LINE3);  // Вторая строка меню
                if (menu.level_1 == 2){i2c_PutStrUtf8_inv(level1_str2);}
                else{i2c_PutStrUtf8(level1_str2);}
*/
                i2c_SetAddress(0, LINE3);  // Вторая строка меню
                if (menu.level_1 == 3){i2c_PutStrUtf8_inv(level1_str3);}
                else{i2c_PutStrUtf8(level1_str3);}

                i2c_SetAddress(0, LINE4);  // Вторая строка меню
                if (menu.level_1 == 4){i2c_PutStrUtf8_inv(level1_str4);}
                else{i2c_PutStrUtf8(level1_str4);}
/*
                i2c_SetAddress(0,LINE6);  // Вторая строка меню
                if(menu.level_1 == 5){i2c_PutStrUtf8_inv(level1_str5);}
                else{i2c_PutStrUtf8(level1_str5);
                }
*/
                i2c_SetAddress(0, LINE5); i2c_PutStrUtf8(level1_str_);
                i2c_SetAddress(0, LINE6); i2c_PutStrUtf8(level1_str_);
                i2c_SetAddress(0, LINE7); i2c_PutStrUtf8(level1_str_);
                i2c_SetAddress(0, LINE8); i2c_PutStrUtf8(level1_str_);
            }
            __delay_cycles(8000);  // обязательно подождать!!!
        }

    }
}
/*
 ********************************************************************************
 *    Функция чистит буфер памяти LCD
 * Описание   :
 * Аргументы  : row    - номер строки дисплея(от 1 до 4)
 *            : col    - номер колонки в строке(от 1 до 20)
 *            : string - строка текста
 * Returns    : нет
 * Примечание :
 ********************************************************************************
 */
void LCDcls(void)
{
    unsigned char i, j;
    //for(i=0;i<8;i++)
    for (i = 8; i > 0; i--)
    {
        //for(j=0;j<22;j++)
        for (j = 22; j > 0; j--)
            LCDbuffer[i][j] = ' ';
        LCDbuffer[i][21] = 0;
    }
}

//*************************************************************************/
// Процедура Настройки номера объекта
//
const char level21_str1[] = "  1.Номер объекта    ";
const char level21_str2[] = " Значение 00000-99999";
const char level21_str4[] = "                     ";

void input_4_digit(char *fourdigit)
{
    unsigned char knop = 0;
    unsigned int mark = 0, i;

    init_LCD();                                // Инициализируем дисплей RDX0154
    clear_LCD(0);                               // заливка - чистим экран
    LCDcls();                                   // чистим буфер LCD
    i2c_SetAddress(0, LINE2);
    i2c_PutStrUtf8(level21_str1);
    i2c_SetAddress(0, LINE3);
    i2c_PutStrUtf8(level21_str2);
    i2c_SetAddress(42, LINE5);
    i2c_PutStr_inv("u=");

    //i2c_PutStr_inv((const char *)&LCDbuffer[3][8]);
    i2c_writeChar_inv(config.addr[0]);
    i2c_writeChar_inv(config.addr[1]);
    i2c_writeChar_inv(config.addr[2]);
    i2c_writeChar_inv(config.addr[3]);
    i2c_writeChar_inv(config.addr[4]);

    i2c_SetAddress(57, LINE6);
    i2c_writeChar('[');  // подробности в файле Font5x7.h, строки 99-101
    /*
     Символы размещаются в буфере LCDbuffer[3][9] следующим образом:
     ячейки     [3][9] [3][10] [3][11] [3][12]
     цифры    ст.  1       2       3       4   мл.
     маркер        =                           // mark сначала становится около старшего разряда
     */

    //menu.level_2 = 0;
//--------------------------------------->01234567890123456789
    while (1)
    {
        knop = ~keypadread() & 0x0F;
        if (knop)
        {
            while (keypadread() != 0x0F)
                ;

            if (knop & 0x01)
            {  // кнопка "ВВЕРХ"

                config.addr[0 + mark]++; // увеличиваем число в текущем разряде
                if (config.addr[0 + mark] > '9')
                {
                    config.addr[0 + mark] = '0'; // следим за десятичной системой
                }
                i2c_SetAddress((56 + (mark * 6)), LINE5);
                i2c_writeChar(config.addr[0 + mark]); // подробности в файле Font5x7.h, строки 99-101
            }
            if (knop & 0x02)
            {  // кнопка "ВНИЗ"

                config.addr[0 + mark]--; // увеличиваем число в текущем разряде
                if (config.addr[0 + mark] < '0')
                {
                    config.addr[0 + mark] = '9'; // следим за десятичной системой
                }
                i2c_SetAddress((56 + (mark * 6)), LINE5);
                i2c_writeChar(config.addr[0 + mark]); // подробности в файле Font5x7.h, строки 99-101
            }
            if (knop & 0x04)
            {  // кнопка "ВВОД" или переход на следующий уровень меню

                mark++;
                i2c_SetAddress(0, LINE6);
                i2c_PutStrUtf8(level21_str4);  // чистим строку
                if (mark == 5)
                {
                    mark = 0;
                }
                i2c_SetAddress((57 + (mark * 6)), LINE6);
                i2c_writeChar('['); // подробности в файле Font5x7.h, строки 99-101
            }
            if (knop & 0x08)
            {  // кнопка "МЕНЮ" или выход на верхний уровень

                //for (i = 5; i > 0; i--)
                for(i=0; i<5; i++)
                {
                    pack.addr[i] = config.addr[i];
                }
                return;
            }
        }
        __delay_cycles(15000);   // обязательно подождать!!!
    }
}
//*************************************************************************/
// Процедура Установки значения счетчика
//
//const char level31_str1[] = " 1.Значение счетчика ";
//const char level31_str2[] = "   00000000-99999999 ";  // "   00000000-99999999 "
