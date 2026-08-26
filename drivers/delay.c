/*
 *	Delay functions
 *	See delay.h for details
 *
 *	Make sure this code is compiled with full optimization!!!
 */

#include	"delay.h"

void
DelayMs(unsigned int cnt)
{
	unsigned int i;
	while (cnt--) {
		i=4;
		while(i--) {
			DelayUs(uS_CNT);	/* Adjust for error */
		} ;
	} ;
}
/*
********************************************************************************
*               Designate the delay
*
* Описание  : Wait for 10 milliseconds
* Аргументы :  cnt - time to wait
* Returns     : None
* Примечание  : Internal Function
********************************************************************************
*/
void wait_10ms(int cnt)
{
        unsigned int i;

        for (i = 0; i < (cnt*5); i++) wait_1ms(10);
}


/*
********************************************************************************
*               Designate the delay
*
* Описание  : Wait for 1 millisecond
* Аргументы :  cnt - time to wait
* Returns     : None
* Примечание : Internal Function
********************************************************************************
*/
void wait_1ms(int cnt)
{
        unsigned int i;

        for (i = 0; i < cnt*2; i++) wait_1us(1000);
}

/*
********************************************************************************
*               Designate the delay
*
* Описание  : Wait for 1 microsecond
* Аргументы :  cnt - time to wait
* Returns     : None
* Примечание  : Internal Function, System Dependant
********************************************************************************
*/
void wait_1us(int cnt)
{
//unsigned int i;

__no_operation();
__no_operation();
//__no_operation();
	//for (i = 0; i < (cnt); i++) ;
}

/********************************************************************************
*/
void i_wait_1us(int cnt)
{
unsigned int i;

	for (i = 0; i < (cnt*2); i++);
}
/********************************************************************************
*/


